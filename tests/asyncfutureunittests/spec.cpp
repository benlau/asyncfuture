#include <QtConcurrent>
#include <QTest>
#include <QFuture>
#include <QFutureWatcher>
#include <Automator>
#include <QFutureWatcher>
#include "trackingdata.h"
#include "testfunctions.h"
#include "asyncfuture.h"
#include "spec.h"
#include "tools.h"

using namespace AsyncFuture;
using namespace Tools;
using namespace Test;

template <typename T>
QFuture<T> finishedFuture(T value) {

    auto o = deferred<T>();

    o.complete(value);

    return o.future();
}

static int mapFunc(int value) {
    return value * value;
}

Spec::Spec(QObject *parent) : QObject(parent)
{
    auto ref = [=]() {
        QTest::qExec(this, 0, 0); // Autotest detect available test cases of a QObject by looking for "QTest::qExec" in source code
    };
    Q_UNUSED(ref);
}

void Spec::initTestCase()
{
    {
        QCOMPARE(TrackingData::aliveCount(), 0);

        TrackingData d1;
        Q_UNUSED(d1);
        QCOMPARE(TrackingData::aliveCount(), 1);

        TrackingData d2;
        Q_UNUSED(d2);
        QCOMPARE(TrackingData::aliveCount(), 2);
    }

    QCOMPARE(TrackingData::aliveCount(), 0);

}

void Spec::cleanup()
{
    QCOMPARE(TrackingData::aliveCount(), 0);
}

static int square(int value) {
    Automator::wait(50);

    return value * value;
}

void Spec::test_QFuture_cancel()
{
    QList<int> input;
    for (int i = 0 ; i < 100;i++) {
        input << i;
    }

    QFuture<int> future = QtConcurrent::mapped(input, square);

    QCOMPARE(future.isRunning(), true);
    QCOMPARE(future.isFinished(), false);
    QCOMPARE(future.isCanceled(), false);

    future.cancel();

    QVERIFY(waitUntil([&](){
        return !future.isRunning();
    }, 1000));

    QCOMPARE(future.isRunning(), false);
    QCOMPARE(future.isFinished(), true);
    QCOMPARE(future.isCanceled(), true);
}

void Spec::test_QFuture_isResultReadyAt()
{
    auto defer = deferred<int>();
    auto future = defer.future();

    QVERIFY(!future.isResultReadyAt(0));
    defer.complete(10);
    QVERIFY(future.isResultReadyAt(0));
    QVERIFY(!future.isResultReadyAt(1));
    QVERIFY(future.results().size() == 1);
    QVERIFY(future.result() == 10);
    QVERIFY(future.results()[0] == 10);

}

void Spec::test_QFutureWatcher_in_thread()
{
    // It prove to use QFutureWatcher in a thread do not works if QEventLoop is not used.

    {
        bool called = false;
        QFutureWatcher<void>* watcher = 0;
        QFuture<void> future;

        auto worker = [&]() {
             watcher = new QFutureWatcher<void>();
             future = timeout(50);
             QObject::connect(watcher, &QFutureWatcher<void>::finished, [&]() {
                 called = true;
             });
             watcher->setFuture(future);
        };

        auto f = QtConcurrent::run(worker);

        QVERIFY(waitUntil(f,1000));

        waitUntil([&]() {
            return called;
        }, 1000);

        QVERIFY(future.isFinished());
        QVERIFY(called == false); // It is not called as the thread is terminated
        delete watcher;
    }

    {
        // Work around solution: Create QFutureWatcher on main thread.

        bool called = false;
        QFutureWatcher<void>* watcher = 0;
        QFuture<void> future;

        auto worker = [&]() {

             future = QFuture<void>();

             {
                 QObject obj;
                 QObject::connect(&obj, &QObject::destroyed,
                                  QCoreApplication::instance(),
                                  [&]() {
                     watcher = new QFutureWatcher<void>();
                     QObject::connect(watcher, &QFutureWatcher<void>::finished, [&]() {
                         called = true;
                     });
                     watcher->setFuture(future);
                 });
             }
        };

        auto f = QtConcurrent::run(worker);

        QVERIFY(waitUntil(f,1000));

        QVERIFY(waitUntil([&]() {
            return watcher != 0;
        }, 1000));

        waitUntil([&]() {
            return called;
        }, 1000);

        QVERIFY(future.isFinished());
        QVERIFY(called == true);
        delete watcher;
    }
}

void Spec::test_QtConcurrent_exception()
{
    auto future = QtConcurrent::run([]() {
        Automator::wait(100);
        throw QException();
        return 99;
    });

    QCOMPARE(future.isFinished(), false);
    QCOMPARE(future.isCanceled(), false);

    await(future);

    QCOMPARE(future.isFinished(), true);
    QCOMPARE(future.isCanceled(), true);
    QCOMPARE(future.isResultReadyAt(0) , false);
}

#if QT_VERSION > QT_VERSION_CHECK(5, 7, 1)
void Spec::test_QtConcurrent_map()
{
    QFuture<void> future;
    QFutureWatcher<void> watcher;

    QList<int> input;
    input << 0 << 1 << 2;

    future = QtConcurrent::map(input, square);
    bool started = false;
    bool paused = false;
    bool resumed = false;

    connect(&watcher, &QFutureWatcher<void>::started, [&]() {
        started = true;
    });

    connect(&watcher, &QFutureWatcher<void>::paused, [&]() {
        paused = true;
    });

    connect(&watcher, &QFutureWatcher<void>::resumed, [&]() {
        resumed = true;
    });

    watcher.setFuture(future);

    QTRY_COMPARE(started, true);
    QTRY_COMPARE(paused, false);
    QTRY_COMPARE(resumed, false);

    future.pause();

    QCOMPARE(paused, false);

    await(future, 5000);

    QCOMPARE(future.isFinished(), false);

    future.resume();

    await(future, 1000);

    QCOMPARE(future.isFinished(), true);

    QCOMPARE(started, true);
    QCOMPARE(paused, true);
    QCOMPARE(resumed, true);
}
#endif

