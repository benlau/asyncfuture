#include <QtConcurrent>
#include <QtShell>
#include <QTest>
#include <Automator>
#include <asyncfuture.h>
#include "tools.h"
#include "cookbook.h"
#include "testfunctions.h"

using namespace Tools;

Cookbook::Cookbook(QObject *parent) : QObject(parent)
{
    // This function do nothing but could make Qt Creator Autotests plugin recognize this test
    auto ref =[=]() {
        QTest::qExec(this, 0, 0);
    };
    Q_UNUSED(ref);
}

static QString cat(QString source) {
    return QtShell::cat(source);
}

void Cookbook::run_mapped()
{

    QString input = QtShell::realpath_strip(QtShell::pwd(), QTest::currentTestFunction());

    QtShell::mkdir("-p", input);

    QtShell::touch(input + "/1.json");
    QtShell::touch(input + "/2.json");
    QtShell::touch(input + "/3.txt");
    QtShell::touch(input + "/4.json");

    {
        /* Method 1 - deferred object + mutable lambda function */

        /* Begin of sample code */
        /////////////////////////

        auto defer = AsyncFuture::deferred<QString>();

        auto worker = [=]() mutable {
            QStringList files = QtShell::find(input, "*.json");

            auto future = QtConcurrent::mapped(files, cat);
            defer.complete(future);
        };

        QtConcurrent::run(worker);

        auto future = defer.future();

        /////////////////////////
        /* End of sample code */

        Test::await(future, 1000);

        QCOMPARE(future.progressValue(), 3);
        QCOMPARE(future.progressMaximum(), 3);

    }

    {
        /* Method 2 - Deferred::complete(QFuture<QFuture<T>) */

        /* Begin of sample code */
        /////////////////////////

        auto defer = AsyncFuture::deferred<QString>();

        auto worker = [=]() mutable {
            QStringList files = QtShell::find(input, "*.json");

            return QtConcurrent::mapped(files, cat);
        };

        QFuture<QFuture<QString>> step1 = QtConcurrent::run(worker);
        defer.complete(step1);
        auto future = defer.future();

        /////////////////////////
        /* End of sample code */

        QCOMPARE(future.progressMaximum(), 0);
        QCOMPARE(future.progressValue(), 0);

        Test::await(future, 1000);

        QCOMPARE(future.progressValue(), 3);
        QCOMPARE(future.progressMaximum(), 3);
    }


    {
        /* Method 3 - observe(QFuture<QFuture<T>) */

        /* Begin of sample code */
        /////////////////////////

        auto worker = [=]() mutable {
            QStringList files = QtShell::find(input, "*.json");

            return QtConcurrent::mapped(files, cat);
        };

        auto future = AsyncFuture::observe(QtConcurrent::run(worker)).future();

        /////////////////////////
        /* End of sample code */

        QCOMPARE(future.progressMaximum(), 0);
        QCOMPARE(future.progressValue(), 0);

        Test::await(future, 1000);

        QCOMPARE(future.progressValue(), 3);
        QCOMPARE(future.progressMaximum(), 3);

    }


}
