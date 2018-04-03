#include <QQmlApplicationEngine>
#include <QTest>
#include <Automator>
#include <QtConcurrent>
#include <asyncfuture.h>
#include <QTimer>
#include "example.h"
#include "testfunctions.h"
#include "tools.h"

using namespace AsyncFuture;
using namespace Tools;
using namespace Test;

static QString delayedFakeRead(const QString& fileName) {
    Q_UNUSED(fileName);

    Automator::wait(100);
    return "";
}

Example::Example(QObject *parent) : QObject(parent)
{

    // This function do nothing but could make Qt Creator Autotests plugin recognize this test
    auto ref =[=]() {
        QTest::qExec(this, 0, 0);
    };
    Q_UNUSED(ref);

}

/*
 * 1. Convert a signal from QObject into a QFuture object
 * https://github.com/benlau/asyncfuture
 *
 */

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

/*
 *
 * 2. Combine multiple futures with different type into a single future object
 * https://github.com/benlau/asyncfuture
 *
 */

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

    QFuture<QImage> result = (combine() << f1 << f2).subscribe([=](){
        // Read an image but do not return before timeout
        return f1.result();
    }).future();

    timer->start();

    QVERIFY(waitUntil(result, 1000));

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


void Example::example_promise_like_timeout()
{
    QTimer* timer = new QTimer(this);
    timer->setInterval(50);

    auto readFileworker = [](QString fileName) -> QString {
        Q_UNUSED(fileName);
        Automator::wait(1000);
        return "";
    };

    auto read = [=](const QString &fileName) {
        timer->start();

        auto timeout = observe(timer, &QTimer::timeout).future();

        auto defer = deferred<QString>();

        defer.complete(QtConcurrent::run(readFileworker, fileName));
        defer.cancel(timeout);

        return defer.future();
    };

    auto future = read("input.txt");

    bool canceldCalled = false;

    observe(future).subscribe([](QString content) {
        Q_UNUSED(content);
        // onCompleted
    },[&]() {
        // onCanceled due to timeout
        canceldCalled = true;
    });

    waitUntil(future);
    QCOMPARE(canceldCalled, true);
    timer->deleteLater();
    Automator::wait(500); // wait until the read is finished
}

void Example::example_fileactor()
{
    /* FileActor- read data from a file.
     - If the cache is available, return a QFuture with the content
     - If cache is missed, check do it have any worker thread is ready, return the future of the threadd
     - If no thread is found, start a new one
     */

    class FileActor : public QObject {
    public:

        QFuture<QString> read(QString fileName) {
            QFuture<QString> future;

            if (cache.contains(fileName)) {
                // Cache hit. But it still return a QFuture [finished].

                auto defer = deferred<QString>();
                defer.complete(*cache.object(fileName));
                future = defer.future();

            } else if (workers.contains(fileName)) {

                // If any worker are reading, just return the future
                future = workers[fileName];

            } else {

                class Session {
                public:
                    QString fileName;
                    QString content;
                };

                auto loader = [=](QString fileName){
                    Automator::wait(50);

                    Session session;
                    session.content = fileName + "+data";
                    session.fileName = fileName;

                    return session;
                };

                QFuture<Session> worker = QtConcurrent::run(loader, fileName);
                auto observer = observe(worker).context(this, [=](Session session) {

                    workers.remove(session.fileName);
                    cache.insert(fileName, new QString(session.content));
                    return session.content;
                });

                future = observer.future();
                workers[fileName] = future;
            }

            return future;
        }

        QCache<QString, QString> cache;
        QMap<QString, QFuture<QString> > workers;
    };


    FileActor actor;
    QString fileName = "input.txt";

    QFuture<QString> future = actor.read(fileName);

    QCOMPARE(future.isFinished(), false);
    QVERIFY(actor.workers.contains(fileName));

    QVERIFY(waitUntil(future, 1000));

    QCOMPARE(future.isFinished(), true);

    QVERIFY(future.result() == "input.txt+data");

    QVERIFY(!actor.workers.contains(fileName));

    // Read the content again
    future = actor.read(fileName);

    QVERIFY(future.isFinished());
    QVERIFY(future.result() == "input.txt+data");

    QString content;

    observe(future).subscribe([&](QString result) {
        content = result;
    });

    tick();

    QVERIFY(future.result() == content);

}

