#include <QTest>
#include <Automator>
#include <QtConcurrent>
#include <QThread>
#include <QScopedPointer>
#include <asyncfuture.h>
#include "tools.h"
#include "testfunctions.h"
#include "bugtests.h"
#include "FinishedAndCancelThread.h"
#include "testclass.h"

using namespace AsyncFuture;
using namespace Tools;
using namespace Test;

BugTests::BugTests(QObject *parent) : QObject(parent)
{
    // This function do nothing but could make Qt Creator Autotests plugin recognize this test
    auto ref =[=]() {
        QTest::qExec(this, 0, 0);
    };
    Q_UNUSED(ref);
}

void BugTests::test_nested_context()
{
    class Actor : QObject {
    public:

        QFuture<void> action1() {

            auto worker = []() {
                Automator::wait(50);
            };

            auto cleanup = []() {

            };

            return observe(QtConcurrent::run(worker)).context(this , cleanup).future();
        }

        QFuture<bool> action2() {

            auto worker = []() {
                Automator::wait(50);
            };

            auto cleanup = [=]() {
                auto defer = deferred<bool>();

                auto future = action1();

                observe(future).context(this, [=]() mutable {
                    defer.complete(true);
                });

                return defer.future();
            };

            return observe(QtConcurrent::run(worker)).context(this, cleanup).future();
        }
    };

    Actor actor;

    QFuture<void> future = actor.action2();

    QVERIFY(waitUntil(future, 1000));
}

void BugTests::test_nested_subscribe_in_thread()
{
    bool called = false;

    auto worker = [&]() {
        observe(timeout(50)).subscribe([]() {
            return timeout(50);
        }).subscribe([&]() {
            called = true;
        });
    };

    QtConcurrent::run(worker);

    QVERIFY(waitUntil([&]() {
        return called;
    }, 1000));

}

void BugTests::test_nested_context_in_thread()
{
    bool called = false;

    auto worker = [&]() {
        auto localTimeout = [](int sleepTime) {
            return QtConcurrent::run([sleepTime]() {
                QThread::currentThread()->msleep(sleepTime);
            });
        };

        QEventLoop loop;

        auto context = QSharedPointer<QObject>::create();

        QThread* workerThread = QThread::currentThread();

        observe(localTimeout(50)).context(context.data(), [localTimeout, context]() {
            return localTimeout(50);
        }).context(context.data(), [context, &called, workerThread, &loop]() {
            called = true;
            QVERIFY(QThread::currentThread() == context->thread());
            QVERIFY(QThread::currentThread() == workerThread);
            QMetaObject::invokeMethod(&loop, "quit"); //;&QEventLoop::quit); //loop.quit();
        }, [&]() {
            QVERIFY(QThread::currentThread() == context->thread());
            QVERIFY(QThread::currentThread() == workerThread);
            QMetaObject::invokeMethod(&loop, "quit"); //&QEventLoop::quit); //loop.quit();
        });

        loop.exec();
    };

    QtConcurrent::run(worker);

    QVERIFY(waitUntil([&]() {
        return called;
    }, 1000));

}

void BugTests::test_issue4()
{
    int actualValue = -1;
    auto defer = deferred<int>();

    auto f = defer.subscribe([&](int&& value) {
        actualValue = value;
    }).future();

    defer.complete(78);

    await(f);

    QCOMPARE(actualValue, 78);
}