#define TYPEOF(x) std::decay<decltype(x)>::type

void Spec::test_function_traits()
{
    int dummy = 0;

    auto func1 = []() {
    };

    Q_UNUSED(func1);

    QVERIFY(Private::function_traits<TYPEOF(func1)>::result_type_is_future == 0);
    QVERIFY((std::is_same<Private::function_traits<TYPEOF(func1)>::future_arg_type, void>::value) == 1);
    QVERIFY((std::is_same<Private::arg0_traits<decltype(func1)>::type, void>::value) == 1);
    QVERIFY((std::is_same<Private::observable_traits<decltype(func1)>::type, void>::value) == 1);

    auto func2 = []() -> QFuture<int> {
        return QFuture<int>();
    };

    Q_UNUSED(func2);

    QVERIFY(Private::function_traits<TYPEOF(func2)>::result_type_is_future == true);
    QVERIFY((std::is_same<Private::function_traits<TYPEOF(func2)>::future_arg_type, void>::value) == 0);
    QVERIFY((std::is_same<Private::function_traits<TYPEOF(func2)>::future_arg_type, int>::value) == 1);
    QVERIFY((std::is_same<Private::observable_traits<decltype(func2)>::type, int>::value) == 1);


    auto func3 = []() -> QFuture<void> {
        return QFuture<void>();
    };

    Q_UNUSED(func3);

    QVERIFY(Private::function_traits<TYPEOF(func3)>::result_type_is_future == true);
    QVERIFY((std::is_same<Private::function_traits<TYPEOF(func3)>::future_arg_type, void>::value) == 1);
    QVERIFY((std::is_same<Private::observable_traits<decltype(func3)>::type, void>::value) == 1);

    auto func4 = [=](bool) mutable -> void  {
        dummy++;
    };
    Q_UNUSED(func4);

    QVERIFY((std::is_same<Private::function_traits<TYPEOF(func4)>::template arg<0>::type, bool>::value) == 1);
    QVERIFY((std::is_same<Private::arg0_traits<decltype(func4)>::type, bool>::value) == 1);
    QVERIFY((std::is_same<Private::observable_traits<decltype(func4)>::type, void>::value) == 1);

    auto func5 = [=]() mutable -> QFuture<QFuture<int>> {
        return QFuture<QFuture<int>>();
    };

    QVERIFY((std::is_same<Private::observable_traits<decltype(func5)>::type, int>::value) == 1);

    {
        QFuture<QFuture<bool>> f1;
        static_assert(std::is_same<Private::future_traits<decltype(f1)>::arg_type, bool>::value, "");

        QFuture<QFuture<QFuture<int>>> f2;
        static_assert(std::is_same<Private::future_traits<decltype(f2)>::arg_type, int>::value, "");

    }

}

void Spec::test_private_DeferredFuture()
{
    auto defer = Private::DeferredFuture<void>::create();

    auto worker = [=]() {
        Automator::wait(50);
    };

    defer->complete(QtConcurrent::run(worker));

    QFuture<void> future = defer->future();
    QCOMPARE(future.isFinished(), false);
    QCOMPARE(future.isRunning(), true);

    QVERIFY(waitUntil(future, 1000));

    QCOMPARE(future.isFinished(), true);
    QCOMPARE(future.isRunning(), false);

}


void Spec::test_private_run()
{
    QFuture<bool> bFuture = finishedFuture<bool>(true);
    QFuture<void> vFuture;
    QFuture<int> iFuture = finishedFuture<int>(10);

    auto iCallbackBool = [](bool value) {
        Q_UNUSED(value);
        return 1;
    };

    auto iCallbackVoid = []() {
        return 2;
    };

    auto vCallbackBool = []() {
    };

    auto value = AsyncFuture::Private::eval(iCallbackBool, bFuture);
    QCOMPARE(value.value, 1);

    value = AsyncFuture::Private::eval(iCallbackVoid, bFuture);
    QCOMPARE(value.value, 2);

    value = AsyncFuture::Private::eval(iCallbackVoid, vFuture);
    QCOMPARE(value.value, 2);

    AsyncFuture::Private::eval(vCallbackBool, bFuture);

    auto iCallbackFutureI = [](QFuture<int> future){
        return future.result();
    };

    auto d = deferred<int>();
    iFuture = d.future();
    d.complete(20);
    QVERIFY(waitUntil(iFuture,1000));

    value = AsyncFuture::Private::eval(iCallbackFutureI, iFuture);
    QCOMPARE(value.value, 20);

    //Error statement
    // value = ObservableFuture::Private::run(iCallbackBool, vFuture);
}

void Spec::test_observe_future_future()
{
    auto worker = [=]() {
        QList<int> list;
        list << 1 << 2 << 3 << 4;
        return QtConcurrent::mapped(list, mapFunc);
    };

    // Convert QFuture<QFuture<int>> to QFuture<int>
    QFuture<int> future = observe(QtConcurrent::run(worker)).future();
    QVERIFY(!future.isFinished());
    await(future);

    QCOMPARE(future.progressValue(), 4);
    QCOMPARE(future.isStarted(), true);

    QList<int> result = future.results();
    QCOMPARE(result.size(), 4);
}

