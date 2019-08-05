#include <QTest>
#include <Automator>
#include <QtConcurrent>
#include <asyncfuture.h>
#include "tools.h"
#include "testfunctions.h"
#include "bugtests.h"

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
    myTest.doWork();
    myTest.doWork();
    myTest.doWork();
    myTest.waitToFinish();

    QCOMPARE(myTest.m_finishCount, 1);
    QCOMPARE(myTest.m_cancelCount, 2);
}
