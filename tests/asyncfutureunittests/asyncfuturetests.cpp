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

template <typename T>
QFuture<T> finishedFuture(T value) {

    auto o = deferred<T>();

    o.complete(value);

    return o.future();
}

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

    auto d = deferred<int>();
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
        auto o = deferred<int>();
        auto c1 = Callable<int>();
        o.subscribe(c1.func);
        o.complete(10);

        QCOMPARE(c1.called, false);
        waitUntil(o.future());
        QCOMPARE(c1.called, true);
        QCOMPARE(c1.value, 10);
    }

    {
        auto o = deferred<int>();
        auto c1 = Callable<int>();
        auto c2 = Callable<void>();
        o.subscribe(c1.func, c2.func);
        o.cancel();

        QCOMPARE(c1.called, false);

        waitUntil(o.future());

        QCOMPARE(c1.called, false);
        QCOMPARE(c2.called, true);
    }
}



void AsyncFutureTests::test_Defer()
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

void AsyncFutureTests::test_Defer_complete_future()
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
}

void AsyncFutureTests::test_Defer_cancel_future()
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

void AsyncFutureTests::test_Combinator()
{
    {
        // case: all completed
        auto d1 = deferred<int>();
        auto d2 = deferred<QString>();
        auto d3 = deferred<void>();

        auto combinator = combine();
        combinator << d1.future() << d2.future() << d3.future();

        QFuture<QVariantList> future = combinator.future();
        QVariantList results;

        observe(future).subscribe([&](QVariantList value) {
            results = value;
        });

        d1.complete(1);
        d2.complete("second");
        d3.complete();

        QCOMPARE(results.isEmpty(), true);
        QCOMPARE(future.isFinished(), false);

        QVERIFY(waitUntil(future,1000));

        QCOMPARE(future.isFinished(), true);

        QCOMPARE(results.size(), 3);
        QVERIFY(results[0] == 1);
        QVERIFY(results[1] == "second");
    }


    {
        // case: all completed (but Combinator was destroyed )
        QFuture<QVariantList> future ;

        auto d1 = deferred<int>();
        auto d2 = deferred<QString>();
        auto d3 = deferred<void>();

        {
            future = (combine() << d1.future() << d2.future() << d3.future()).future();
        }

        QVariantList results;

        observe(future).subscribe([&](QVariantList value) {
            results = value;
        });

        d1.complete(1);
        d2.complete("second");
        d3.complete();

        QCOMPARE(results.isEmpty(), true);
        QCOMPARE(future.isFinished(), false);

        QVERIFY(waitUntil(future,1000));

        QCOMPARE(future.isFinished(), true);

        QCOMPARE(results.size(), 3);
        QVERIFY(results[0] == 1);
        QVERIFY(results[1] == "second");
    }

    {
        // case: combine(false), cancel
        auto d1 = deferred<int>();
        auto d2 = deferred<QString>();
        auto d3 = deferred<void>();

        auto combinator = combine();
        combinator << d1.future() << d2.future() << d3.future();

        QFuture<QVariantList> future = combinator.future();

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

        auto combinator = combine(true);
        combinator << d1.future() << d2.future() << d3.future();

        QFuture<QVariantList> future = combinator.future();

        Callable<QVariantList> completed;
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