void Spec::test_Observable_context()
{

    QFuture<bool> bFuture;
    QFuture<int> iFuture;
    QFuture<void> vFuture, vFuture2;

    /* Case 1. QFuture<bool> + int(bool) */

    auto bWorker = [=]() -> bool {
        Automator::wait(50);
        return true;
    };

    auto iCleanupBool = [&](bool value) -> int {
        Automator::wait(50);
        Q_UNUSED(value);
        return 5;
    };

    bFuture = QtConcurrent::run(bWorker);
    iFuture = observe(bFuture).context(this,iCleanupBool).future();
    QCOMPARE(iFuture.isFinished(), false);
    QCOMPARE(iFuture.isRunning(), true);

    QVERIFY(waitUntil([&](){
        return iFuture.isFinished();
    }, 1000));

    QCOMPARE(iFuture.isRunning(), false);
    QCOMPARE(iFuture.isFinished(), true);
    QCOMPARE(iFuture.result(), 5);

    /* Case 2: QFuture<bool> + void(bool) */

    bool vCleanupBoolCalled = false;

    auto vCleanupBool = [&](bool value) -> void {
        Q_UNUSED(value);
        Automator::wait(50);
        vCleanupBoolCalled = true;
    };

    bFuture = QtConcurrent::run(bWorker);
    vFuture = observe(bFuture).context(this, vCleanupBool).future();
    QCOMPARE(vFuture.isFinished(), false);

    QVERIFY(waitUntil([&](){
        return vFuture.isFinished();
    }, 1000));

    QCOMPARE(vFuture.isFinished(), true);
    QCOMPARE(vCleanupBoolCalled, true);

    /* Case3: QFuture<void> + void(void) */

    bool vCleanupVoidCalled = false;

    auto vCleanupVoid = [&]() -> void {
        Automator::wait(50);
        vCleanupVoidCalled = true;
    };

    auto vWorker = []() -> void {
            Automator::wait(50);
    };

    vFuture = QtConcurrent::run(vWorker);
    vFuture2 = observe(vFuture).context(this, vCleanupVoid).future();
    QCOMPARE(vFuture2.isFinished(), false);

    QVERIFY(waitUntil([&](){
        return vFuture2.isFinished();
    }, 1000));

    QCOMPARE(vFuture2.isFinished(), true);
    QCOMPARE(vCleanupVoidCalled, true);

    /* Remarks: QFuture<void> + void(bool) */
    /* It is not a valid situation */

    /* Extra case. Depend on a finished future */
    {
        vCleanupVoidCalled = false;
        auto d = deferred<void>();
        d.complete();
        vFuture = d.future();
        QCOMPARE(vFuture.isFinished(), true);
        QCOMPARE(vFuture.isCanceled(), false);

        vFuture2 = observe(vFuture).context(this, vCleanupVoid).future();
        QCOMPARE(vFuture2.isFinished(), false);

        QVERIFY(waitUntil([&](){
            return vFuture2.isFinished();
        }, 1000));

        QCOMPARE(vFuture2.isFinished(), true);
        QCOMPARE(vCleanupVoidCalled, true);
    }
}

void Spec::test_Observable_context_destroyed()
{
    QObject* context = new QObject();

    auto bWorker = [=]() -> bool {
        Automator::wait(500);
        return true;
    };

    bool vCleanupVoidCalled = false;
    auto vCleanupVoid = [&]() -> void {
        vCleanupVoidCalled = true;
    };

    QFuture<bool> bFuture = QtConcurrent::run(bWorker);

    QFuture<void> vFuture = observe(bFuture).context(context, vCleanupVoid).future();

    QCOMPARE(vFuture.isFinished(), false);
    delete context;

    QCOMPARE(vFuture.isFinished(), true);
    QCOMPARE(vFuture.isCanceled(), true);
    QCOMPARE(vCleanupVoidCalled, false);


    QVERIFY(waitUntil([&](){
        return bFuture.isFinished();
    }, 1000));
}

void Spec::test_Observable_context_in_thread()
{
    auto worker = [&]() -> void {
        QObject context;

        QThread* workerThread = QThread::currentThread();

        QVERIFY(workerThread != QCoreApplication::instance()->thread());

        auto worker = [=]() -> void {
            QVERIFY(QThread::currentThread() != workerThread);

            Automator::wait(50);
        };

        auto cleanup = [&]() -> void {
            QVERIFY(QThread::currentThread() == workerThread);
            Automator::wait(50);
        };

        auto f1 = QtConcurrent::run(worker);
        auto f2 = observe(f1).context(&context, cleanup).future();

        QVERIFY(waitUntil([&](){
            return f2.isFinished();
        }, 1000));
    };

    QThreadPool pool;
    pool.setMaxThreadCount(1);
    QFuture<void> future = QtConcurrent::run(&pool, worker);

    future.waitForFinished();
}

void Spec::test_Observable_context_in_different_thread()
{
    QObject context;

    auto worker = [&]() -> void {

        QThread* workerThread = QThread::currentThread();

        QVERIFY(workerThread != QCoreApplication::instance()->thread());

        auto worker = [=]() -> void {
            QVERIFY(QThread::currentThread() != workerThread);

            Automator::wait(50);
        };

        auto cleanup = [&]() -> void {
            QVERIFY(QThread::currentThread() != workerThread);
            Automator::wait(50);
        };

        auto f1 = QtConcurrent::run(worker);
        auto f2 = observe(f1).context(&context, cleanup).future();

        QVERIFY(waitUntil([&](){
            return f2.isFinished();
        }, 1000));
    };

    QThreadPool pool;
    pool.setMaxThreadCount(1);
    QFuture<void> future = QtConcurrent::run(&pool, worker);

    waitUntil(future);
}

void Spec::test_Observable_context_return_future()
{
    auto bWorker = [=]() -> bool {
        Automator::wait(50);
        return true;
    };

    auto futureCleanupBool = [&](bool value) {
        Q_UNUSED(value)
        auto internalWorker = [=]() -> int {
            Automator::wait(50);

            return 10;
        };
        QFuture<int> future = QtConcurrent::run(internalWorker);
        return future;
    };

    QFuture<bool> bFuture = QtConcurrent::run(bWorker);

    Observable<int> observable = observe(bFuture).context(this, futureCleanupBool);

    waitUntil(observable.future(), 1000);

}

void Spec::test_Observable_signal()
{
    auto proxy = new SignalProxy(this);

    QFuture<void> vFuture = observe(proxy, &SignalProxy::proxy0).future();

    QCOMPARE(vFuture.isFinished(), false);
    QCOMPARE(vFuture.isRunning(), true);

    QMetaObject::invokeMethod(proxy, "proxy0");

    QCOMPARE(vFuture.isFinished(), false);
    QCOMPARE(vFuture.isRunning(), true);

    QVERIFY(waitUntil([&](){
        return vFuture.isFinished();
    }, 1000));

    QCOMPARE(vFuture.isFinished(), true);
    QCOMPARE(vFuture.isRunning(), false);

    delete proxy;
}

