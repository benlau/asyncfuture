#ifndef ASYNCFUTUREUTILS_H
#define ASYNCFUTUREUTILS_H

#include <asyncfuture.h>
#include <QTimer>

// Not part of the AsyncFuture, but user may c&p the code to their program

namespace AsyncFutureUtils {


    template <typename Functor>
    inline void runOnMainThread(Functor func) {
        QObject tmp;
        QObject::connect(&tmp, &QObject::destroyed, QCoreApplication::instance(), func, Qt::QueuedConnection);
    }

    /// Returns a QFuture<void> which will be completed after msec specified by value.
    inline
    QFuture<void> timeout(int value) {
       auto defer = AsyncFuture::deferred<void>();

       runOnMainThread([=]() {
           QTimer::singleShot(value, [=]() mutable {
               auto d = defer;
               d.complete();
           });
       });

       return defer.future();
    }

}

#endif // ASYNCFUTUREUTILS_H
