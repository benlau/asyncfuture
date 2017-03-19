#include <QQmlApplicationEngine>
#include <QTest>
#include <Automator>
#include <QtConcurrent>
#include <asyncfuture.h>
#include <QTimer>
#include "example.h"
#include "testfunctions.h"

using namespace AsyncFuture;
using namespace Test;

Example::Example(QObject *parent) : QObject(parent)
{

    // This function do nothing but could make Qt Creator Autotests plugin recognize QuickTests
    auto ref =[=]() {
        QTest::qExec(this, 0, 0);
    };
    Q_UNUSED(ref);

}

void Example::example_Timer_timeout()
{
    QTimer *timer = new QTimer(this);
    timer->setInterval(50);

    // Convert a signal from QObject into QFuture
    QFuture<void> future = observe(timer,
                                   &QTimer::timeout).future();

    // Listen from the future without using QFutureWatcher<T>
    observe(future).subscribe([]() {
        // onCompleted. It is invoked when the observed future is finished successfully
        qDebug() << "onCompleted";
    },[]() {
        // onCanceled
        qDebug() << "onCancel";
    });

    // It is chainable. Listen from a timeout signal only once

    observe(timer, &QTimer::timeout).subscribe([=]() { /*…*/ });

    timer->start();
    waitUntil(future);
}

void Example::example_combine_multiple_future()
{
    auto readImage = [](QString) -> QImage {
        // Not a real image reader
        Automator::wait(50);
        return QImage();
    };

    QTimer *timer = new QTimer(this);
    timer->setInterval(80);

    // Combine multiple futures with different type into a single future
    QFuture<QImage> f1 = QtConcurrent::run(readImage, QString("image.jpg"));

    QFuture<void> f2 = observe(timer, &QTimer::timeout).future();

    (combine() << f1 << f2).subscribe([](QVariantList result){
        // result[0] = QImage
        qDebug() << result;
    });

    timer->start();

    waitUntil(f1);
    waitUntil(f2);

    timer->deleteLater();
}

void Example::example_worker_context()
{

    auto readImage = [](QString) -> QImage {
        // Not a real image reader
        Automator::wait(50);
        return QImage();
    };

    auto validator = [](QImage image) {
        return image.isNull();
    };

    QObject* contextObject = new QObject(this);

    // Start a thread and process its result in main thread

    QFuture<QImage> reading = QtConcurrent::run(readImage, QString("image.jpg"));

    QFuture<bool> validating = observe(reading).context(contextObject, validator).future();

    // Read image by a thread, when it is ready, run the validator function
    // in the thread of the contextObject(e.g main thread)
    // And it return another QFuture to represent the final result.


    QVERIFY(waitUntil(validating, 1000));

    QCOMPARE(validating.result(), true);

}

static int mapFunc(int value) {
    return value * value;
}

void Example::example_context_return_future()
{

    auto reducerFunc = [](QList<int> input ) -> int {
        return input.size();
    };

    QList<int> input;
    for (int i = 0 ; i < 10;i++) {
        input << i;
    }

    QObject* contextObject = this;

    QFuture<int> f1 = QtConcurrent::mapped(input, mapFunc);

    // You may use QFuture as the input argument of your callback function
    // It will be set to the observed future object. So that you may obtain
    // the value of results()
    QFuture<int> f2 = observe(f1).context(contextObject, [=](QFuture<int> future) {
        qDebug() << future.results();

        // Return another QFuture is possible.
        return QtConcurrent::run(reducerFunc, future.results());
    }).future();

    // f2 is constructed before the QtConcurrent::run statement
    // But its value is equal to the result of reducerFunc

    waitUntil(f2);

    QCOMPARE(f2.result(), 10);
}

void Example::example_promise_like()
{
    // Complete / cancel a future on your own choice
    auto d = deferred<bool>();

    observe(d.future()).subscribe([]() {
        qDebug() << "onCompleted";
    }, []() {
        qDebug() << "onCancel";
    });

    d.complete(true);
    d.cancel();

    QCOMPARE(d.future().isFinished(), true);
    QCOMPARE(d.future().isCanceled(), false);

    QVERIFY(waitUntil(d.future(), 1000));
}

void Example::example_promise_like_complete_future()
{
    // Complete / cancel a future according to another future object.
    auto timeout = []() -> void {
        Automator::wait(50);
    };

    auto d = deferred<void>();

    d.complete(QtConcurrent::run(timeout));

    QCOMPARE(d.future().isFinished(), false);
    QCOMPARE(d.future().isCanceled(), false);

    QVERIFY(waitUntil(d.future(), 1000));
}