void Spec::test_Observable_signal_with_argument()
{
    auto *proxy = new SignalProxy(this);

    QFuture<int> iFuture = observe(proxy, &SignalProxy::proxy1).future();

    QCOMPARE(iFuture.isFinished(), false);
    QCOMPARE(iFuture.isRunning(), true);
    QMetaObject::invokeMethod(proxy,
                              "proxy1",
                              Qt::DirectConnection,
                              Q_ARG(int, 5));

    QCOMPARE(iFuture.isFinished(), false);
    QCOMPARE(iFuture.isRunning(), true);

    QVERIFY(waitUntil([&](){
        return iFuture.isFinished();
    }, 1000));

    QCOMPARE(iFuture.isFinished(), true);
    QCOMPARE(iFuture.isRunning(), false);
    QCOMPARE(iFuture.result(), 5);

    delete proxy;
}

void Spec::test_Observable_signal_by_signature()
{

    {
        // Observe a signal with no argument
        auto proxy = new SignalProxy(this);

        QFuture<void> vFuture = observe(proxy, SIGNAL(proxy0())).future();

        QCOMPARE(vFuture.isFinished(), false);
        QCOMPARE(vFuture.isRunning(), true);

        QMetaObject::invokeMethod(proxy, "proxy0");

        QCOMPARE(vFuture.isFinished(), false);
        QCOMPARE(vFuture.isRunning(), true);

        QVERIFY(waitUntil([&](){
            return vFuture.isFinished();
        }, 1000));

        QCOMPARE(vFuture.isFinished(), true);
        QCOMPARE(vFuture.isRunning(), false);

        delete proxy;
    }

    {
        // Observe a sigal without argument
        auto *proxy = new SignalProxy(this);

        QFuture<QVariant> iFuture = observe(proxy, SIGNAL(proxy1(int))).future();

        QCOMPARE(iFuture.isFinished(), false);
        QCOMPARE(iFuture.isRunning(), true);
        QMetaObject::invokeMethod(proxy,
                                  "proxy1",
                                  Qt::DirectConnection,
                                  Q_ARG(int, 5));

        QCOMPARE(iFuture.isFinished(), false);
        QCOMPARE(iFuture.isRunning(), true);

        QVERIFY(waitUntil([&](){
            return iFuture.isFinished();
        }, 1000));

        QCOMPARE(iFuture.isFinished(), true);
        QCOMPARE(iFuture.isRunning(), false);
        QCOMPARE(iFuture.result().toInt(), 5);

        delete proxy;
    }

    {
        // invalid signature

        auto *proxy = new SignalProxy(this);

        QFuture<QVariant> iFuture = observe(proxy, SIGNAL(noSuchSignal())).future();
        QCOMPARE(iFuture.isCanceled(), true);
        Q_UNUSED(iFuture);

        delete proxy;
    }

}

void Spec::test_Observable_signal_destroyed()
{
    auto proxy = new SignalProxy(this);

    QFuture<void> vFuture = observe(proxy, &SignalProxy::proxy0).future();

    QCOMPARE(vFuture.isFinished(), false);
    QCOMPARE(vFuture.isRunning(), true);

    delete proxy;

    QCOMPARE(vFuture.isFinished(), true);
    QCOMPARE(vFuture.isRunning(), false);
    QCOMPARE(vFuture.isCanceled(), true);
}

void Spec::test_Observable_subscribe()
{
    {
        // complete
        auto o = deferred<int>();
        auto c1 = Callable<int>();
        auto result = o.subscribe(c1.func).future();

        QCOMPARE(result.isFinished(), false);
        o.complete(10);

        QCOMPARE(c1.called, false);
        waitUntil(o.future());
        QCOMPARE(c1.called, true);
        QCOMPARE(c1.value, 10);
        QCOMPARE(result.isFinished(), true);
        QCOMPARE(result.isCanceled(), false);
    }

    {
        // cancel
        auto o = deferred<int>();
        auto c1 = Callable<int>();
        auto c2 = Callable<void>();
        auto result = o.subscribe(c1.func, c2.func).future();
        o.cancel();

        QCOMPARE(c1.called, false);
        QCOMPARE(result.isFinished(), false);

        waitUntil(o.future());

        QCOMPARE(c1.called, false);
        QCOMPARE(c2.called, true);
        QCOMPARE(result.isFinished(), true);
        QCOMPARE(result.isCanceled(), true);
    }
}

void Spec::test_Observable_subscribe_in_thread()
{
    QThreadPool pool;
    pool.setMaxThreadCount(4);

    auto worker = [&]() -> void {
        QThread* workerThread = QThread::currentThread();

        QVERIFY(workerThread != QCoreApplication::instance()->thread());

        auto worker = [=]() -> void {
            QVERIFY(QThread::currentThread() != workerThread);
            Automator::wait(50);
        };

        auto cleanup = [&]() -> void {
            QVERIFY(QThread::currentThread() == QCoreApplication::instance()->thread());
            Automator::wait(50);
        };

        auto f1 = QtConcurrent::run(&pool, worker);
        auto f2 = observe(f1).subscribe(cleanup).future();

        QVERIFY(waitUntil([&](){
            return f2.isFinished();
        }, 1000));
    };

    QFuture<void> future = QtConcurrent::run(&pool, worker);

    QVERIFY(waitUntil(future , 1000));
}

