#include <QTest>
#include "samplecode.h"
#include "asyncfutureutils.h"
#include "testfunctions.h"

using namespace AsyncFuture;
using namespace AsyncFutureUtils;
using namespace Test;

SampleCode::SampleCode(QObject *parent) : QObject(parent)
{
    // This function do nothing but could make Qt Creator Autotests plugin recognize this test
    auto ref =[=]() {
        QTest::qExec(this, 0, 0);
    };
    Q_UNUSED(ref);
}

static QImage readImage(const QString& source) {
    Q_UNUSED(source);
    return QImage();
}

void SampleCode::v0_4_release_note()
{
    {
        // Sample Code 1

        QStringList files;
        files << "1" << "2";

        /* Sample code */

        auto defer = AsyncFuture::deferred<QImage>();

        QFuture<QImage> input = QtConcurrent::mapped(files, readImage);

        defer.complete(input); // defer.future() will be a mirror of `input`. The `progressValue` will be changed and it will emit "started" signal via QFutureWatcher

        /* End of Sample code */

        await(defer.future());
        Automator::wait(10);

        auto future = defer.future();

        QCOMPARE(input.isStarted(), true);

        QCOMPARE(future.progressValue(), files.size());
        QCOMPARE(future.isStarted(), true);
    }
}
