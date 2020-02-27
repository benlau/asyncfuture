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

        observe(localTimeout(50)).context(context.get(), [localTimeout, context]() {
            return localTimeout(50);
        }).context(context.get(), [context, &called, workerThread, &loop]() {
            called = true;
            QVERIFY(QThread::currentThread() == context->thread());
            QVERIFY(QThread::currentThread() == workerThread);
            QMetaObject::invokeMethod(&loop, &QEventLoop::quit); //loop.quit();
        }, [&]() {
            QVERIFY(QThread::currentThread() == context->thread());
            QVERIFY(QThread::currentThread() == workerThread);
            QMetaObject::invokeMethod(&loop, &QEventLoop::quit); //loop.quit();
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
    auto thread = std::shared_ptr<FinishedAndCancelThread>(new FinishedAndCancelThread());

    QScopedPointer<QObject> context(new QObject());

    connect(thread.get(), &FinishedAndCancelThread::startedSubTask, context.get(), [thread]() {
        thread->doWork();
        QThread::msleep(100);
        thread->m_concurrentFuture.cancel();
    });

    auto threadFuture = observe(thread.get(), &FinishedAndCancelThread::finished).future();
    thread->start();

    await(threadFuture);

    QCOMPARE(thread->m_cancelCount, 1);
}