void Spec::test_Observable_subscribe_return_future()
{
    auto bWorker = [=]() -> bool {
        Automator::wait(50);
        return true;
    };

    auto futureCleanupBool = [&](bool value) {
        Q_UNUSED(value)
        auto internalWorker = [=]() -> int {
            Automator::wait(50);

            return 10;
        };
        QFuture<int> future = QtConcurrent::run(internalWorker);
        return future;
    };

    Observable<bool> observable1 = observe(QtConcurrent::run(bWorker));
    Observable<int> observable2 = observable1.subscribe(futureCleanupBool);

    waitUntil(observable2.future(), 1000);
    QCOMPARE(observable2.future().result(), 10);

    observable2 = observe(QtConcurrent::run(bWorker)).subscribe(futureCleanupBool,[]() {});
    QCOMPARE(observable2.future().isFinished(), false);
    waitUntil(observable2.future(), 1000);
    QCOMPARE(observable2.future().result(), 10);
}

void Spec::test_Observable_subscribe_return_canceledFuture()
{
    auto start = Deferred<void>();
    auto f1 = start.future();
    QList<int> sequence;
    QList<int> expectedSequence;
    expectedSequence << 2;

    auto defer = Deferred<void>();
    defer.cancel();
    auto canceledFuture = defer.future();

    auto f2 = observe(f1).subscribe([&]() {
        sequence << 2;
        return canceledFuture;
    }).future();

    auto f3 = observe(f2).subscribe([&]() {
        sequence << 3;
    }).future();

    start.complete();

    waitUntil(f3, 1000);

    QCOMPARE(sequence, expectedSequence);

}

void Spec::test_Observable_subscribe_return_mappedFuture()
{

    auto future = observe(QtConcurrent::run([](){})).subscribe([=]() {
        QList<int> input;
        input << 1 << 2 << 3;
        auto ret = QtConcurrent::mapped(input, square);
        return ret;
    }).future();

    QTRY_COMPARE(future.isRunning(), false);

    QCOMPARE(future.progressMaximum(), 3);
    QList<int> expected;
    expected << 1 << 4 << 9;

    QCOMPARE(future.results(), expected);
}

void Spec::test_Observable_subscribe_exception()
{

    auto future = observe(QtConcurrent::run([](){})).subscribe([=]() {
        throw QException();
    }).future();

    await(future);

    QCOMPARE(future.isCanceled(), true);

    future = observe(QtConcurrent::run([](){})).subscribe([=]() {
        throw std::exception();
    }).future();

    await(future);

    QCOMPARE(future.isCanceled(), true);

}

void Spec::test_Observable_onProgress()
{
    class CustomDeferred: public AsyncFuture::Deferred<int> {
    public:

        void setProgressValue(int value) {
            AsyncFuture::Deferred<int>::deferredFuture->setProgressValue(value);
        }

        void setProgressRange(int min, int max) {
            AsyncFuture::Deferred<int>::deferredFuture->setProgressRange(min, max);
        }

        void reportResult(int value, int index) {
            AsyncFuture::Deferred<int>::deferredFuture->reportResult(value, index);
        }

    };

    CustomDeferred defer;
    auto future = defer.future();

    int count = 0;
    int value = 999;
    int min = 999;
    int max = -1;

    bool inMainThread = false;

    defer.onProgress([&]() -> bool {
        count++;
        value = future.progressValue();
        min = future.progressMinimum();
        max = future.progressMaximum();

        inMainThread = (QThread::currentThread() == QCoreApplication::instance()->thread());
        return value != max;
    });

    QCOMPARE(count, 0);
    defer.setProgressRange(0, 10);

    QTRY_VERIFY2_WITH_TIMEOUT(count > 0, "", 1000);
    QCOMPARE(value, 0);
    QCOMPARE(min, 0);
    QCOMPARE(max, 10);
    QVERIFY(inMainThread);
    int oldCount = count;

    defer.setProgressValue(5);
    QTRY_VERIFY2_WITH_TIMEOUT(oldCount != count, "", 1000);

    QCOMPARE(value, 5);
    QCOMPARE(min, 0);
    QCOMPARE(max, 10);
    QVERIFY(inMainThread);
    oldCount = count;

    defer.setProgressValue(10);
    QTRY_VERIFY2_WITH_TIMEOUT(oldCount != count, "", 1000);

    QCOMPARE(value, 10);
    QCOMPARE(min, 0);
    QCOMPARE(max, 10);
    QVERIFY(inMainThread);
    oldCount = count;

    defer.setProgressValue(12);
    Automator::wait(100);

    QVERIFY(oldCount == count);

    {
        // Test with mutable function

        auto defer = AsyncFuture::deferred<void>();

        defer.onProgress([=]() mutable -> void {

        });

    }
}

void Spec::test_Observable_onCanceled()
{
    bool canceled = false;
    auto defer = deferred<void>();

    defer.onCanceled([&]() {
        canceled = true;
    });

    defer.cancel();
    QCOMPARE(canceled, false);
    await(defer.future());
    Automator::wait(10);

    QCOMPARE(canceled, true);
}

void Spec::test_Observable_onCanceled_deferred()
{
    auto d1 = deferred<void>();
    auto d2 = deferred<void>();

    d1.onCanceled(d2);

    d1.cancel();

    QCOMPARE(d1.future().isFinished(), true);
    QCOMPARE(d2.future().isFinished(), false);
    QCOMPARE(d2.future().isCanceled(), false);

    await(d2.future());

    QCOMPARE(d2.future().isFinished(), true);
    QCOMPARE(d2.future().isCanceled(), true);
}

void Spec::test_Observable_onCanceled_future()
{
    auto d1 = deferred<int>();
    auto d2 = deferred<int>();

    d1.onCanceled(d2.future());

    d1.future().cancel();

    QCOMPARE(d1.future().isCanceled(), true);
    QCOMPARE(d1.future().isFinished(), false);

    QCOMPARE(d2.future().isCanceled(), false);
    QCOMPARE(d2.future().isFinished(), false);

    QTRY_COMPARE(d2.future().isCanceled(), true);

    QCOMPARE(d2.future().isFinished(), false);
}