/* QtConcurrent::mapped doesn't support lambda function. This example show how AsyncFuture could make
 * a mapped that support lambda
 */
void Example::example_mapped_with_lambda()
{

    qDebug() << "Case 1 - Normal Usage";
    { // Case 1 : Normal usage
        auto worker = [=](int value) {
            Automator::wait(10);
            return value * value;
        };

        QList<int> input, expected;
        for (int i = 0; i < 20; i++) {
            input << i;
            expected << i * i;
        }

        QFuture<int> future = Tools::mapped<int>(input, worker);

        QCOMPARE(future.progressMaximum(), 20);

        Test::waitUntil(future);

        QCOMPARE(future.isFinished(), true);

        QList<int> result = future.results();

        QVERIFY(result == expected);

        QCOMPARE(future.progressValue(), 20);
        QCOMPARE(future.progressMaximum(), 20);
    }

    qDebug() << "Case 2 - Cancalation";

    {
        QSemaphore semaphore(1);
        semaphore.acquire(1);
        QMutex mutex;

        // Case 2: Cancellation
        int threadCount = QThread::idealThreadCount();
        int workerCount = 0;
        int finishedCount = 0;

        auto worker = [&](int value) {
            mutex.lock();
            workerCount++;
            mutex.unlock();

            semaphore.acquire();
            Automator::wait(10);
            semaphore.release();

            mutex.lock();
            finishedCount++;
            mutex.unlock();

            return value * value;
        };

        QList<int> input;
        for (int i = 0; i < threadCount * 2; i++) {
            input << i;
        }

        QFuture<int> future = Tools::mapped<int>(input, worker);

        QCOMPARE(future.progressValue(), 0);
        QCOMPARE(future.progressMaximum(), threadCount * 2);

        QTRY_COMPARE(workerCount, threadCount);
        // no one could acquire the lock.
        QCOMPARE(finishedCount, 0);

        future.cancel();
        semaphore.release();

        Automator::wait(500);

        QTRY_COMPARE(finishedCount, threadCount);
        QCOMPARE(workerCount, threadCount);

        /// Progress value can't be changed after canceled.
        QCOMPARE(future.progressValue(), 0);
        QCOMPARE(future.progressMaximum(), threadCount * 2);
    }
}

template <typename Functor, typename Token>
auto cancellationWrapper(Functor functor, Token token) -> std::function<QFuture<void>()> {

    return [=]() mutable -> QFuture<void> {
        if (token.isCanceled()) {
            return token;
        }

        functor();
        auto defer = deferred<void>();
        defer.complete();
        return defer.future();
    };
}

void Example::example_CancellationToken()
{
    //@TODO - It is over complicated
    Deferred<void> cancellation = deferred<void>();

    QFuture<void> cancellationToken = cancellation.future();

    Deferred<void> start;

    QList<qreal> sequence;
    QList<qreal> expectedSquence;
    expectedSquence << 1 << 1.5;

    auto f1 = start.future();

    auto f2 = observe(f1).subscribe(cancellationWrapper([&]() {
        sequence << 1;
    }, cancellationToken)).future();

    observe(f2).subscribe([&]() {
        sequence << 1.5;
        cancellation.cancel();
    });

    auto f3 = observe(f2).subscribe(cancellationWrapper([&]() mutable {
        sequence << 2;
    },cancellationToken)).future();

    observe(f3).subscribe(cancellationWrapper([&]() {
        sequence << 3;
    }, cancellationToken)).future();

    start.complete();
    Automator::wait(500);

    QCOMPARE(sequence, expectedSquence);

}

void Example::example_Combinator_timeout()
{
    QFuture<QString> f1 = QtConcurrent::run(delayedFakeRead, QString("input1.txt"));

    QFuture<QString> f2 = QtConcurrent::run(delayedFakeRead, QString("input2.txt"));

    auto combinator = AsyncFuture::combine();
    combinator << f1 << f2;

    auto defer = deferred<void>();

    defer.complete(combinator.future());
    defer.cancel(Tools::timeout(10));

    auto returningFuture = defer.future();

    await(returningFuture);

    QCOMPARE(returningFuture.isCanceled(), true);
    QCOMPARE(returningFuture.isFinished(), true);

    QCOMPARE(returningFuture.progressMaximum(), 2);
}