void BugTests::test_canceled_before_finished() {

    class TestClass {
    public:
        void doWork() {
            if(m_doWorkFuture.isRunning() || m_doWorkFuture.isStarted()) {
                m_doWorkFuture.cancel();
            }

            int currentCount = m_count;
            auto runFuture = QtConcurrent::run([currentCount](){
                return currentCount + 1;
            });

            m_doWorkFuture = runFuture;

            m_subscribeFuture = observe(runFuture).subscribe([this](){
                m_finishCount++;
            },
            [this](){
                m_cancelCount++;
            }).future();

        }

        void waitToFinish() {
            await(m_subscribeFuture);
        }

        QFuture<void> m_subscribeFuture;
        QFuture<int> m_doWorkFuture;
        int m_count = 0;
        int m_finishCount = 0;
        int m_cancelCount = 0;
    };

    TestClass myTest;
    int totalRun = 10;
    for(int i = 0; i < totalRun; i++) {
        myTest.doWork();
    }
    myTest.waitToFinish();

    QCOMPARE(myTest.m_finishCount, 1);
    QCOMPARE(myTest.m_cancelCount, totalRun - 1);
}

void BugTests::test_finished_and_cancel_in_other_thread() {
    auto thread = QSharedPointer<FinishedAndCancelThread>(new FinishedAndCancelThread());

    QScopedPointer<QObject> context(new QObject());

    connect(thread.data(), &FinishedAndCancelThread::startedSubTask, context.data(), [thread]() {
        thread->doWork();
        QThread::msleep(100);
        thread->m_concurrentFuture.cancel();
    });

    auto threadFuture = observe(thread.data(), &FinishedAndCancelThread::finished).future();
    thread->start();

    await(threadFuture);

    QCOMPARE(thread->m_cancelCount, 1);
}

void BugTests::test_combiner_handle_nested_progress()
{
    QVector<int> ints(100);
    std::iota(ints.begin(), ints.end(), ints.size());
    std::function<int (int)> func = [](int x)->int {
        QThread::msleep(100);
        return x * x;
    };
    QFuture<int> mappedFuture = QtConcurrent::mapped(ints, func);
    QFuture<int> runFuture = QtConcurrent::run([]() {
        QThread::msleep(100);
        return 10;
    });

    AsyncFuture::Combinator combine;
    combine << mappedFuture << runFuture;

    auto combineFuture = combine.future();

    QCOMPARE(combineFuture.progressMinimum(), 0);
    QCOMPARE(combineFuture.progressMaximum(), ints.size() + 1);

    int progress = 0;
    AsyncFuture::observe(combineFuture).onProgress([&progress, combineFuture](){
        QVERIFY(progress <= combineFuture.progressValue());
        progress = combineFuture.progressValue();
    });

    await(combineFuture);

    QCOMPARE(combineFuture.progressMaximum(), combineFuture.progressValue());
}

void BugTests::test_combiner_combiner_handle_nested_progress()
{
    QVector<int> ints(100);
    std::iota(ints.begin(), ints.end(), ints.size());
    std::function<int (int)> func = [](int x)->int {
        QThread::msleep(100);
        return x * x;
    };
    QFuture<int> mappedFuture = QtConcurrent::mapped(ints, func);
    QFuture<int> runFuture = QtConcurrent::run([]() {
        QThread::msleep(100);
        return 10;
    });

    AsyncFuture::Combinator combine;
    combine << mappedFuture << runFuture;

    QFuture<int> mappedFuture2 = QtConcurrent::mapped(ints, func);
    AsyncFuture::Combinator combine2;
    combine2 << combine.future() << mappedFuture;

    auto combineFuture = combine2.future();

    QCOMPARE(combineFuture.progressMinimum(), 0);
    QCOMPARE(combineFuture.progressMaximum(), 2 * ints.size() + 1);

    int progress = -1;
    AsyncFuture::observe(combineFuture).onProgress([&progress, combineFuture](){
        QVERIFY(progress <= combineFuture.progressValue());
        progress = combineFuture.progressValue();
    });

    await(combineFuture);

    QCOMPARE(combineFuture.progressMaximum(), combineFuture.progressValue());
}