void Spec::test_Observable_onCompleted()
{
    bool called = false;
    auto defer = deferred<void>();

    defer.onCompleted([&]() {
        called = true;
    });

    defer.complete();
    QCOMPARE(called, false);
    await(defer.future());
    Automator::wait(10);

    QCOMPARE(called, true);
}

void Spec::test_Observable_setProgressValue()
{

    auto defer = deferred<int>();
    auto future = defer.future();

    QCOMPARE(future.progressValue(), 0);
    QCOMPARE(future.progressMinimum(), 0);
    QCOMPARE(future.progressMaximum(), 0);

    defer.setProgressValue(10);
    defer.setProgressRange(5, 30);

    QCOMPARE(future.progressValue(), 10);
    QCOMPARE(future.progressMinimum(), 5);
    QCOMPARE(future.progressMaximum(), 30);
}

void Spec::test_Deferred()
{
    {
        // defer<bool>::complete
        auto d = AsyncFuture::deferred<bool>();
        auto d2 = d;
        auto future = d.future();
        Callable<bool> callable;
        observe(future).context(this,callable.func);

        QCOMPARE(future.isRunning(), true);
        QCOMPARE(future.isFinished(), false);

        d.complete(true);

        QCOMPARE(callable.called, false);
        QCOMPARE(future.isRunning(), false);
        QCOMPARE(future.isFinished(), true);

        tick();

        QCOMPARE(callable.called, true);
        QCOMPARE(future.isRunning(), false);
        QCOMPARE(future.isFinished(), true);
	QCOMPARE(future.isResultReadyAt(0), true);
        QCOMPARE(future.result(), true);

        d2.complete(true);
    }

    {
        // defer<void>::complete
        QFuture<void> future;
        {
            auto d = AsyncFuture::deferred<void>();
            future = d.future();
            QCOMPARE(future.isRunning(), true);
            QCOMPARE(future.isFinished(), false);

            d.complete();

            QCOMPARE(future.isRunning(), false);
            QCOMPARE(future.isFinished(), true);
        }

        // d was destroyed, but future leave

        waitUntil(future, 1000);

        QCOMPARE(future.isRunning(), false);
        QCOMPARE(future.isFinished(), true);
    }

    {
        // defer<bool>::cancel
        auto d = AsyncFuture::deferred<bool>();
        auto future = d.future();
        QCOMPARE(future.isRunning(), true);
        QCOMPARE(future.isFinished(), false);

        d.cancel();

        QCOMPARE(future.isRunning(), false);
        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), true);

        QCOMPARE(future.isResultReadyAt(0), false);
    }


    {
        // defer<void>::cancel
        auto d = AsyncFuture::deferred<void>();
        auto future = d.future();
        QCOMPARE(future.isRunning(), true);
        QCOMPARE(future.isFinished(), false);

        d.cancel();

        QCOMPARE(future.isRunning(), false);
        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), true);
    }

    {
        // destroy defer<void>
        QFuture<void> future;
        {
            auto d = AsyncFuture::deferred<void>();
            future = d.future();
            QCOMPARE(future.isRunning(), true);
            QCOMPARE(future.isFinished(), false);
        }

        QCOMPARE(future.isRunning(), false);
        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), true);
    }

}

void Spec::test_Deferred_complete_future()
{
    auto timeout = []() {
        Automator::wait(50);
    };

    {
        // Case: complete(QFuture) which could be finished without error
        QFuture<void> future;

        {
            auto d = deferred<void>();
            future = d.future();

            // d is destroyed but complete(QFuture) increased the ref count and therefore it won't be canceled
            d.complete(QtConcurrent::run(timeout));
        }

        QCOMPARE(future.isFinished(), false);
        QCOMPARE(future.isCanceled(), false);

        waitUntil(future);

        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), false);
    }

    {
        // case: complete(future) which will be canceled.

        auto source = deferred<void>();

        // Case: complete(QFuture) which could be finished without error
        QFuture<void> future;
        {
            auto d = deferred<void>();
            future = d.future();
            d.complete(source.future());
        }

        QCOMPARE(future.isFinished(), false);
        QCOMPARE(future.isCanceled(), false);

        source.cancel();

        QCOMPARE(future.isFinished(), false);
        QCOMPARE(future.isCanceled(), false);

        QVERIFY(waitUntil(future, 1000));

        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), true);
    }

    {
        // case: 3 - complete QFuture<QFuture<>>

        auto defer = deferred<int>();

        {
            auto worker = [=]() {
                auto internalWorker = [=]() {
                    return 99;
                };
                return QtConcurrent::run(internalWorker);
            };

            QFuture<QFuture<int>> future = QtConcurrent::run(worker);
            defer.complete(future);
        }

        auto future = defer.future();
        QCOMPARE(future.isFinished(), false);
        QCOMPARE(future.isCanceled(), false);

        await(future, 1000);

        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), false);
        QCOMPARE(future.result(), 99);
    }
}

void Spec::test_Deferred_complete_future_future()
{
    /*
     Remarks:
     - Deferred<void>::complete(QFuture<QFuture<int>>)
     - That is not supported.
     */

    auto worker = [=]() {
        Automator::wait(50);
        QList<int> list;
        list << 1 << 2 <<3;

        return QtConcurrent::mapped(list, mapFunc);
    };

    QList<int> expectedResult;
    expectedResult << 1 << 4 << 9;

    {
        // Deferred<int>::complete(QFuture<QFuture<int>>)
        auto f1 = QtConcurrent::run(worker);

        auto defer = deferred<int>();

        defer.complete(f1);
        auto f2 = defer.future();
        QCOMPARE(f2.progressValue(), 0);
        QCOMPARE(f2.progressMaximum(), 0);

        await(f2);

        QCOMPARE(f2.progressValue(), 3);
        QCOMPARE(f2.progressMaximum(), 3);

        QCOMPARE(f2.results() , expectedResult);
    }


}

void Spec::test_Deferred_complete_list()
{
    auto defer = deferred<int>();

    QList<int> expected;

    expected << 1 << 2 << 3;

    defer.complete(expected);

    auto future = defer.future();
    QVERIFY(future.isFinished());

    QVERIFY(future.results() == expected);
}

