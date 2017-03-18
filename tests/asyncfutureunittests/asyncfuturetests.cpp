#include <QtConcurrent>
#include <QTest>
#include <QFuture>
#include <QFutureWatcher>
#include <Automator>
#include "testfunctions.h"
#include "asyncfuture.h"
#include "asyncfuturetests.h"

using namespace AsyncFuture;
using namespace Test;

AsyncFutureTests::AsyncFutureTests(QObject *parent) : QObject(parent)
{
    auto ref = [=]() {
        QTest::qExec(this, 0, 0); // Autotest detect available test cases of a QObject by looking for "QTest::qExec" in source code
    };
    Q_UNUSED(ref);
}

static int test_cancel_worker(int value) {
    Automator::wait(50);

    return value * value;
}

void AsyncFutureTests::test_QFuture_cancel()
{
    QList<int> input;
    for (int i = 0 ; i < 100;i++) {
        input << i;
    }

    QFuture<int> future = QtConcurrent::mapped(input, test_cancel_worker);

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

void AsyncFutureTests::test_private_DeferredFuture()
{
    Private::DeferredFuture<void> defer;

    auto worker = [=]() {
        Automator::wait(50);
    };

    defer.complete(QtConcurrent::run(worker));

    QFuture<void> future = defer.future();
    QCOMPARE(future.isFinished(), false);
    QCOMPARE(future.isRunning(), true);

    QVERIFY(waitUntil(future, 1000));

    QCOMPARE(future.isFinished(), true);
    QCOMPARE(future.isRunning(), false);
}


void AsyncFutureTests::test_private_run()
{
    QFuture<bool> bFuture;
    QFuture<void> vFuture;
    QFuture<int> iFuture;

    auto iCallbackBool = [](bool value) {
        Q_UNUSED(value);
        return 1;
    };

    auto iCallbackVoid = []() {
        return 2;
    };

    auto vCallbackBool = []() {
    };

    auto value = AsyncFuture::Private::run(iCallbackBool, bFuture);
    QCOMPARE(value.value, 1);

    value = AsyncFuture::Private::run(iCallbackVoid, bFuture);
    QCOMPARE(value.value, 2);

    value = AsyncFuture::Private::run(iCallbackVoid, vFuture);
    QCOMPARE(value.value, 2);

    AsyncFuture::Private::run(vCallbackBool, bFuture);

    auto iCallbackFutureI = [](QFuture<int> future){
        return future.result();
    };

    auto d = defer<int>();
    iFuture = d.future();
    d.complete(20);
    QVERIFY(waitUntil(iFuture,1000));

    value = AsyncFuture::Private::run(iCallbackFutureI, iFuture);
    QCOMPARE(value.value, 20);

    //Error statement
    // value = ObservableFuture::Private::run(iCallbackBool, vFuture);
}

void AsyncFutureTests::test_Observable_context()
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
    vCleanupVoidCalled = false;
    vFuture = QFuture<void>();
    QCOMPARE(vFuture.isFinished(), true);
    vFuture2 = observe(vFuture).context(this, vCleanupVoid).future();
    QCOMPARE(vFuture2.isFinished(), false);

    QVERIFY(waitUntil([&](){
        return vFuture2.isFinished();
    }, 1000));

    QCOMPARE(vFuture2.isFinished(), true);
    QCOMPARE(vCleanupVoidCalled, true);
}

void AsyncFutureTests::test_Observable_context_destroyed()
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

void AsyncFutureTests::test_Observable_context_in_thread()
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

void AsyncFutureTests::test_Observable_context_return_future()
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

    //@TODO

    QFuture<bool> bFuture = QtConcurrent::run(bWorker);

    Observable<int> observable = observe(bFuture).context(this, futureCleanupBool);

    waitUntil(observable.future(), 1000);

}

void AsyncFutureTests::test_Observable_signal()
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

void AsyncFutureTests::test_Observable_signal_with_argument()
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

void AsyncFutureTests::test_Observable_signal_destroyed()
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

void AsyncFutureTests::test_Observable_subscribe()
{
    {
        auto o = defer<int>();
        auto c1 = Callable<int>();
        o.subscribe(c1.func);
        o.complete(10);

        QCOMPARE(c1.called, false);
        waitUntil(o.future());
        QCOMPARE(c1.called, true);
        QCOMPARE(c1.value, 10);
    }

    {
        auto o = defer<int>();
        auto c1 = Callable<int>();
        auto c2 = Callable<void>();
        o.subscribe(c1.func, c2.func);
        o.cancel();

        QCOMPARE(c1.called, false);

        waitUntil(o.future());
        tick();
        QCOMPARE(c1.called, false);
        QCOMPARE(c2.called, true);
    }
}



void AsyncFutureTests::test_Defer()
{
    {
        auto d = AsyncFuture::defer<bool>();
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
        QCOMPARE(future.result(), true);

        d2.complete(true);
    }

    {
        QFuture<void> future;
        {
            auto d = AsyncFuture::defer<void>();
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
        auto d = AsyncFuture::defer<bool>();
        auto future = d.future();
        QCOMPARE(future.isRunning(), true);
        QCOMPARE(future.isFinished(), false);

        d.cancel();

        QCOMPARE(future.isRunning(), false);
        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), true);
        QCOMPARE(future.result(), false);
    }

    {
        auto d = AsyncFuture::defer<void>();
        auto future = d.future();
        QCOMPARE(future.isRunning(), true);
        QCOMPARE(future.isFinished(), false);

        d.cancel();

        QCOMPARE(future.isRunning(), false);
        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), true);
    }

    // Auto cancel on destroy
    {
        QFuture<void> future;
        {
            auto d = AsyncFuture::defer<void>();
            future = d.future();
            QCOMPARE(future.isRunning(), true);
            QCOMPARE(future.isFinished(), false);
        }

        QCOMPARE(future.isRunning(), false);
        QCOMPARE(future.isFinished(), true);
        QCOMPARE(future.isCanceled(), true);
    }

}