void BugTests::test_chained_obserable_progress()
{
    QVector<int> ints(100);
    std::iota(ints.begin(), ints.end(), ints.size());
    std::function<int (int)> func = [](int x)->int {
        QThread::msleep(100);
        return x * x;
    };
    QFuture<int> mappedFuture = QtConcurrent::mapped(ints, func);

    auto nextFuture = AsyncFuture::observe(mappedFuture).subscribe([ints, func](){
        QFuture<int> mappedFuture2 = QtConcurrent::mapped(ints, func);
        return mappedFuture2;
    }).future();

    bool nextExecuted2 = false;
    auto nextFuture2 = AsyncFuture::observe(nextFuture).subscribe([&nextExecuted2, ints, func](){
        QFuture<int> mappedFuture2 = QtConcurrent::mapped(ints, func);
        nextExecuted2 = true;
        return mappedFuture2;
    }).future();

    int progress = -1;
    AsyncFuture::observe(nextFuture2).onProgress([&progress, nextFuture2](){
        QVERIFY2(progress <= nextFuture2.progressValue(), QString("%1 <= %2").arg(progress).arg(nextFuture2.progressValue()).toLocal8Bit());
        progress = nextFuture2.progressValue();
    });

    await(nextFuture2);

    QCOMPARE(nextFuture2.progressMinimum(), 0);
    QCOMPARE(nextFuture2.progressMaximum(), ints.size() * 3);

    QCOMPARE(nextExecuted2, true);
    QCOMPARE(nextFuture2.progressValue(), ints.size() * 3);
}

void BugTests::test_forward_canceled() {
    QVector<int> ints(100);
    std::iota(ints.begin(), ints.end(), ints.size());
    std::function<int (int)> func = [](int x)->int {
        QThread::msleep(100);
        return x * x;
    };
    QFuture<int> mappedFuture = QtConcurrent::mapped(ints, func);

    bool completed1 = false;
    bool canceled1 = false;
    AsyncFuture::observe(mappedFuture).subscribe(
                [&completed1]{ completed1 = true; },
    [&canceled1]{ canceled1 = true; }
    );

    bool started = false;
    bool completed2 = false;
    bool canceled2 = false;
    bool nextFutureCanceled = false;

    auto nextFuture = AsyncFuture::observe(mappedFuture).subscribe([ints, func, &completed2, &canceled2, &started](){
        started = true;
        QFuture<int> mappedFuture2 = QtConcurrent::mapped(ints, func);

        AsyncFuture::observe(mappedFuture2).subscribe(
                    [&completed2]{ completed2 = true; },
        [&canceled2]{ canceled2 = true; }
        );

        return mappedFuture2;
    },
    [&nextFutureCanceled]() {
        nextFutureCanceled = true;
    }

    ).future();

    observe(timeout(50)).subscribe([&nextFuture](){
        nextFuture.cancel();
    });

    await(nextFuture);

    QCOMPARE(completed1, false);
    QCOMPARE(completed2, false);
    QCOMPARE(started, false);
    QCOMPARE(nextFutureCanceled, true);
    QCOMPARE(canceled1, true);
    QCOMPARE(canceled2, false); //This was never started, so it can't be cancelled
}

void BugTests::test_issue4_cancel() {

    class System {
    public:
        double scale = 2.0;
    };

    class Worker {
    public:
        Worker(const System& system) :
            system(system)
        {

        }

        double operator()(QPointF point) const {
            QThread::msleep(100);
            auto scaledPoint = point * system.scale;
            return scaledPoint.manhattanLength();
        }

    private:
        System system;
    };


    auto getSystem = []() {
        return observe(timeout(50)).subscribe([](){ return System(); }).future();
    };

    QAtomicInt mapCount = 0;

    auto compute = [getSystem, &mapCount](QVector<QPointF> coords)->QFuture<double> const
    {
        QFuture<System> sys = getSystem();
        auto f = [=, &mapCount](System&& system)->QFuture<double>
        {
            Worker worker(system);
            std::function<double (QPointF point)> func = [worker, &mapCount](QPointF point)->double {
                mapCount++;
                return worker(point);
            };
            return QtConcurrent::mapped(coords, func);
        };

        auto rv = AsyncFuture::observe(sys).subscribe(f);
        return rv.future();
    };

    auto points = QVector<QPointF>({
                                       QPointF(1.0, 2.0),
                                       QPointF(1.0, 3.0),
                                       QPointF(1.0, 4.0),
                                       QPointF(1.0, 5.0),
                                       QPointF(1.0, 6.0)
                                   });

    while(points.size() < QThread::idealThreadCount() * 2) {
        points += points;
    }

    auto userFuture = compute(points);

    auto timeoutFuture = observe(timeout(100)).subscribe(
                [&userFuture](){
        userFuture.cancel(); }
    ).future();

    auto c = combine() << userFuture << timeoutFuture;
    await(c.future());

    QVERIFY(mapCount < points.size() - 1);
}