void Spec::test_Deferred_cancel_future()
{

    auto timeout = []() {
        Automator::wait(50);
    };

    {
        // Case: cancel(QFuture) which could be finished without error
        QFuture<void> future;

        {
            auto d = deferred<void>();
            future = d.future();

            // d is destroyed but complete(QFuture) increased the ref count and therefore it won't be canceled
            d.cancel(QtConcurrent::run(timeout));
        }

        QCOMPARE(future.isFinished(), false);
        QCOMPARE(future.isCanceled(), false);

        waitUntil(future);

        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), true);
    }

    {
        // case: complete(future) which will be canceled.

        auto completeSource = deferred<void>();
        auto cancelSource = deferred<void>();

        // Case: complete(QFuture) which could will be canceled
        QFuture<void> future;
        {
            auto d = deferred<void>();
            future = d.future();
            d.complete(completeSource.future());
            d.cancel(cancelSource.future());
        }

        QCOMPARE(future.isFinished(), false);
        QCOMPARE(future.isCanceled(), false);

        cancelSource.cancel();

        QCOMPARE(future.isFinished(), false);
        QCOMPARE(future.isCanceled(), false);

        waitUntil(future, 500);

        QCOMPARE(future.isFinished(), false);
        QCOMPARE(future.isCanceled(), false);
        // It won't reponse to a canceled future

        // However, if you didn't call complete(), the future will be canceled due to ref count system

        completeSource.complete();
        QVERIFY(waitUntil(future, 1000));

        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), false);
    }
}

void Spec::test_Deferred_future_cancel()
{
    {
        int canceledCount = 0;

        auto defer = deferred<void>();

        defer.onCanceled([&]() {
            canceledCount++;
        });

        defer.future().cancel();
        Automator::wait(50);

        auto future = defer.future();

        QTRY_COMPARE(canceledCount, 1);
        Automator::wait(50);

        QCOMPARE(future.isCanceled(), true);
        QCOMPARE(future.isFinished(), false);
    }


}

void Spec::test_Deferred_across_thread()
{
    auto defer = deferred<int>();

    auto worker = [=]() mutable {
        Automator::wait(50);
        defer.complete(99);
    };

    QtConcurrent::run(worker);

    Test::waitUntil(defer.future());
    QCOMPARE(defer.future().result(), 99);
}

void Spec::test_Deferred_inherit()
{
    class CustomDeferredVoid : public Deferred<void> {
    public:
        CustomDeferredVoid() {
            deferredFuture->setProgressRange(0, 3);
        }
    };

    class CustomDeferredInt : public Deferred<int> {
    public:
        CustomDeferredInt() {
            deferredFuture->setProgressRange(0, 3);
        }

        void reportResult(int value, int index) {
            int progressValue = future().progressValue();
            deferredFuture->reportResult(value, index);
            deferredFuture->setProgressValue(progressValue + 1);
        }
    };

    CustomDeferredInt defer;
    QCOMPARE(defer.future().progressValue(), 0);
    QList<int> progressList;

    QFutureWatcher<int> watcher;
    connect(&watcher, &QFutureWatcher<int>::progressValueChanged, [&](int value) {
        progressList << value;
    });
    watcher.setFuture(defer.future());

    defer.reportResult(2, 2);
    QCOMPARE(defer.future().progressValue(), 1);

    defer.reportResult(1, 1);
    QCOMPARE(defer.future().progressValue(), 2);

    defer.reportResult(3, 0);
    QCOMPARE(defer.future().progressValue(), 3);

    QVERIFY(!defer.future().isFinished());

// If you call the following code, valgrind could complains memory leakage in reportResult().
// Reference: https://www.travis-ci.org/benlau/asyncfuture/builds/246818440
/*
    QList<int> expected;
    expected << 0 << 1 << 2;
    defer.complete(expected);

    QVERIFY(defer.future().results() == expected);
*/
    Automator::wait(100);
    QVERIFY(progressList.size() > 1);
    QVERIFY(progressList.size() < 3);
    for (int i = 0 ; i < progressList.size() - 1;i++) {
        QVERIFY(progressList[i] < progressList[i+1]);
    }
}

void Spec::test_Deferred_track()
{
    class CustomDeferred : public Deferred<int> {
    public:
        AsyncFuture::Private::DeferredFuture<int>* deferred() const {
            return deferredFuture.data();
        }
    };

    CustomDeferred cd;
    cd.deferred()->setProgressRange(0, 10);

    AsyncFuture::Deferred<int> defer;
    defer.track(cd.future());

    QCOMPARE(defer.future().progressMaximum(), 10);
    QCOMPARE(defer.future().progressValue(), 0);

    cd.deferred()->setProgressValue(1);

    QTRY_COMPARE(defer.future().progressMaximum(), 10);
    QTRY_COMPARE(defer.future().progressValue(), 1);

    cd.deferred()->setProgressValue(10);
    Automator::wait(10);

    QCOMPARE(defer.future().progressMaximum(), 10);
    QCOMPARE(defer.future().progressValue(), 10);
    QVERIFY(!defer.future().isFinished());

}

void Spec::test_Deferred_track_started()
{
    QFuture<void> future;
    QFutureWatcher<void> watcher;

    QList<int> input;
    input << 0 << 1 << 2;

    future = observe(QtConcurrent::run([=]() { })).subscribe([=]() {
        return QtConcurrent::map(input , square);
    }).future();

    bool started = false;

    connect(&watcher, &QFutureWatcher<void>::started, [&]() {
        started = true;
    });

    watcher.setFuture(future);

    QTRY_COMPARE(started, true);

    await(future, 1000);
    QCOMPARE(future.isFinished(), true);
}

