#include <QQmlApplicationEngine>
#include <QTest>
#include <Automator>
#include <QtConcurrent>
#include <asyncfuture.h>
#include <QTimer>
#include "example.h"
#include "testfunctions.h"
#include "asyncfutureutils.h"

using namespace AsyncFuture;
using namespace AsyncFutureUtils;
using namespace Test;

Example::Example(QObject *parent) : QObject(parent)
{

    // This function do nothing but could make Qt Creator Autotests plugin recognize this test
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

template <typename T, typename Sequence, typename Functor>
QFuture<T> mapped(Sequence input, Functor func){
    auto defer = AsyncFuture::deferred<T>();

    QList<QFuture<T>> futures;
    auto combinator = AsyncFuture::combine();

    for (int i = 0 ; i < input.size() ; i++) {
        auto future = QtConcurrent::run(func, input[i]);
        combinator << future;
        futures << future;
    }

    AsyncFuture::observe(combinator.future()).subscribe([=]() mutable {
        QList<T> res;
        for (int i = 0 ; i < futures.size(); i++) {
            res << futures[i].result();
        }
        defer.complete(res);
    });

    return defer.future();
}

/// To simulate a QtConcurrent::mapped
void Example::example_simulate_qtconcurrent_mapped()
{
    auto worker = [=](int value) {
        Automator::wait(10);
        return value * value;
    };

    QList<int> input, expected;
    for (int i = 0; i < 3; i++) {
        input << i;
        expected << i * i;
    }

    QFuture<int> future = mapped<int>(input, worker);

    Test::waitUntil(future);

    QCOMPARE(future.isFinished(), true);

    QList<int> result = future.results();

    QVERIFY(result == expected);
}

static QMutex semaphoreWorkerMutex;
static int semaphoreWorkerRunningCount = 0;
static int semaphoreWorkerFinishedCount = 0;
static QSemaphore internalSemaphore(1);

static int semaphoreWorker(int value) {
    semaphoreWorkerMutex.lock();
    semaphoreWorkerRunningCount++;
    semaphoreWorkerMutex.unlock();
    internalSemaphore.acquire();
    semaphoreWorkerMutex.lock();
    semaphoreWorkerFinishedCount++;
    semaphoreWorkerMutex.unlock();
    internalSemaphore.release();
    return value;
}

void Example::example_qtconcurrent_mapped_cancel()
{
    //@TODO - It need an improved cancellation token mechanism

    semaphoreWorkerRunningCount = 0;
    semaphoreWorkerFinishedCount = 0;
    internalSemaphore.acquire(1);

    int count = QThreadPool::globalInstance()->maxThreadCount() * 2;

    // The input size should be double than the no. of thread available in QThreadPool
    QList<int> input;

    for (int i = 0 ;i < count;i++) {
        input << i;
    }

    auto cancelToken = deferred<void>();

    auto f1 = timeout(100);

    auto f2 = observe(f1).subscribe([&](){
        auto future = QtConcurrent::mapped(input, semaphoreWorker);
        cancelToken.subscribe([](){}, [=]() {
            auto f = future;
            f.cancel();
        });

        return future;
    }).future();

    observe(f2).subscribe([](){}, [=]() {
        auto d = cancelToken;
        d.cancel();
    });

    await(f1);

    // Wait until all the thread are blocked;
    // QtConcurrent::mapped coult not take QThreadPool as input argument.
    Automator::wait(1000);

    QVERIFY(semaphoreWorkerRunningCount > 0);
    QCOMPARE(semaphoreWorkerFinishedCount, 0);
    int runningCount = semaphoreWorkerRunningCount;

    // Cancel f2. No more worker should be executed
    f2.cancel();

    Automator::wait(100);

    internalSemaphore.release();

    QTRY_COMPARE(semaphoreWorkerFinishedCount, runningCount);
    Automator::wait(100);

    QCOMPARE(semaphoreWorkerRunningCount, runningCount);
    QCOMPARE(semaphoreWorkerFinishedCount, runningCount);

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

