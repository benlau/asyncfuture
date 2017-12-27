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

static QImage readImage(const QString& src) {
    Q_UNUSED(src);
    return QImage();
}

static QStringList findImageFiles(const QString& src) {
    Q_UNUSED(src);
    QStringList res;
    res << "1" << "2" << "3";
    return res;
}

static QFuture<QImage> readImagesFromFolder(const QString& folder) {
    auto defer = AsyncFuture::deferred<QImage>();

    auto worker = [=]() mutable {
        QStringList files = findImageFiles(folder);
        QFuture<QImage> future = QtConcurrent::mapped(files, readImage);
        defer.complete(future);
    };

    QtConcurrent::run(worker);

    return defer.future();
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
        auto future = defer.future();

        QCOMPARE(input.isStarted(), true);

        QCOMPARE(future.progressValue(), files.size());
        QCOMPARE(future.isStarted(), true);

        QList<QImage> result = future.results();
        QCOMPARE(result.size(), files.size());
    }

    {
        QString input;
        // Sample code 2

        auto future = readImagesFromFolder(input);
        QVERIFY(!future.isFinished());
        await(future);
        Automator::wait(100);

        QCOMPARE(future.progressValue(), 3);
        QCOMPARE(future.isStarted(), true);

        QList<QImage> result = future.results();
        QCOMPARE(result.size(), 3);
    }
}