void Spec::test_Deferred_setProgress()
{
    class CustomDeferred : public Deferred<void> {
    public:
        CustomDeferred() {
        }

        void setProgressRange(int minimum, int maximum) {
            deferredFuture->setProgressRange(minimum, maximum);
        }

        void setProgressValue(int value) {
            deferredFuture->setProgressValue(value);
        }
    };

    CustomDeferred defer;

    QCOMPARE(defer.future().progressMaximum(), 0);
    QCOMPARE(defer.future().progressMinimum(), 0);
    QCOMPARE(defer.future().progressValue(), 0);

    defer.setProgressRange(2,10);
    defer.setProgressValue(3);

    QCOMPARE(defer.future().progressMaximum(), 10);
    QCOMPARE(defer.future().progressMinimum(), 2);
    QCOMPARE(defer.future().progressValue(), 3);
}

void Spec::test_Combinator()
{
    {
        // case: all completed
        auto d1 = deferred<int>();
        auto d2 = deferred<QString>();
        auto d3 = deferred<void>();
        auto c = Callable<void>();

        auto combinator = combine();
        combinator << d1.future() << d2.future() << d3.future();

        QFuture<void> future = combinator.future();

        observe(future).subscribe(c.func);

        d1.complete(1);
        d2.complete("second");
        d3.complete();

        QCOMPARE(c.called, false);
        QCOMPARE(future.isFinished(), false);

        QVERIFY(waitUntil(future,1000));

        QCOMPARE(c.called, true);
        QCOMPARE(future.isFinished(), true);
    }


    {
        // case: all completed (but Combinator was destroyed )
        QFuture<void> future ;

        auto d1 = deferred<int>();
        auto d2 = deferred<QString>();
        auto d3 = deferred<void>();
        auto c = Callable<void>();

        {
            future = (combine() << d1.future() << d2.future() << d3.future()).future();
        }

        observe(future).subscribe(c.func);

        d1.complete(1);
        d2.complete("second");
        d3.complete();

        QCOMPARE(c.called, false);
        QCOMPARE(future.isFinished(), false);

        QVERIFY(waitUntil(future,1000));

        QCOMPARE(future.isFinished(), true);
        QCOMPARE(c.called, true);
    }

    {
        // case: combine(false), cancel
        auto d1 = deferred<int>();
        auto d2 = deferred<QString>();
        auto d3 = deferred<void>();

        auto combinator = combine();
        combinator << d1.future() << d2.future() << d3.future();

        QFuture<void> future = combinator.future();

        Callable<void> canceled;

        observe(future).subscribe([](){}, canceled.func);

        d1.complete(2);
        d2.cancel();

        QVERIFY(waitUntil(future,1000));

        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), true);
        QCOMPARE(canceled.called, true);
    }

    {
        // case: combine(true), cancel
        auto d1 = deferred<int>();
        auto d2 = deferred<QString>();
        auto d3 = deferred<void>();

        auto combinator = combine(AllSettled);
        combinator << d1.future() << d2.future() << d3.future();

        QFuture<void> future = combinator.future();

        Callable<void> completed;
        Callable<void> canceled;

        observe(future).subscribe(completed.func, canceled.func);

        d1.complete(2);
        d2.cancel();

        QVERIFY(!waitUntil(future,1000));

        QCOMPARE(future.isFinished(), false);
        QCOMPARE(future.isCanceled(), false);

        QCOMPARE(canceled.called, false);
        d3.complete();

        QVERIFY(waitUntil(future,1000));
        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), true);
        QCOMPARE(canceled.called, true);
    }

    {
        // ccombinator << Deferred << Deferred
        auto d1 = deferred<int>();
        auto d2 = deferred<QString>();
        auto d3 = deferred<void>();

        auto combinator = combine(AllSettled);
        combinator << d1 << d2 << d3;

        QFuture<void> future = combinator.future();

        Callable<void> completed;
        Callable<void> canceled;

        observe(future).subscribe(completed.func, canceled.func);

        d1.complete(2);
        d2.cancel();

        QVERIFY(!waitUntil(future,1000));

        QCOMPARE(future.isFinished(), false);
        QCOMPARE(future.isCanceled(), false);

        QCOMPARE(canceled.called, false);
        d3.complete();

        QVERIFY(waitUntil(future,1000));
        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), true);
        QCOMPARE(canceled.called, true);


    }

}

void Spec::test_Combinator_add_to_already_finished()
{
    {
        // case: combine(true), cancel
        auto d1 = deferred<int>();
        auto d2 = deferred<QString>();
        auto d3 = deferred<void>();
        auto d4 = deferred<bool>();

        Combinator copy;

        {
            auto combinator = combine();
            copy = combinator;

            combinator << d1.future() << d2.future() << d3.future();

            d1.complete(1);
            d2.complete("second");
            d3.complete();

            QVERIFY(waitUntil(combinator.future(), 1000));
        }

        copy << d4.future();
        d4.complete(true);

        QVERIFY(waitUntil(copy.future(), 1000)); // It is already resolved
    }
}

void Spec::test_Combinator_progressValue()
{

    {
        auto d1 = timeout(50);
        auto d2 = timeout(60);
        auto d3 = timeout(30);

        auto combinator = combine();

        auto future = combinator.future();

        QCOMPARE(future.progressValue(), 0);
        QCOMPARE(future.progressMaximum(), 0);

        combinator << d1 << d2 << d3;

        QCOMPARE(future.progressValue(), 0);
        QCOMPARE(future.progressMaximum(), 3);

        await(future);

        QCOMPARE(future.progressValue(), 3);
        QCOMPARE(future.progressMaximum(), 3);

    }

}

void Spec::test_alive()
{

    {
        auto d1 = deferred<TrackingData>();
        auto d2 = deferred<TrackingData>();

        d2.complete(d1.future());

        QCOMPARE(TrackingData::aliveCount(), 0);

        {
            TrackingData dummy;
            d1.complete(dummy);
            QCOMPARE(TrackingData::aliveCount(), 1);
        }

        QCOMPARE(TrackingData::aliveCount(), 1);

        await(d2.future());
        Automator::wait(10);
    }

    Automator::wait(10);
    QCOMPARE(TrackingData::aliveCount(), 0);

}
