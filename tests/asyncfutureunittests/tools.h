#pragma once

#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent>
#include <QNetworkReply>
#include <asyncfuture.h>

// Not part of the AsyncFuture, but user may c&p the code to their program

namespace Tools {

    template <typename Functor>
    inline void runOnMainThread(Functor func) {
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)

        QObject tmp;
        QObject::connect(&tmp, &QObject::destroyed, QCoreApplication::instance(), func, Qt::QueuedConnection);
#else
        QMetaObject::invokeMethod(QCoreApplication::instance(), func, Qt::QueuedConnection);
#endif
    }

    /// Returns a QFuture<void> which will be completed after msec specified by value.
    /// This version works on non-main trehad
    inline
    QFuture<void> timeout(int value) {
       auto defer = AsyncFuture::deferred<void>();

       runOnMainThread([=]() {
           // Don't run QTimer::singleShot on non-main thread.
           // It may not trigger the callback if the
           // event loop of the sender thread is not
           // running.
           QTimer::singleShot(value, [=]() mutable {
               auto d = defer;
               d.complete();
           });
       });

       return defer.future();
    }

    template <typename T, typename Sequence, typename Functor>
    QFuture<T> mapped(Sequence input, Functor func){
        class Context {
        public:
            QMutex mutex;
            Sequence input;
            QVector<T> output;
            int finishedCount;
            int index;
            std::function<void(int)> worker;
        };
        QThreadPool *pool = QThreadPool::globalInstance();

        auto defer = AsyncFuture::deferred<T>();

        /// Allow to be canceled by user
        defer.onCanceled(defer);

        int insertCount = qMin(pool->maxThreadCount(),input.size());

        QSharedPointer<Context> context(new Context());
        context->input = input;
        context->index = insertCount;
        context->finishedCount = 0;
        context->output = QVector<T>(input.size());

        context->worker = [defer, pool, context, func](int pos) mutable {
            if (defer.future().isCanceled()) {
                context->mutex.lock();
                context->worker = nullptr;
                context->mutex.unlock();
                return;
            }

            context->mutex.lock();
            auto value = context->input[pos];
            context->mutex.unlock();

            T res = func(value);

            context->mutex.lock();
            context->output[pos] = res;

            if (!defer.future().isCanceled()) {
                int index = context->index ;
                if (index < context->input.size()) {
                    auto worker = context->worker;
                    QtConcurrent::run(pool, worker, index);
                    context->index++;
                }

                context->finishedCount++;
                defer.setProgressValue(context->finishedCount);

                if (context->finishedCount >= context->input.size()) {
                    defer.complete(context->output.toList());
                    context->worker = nullptr;
                }
            } else {
                // Make sure it could release the ref to context by worker in cancellation
                context->worker = nullptr;
            }
            context->mutex.unlock();
        };

        defer.setProgressRange(0, input.size());

        for (int i = 0 ; i < insertCount ; i++) {
            QtConcurrent::run(pool, context->worker, i);
        }

        return defer.future();
    }
}