void BugTests::test_combine_forward_cancel() {

    QAtomicInt runCount = 0;
    const int count = 100;

    QThreadPool::globalInstance()->setMaxThreadCount(2);

    auto createMappedFuture = [&runCount, count]() {
        QVector<int> ints(count);
        std::iota(ints.begin(), ints.end(), ints.size());
        std::function<int (int)> func = [&runCount](int x)->int {
            QThread::msleep(10);
            runCount++;
            return x * x;
        };
        QFuture<int> mappedFuture = QtConcurrent::mapped(ints, func);
        return mappedFuture;
    };

    QList<QFuture<int>> futures;

    for(int i = 0; i < 4; i++) {
        futures.append(createMappedFuture());
    }

    auto c = combine();
    c << futures;

    auto combineFuture = c.future();

    observe(combineFuture).onProgress([&combineFuture](){
        //Cancel after startup
        combineFuture.cancel();
    });

    await(combineFuture);
    QThreadPool::globalInstance()->waitForDone();

    //All the sub futures should be canceled
    for(auto future : futures) {
        QVERIFY(future.isCanceled());
    }

    //Make sure the sub futures didn't run all the way through
    double fullNumberOfRuns = futures.size() * count;
    double ratio = runCount / fullNumberOfRuns;
    QVERIFY(ratio < 0.1);

    QThreadPool::globalInstance()->setMaxThreadCount(QThread::idealThreadCount());
}

//This is to test https://github.com/benlau/asyncfuture/issues/37
void BugTests::test_chained_cancel() {
    TestClass testclass;
    auto wasCancelled = AsyncFuture::deferred<bool>();
    auto wasInnerCancelled = AsyncFuture::deferred<bool>();

    QFuture<void> future = AsyncFuture::observe(&testclass, &TestClass::someTestSignal)
            .subscribe(
                [&wasInnerCancelled, &testclass, &future]()
    {
        auto innerFuture = AsyncFuture::observe(&testclass,
                                    &TestClass::someOtherTestSignal).subscribe(
                    [&wasInnerCancelled]() {
            wasInnerCancelled.complete(false);
        },
        //Canceled inner
        [&wasInnerCancelled]() {
            //following deferred gets never called
            wasInnerCancelled.complete(true);
        }).future();

        QTimer::singleShot(40, QCoreApplication::instance(), [&future]() {
            future.cancel();
        });

        return innerFuture;
    },
    //Canceled Outer
    [&wasCancelled]() {
        wasCancelled.complete(true);
    }
    ).future();

    testclass.emitSomeTestSignal();
    await(future);
    QVERIFY(future.isCanceled());
    QVERIFY(future.isFinished());

    //These should finish
    await(wasInnerCancelled.future(), 100);
    QVERIFY(wasInnerCancelled.future().isFinished());
    QVERIFY(wasInnerCancelled.future().result());

    //These should finish
    await(wasCancelled.future(), 100);
    QVERIFY(wasCancelled.future().isFinished());
    QVERIFY(wasCancelled.future().result());
}


