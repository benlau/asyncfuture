#include <QQmlApplicationEngine>
#include <QTest>
#include <Automator>
#include <asyncfuture.h>
#include <QTimer>
#include "example.h"
#include "testfunctions.h"

using namespace AsyncFuture;
using namespace Test;

Example::Example(QObject *parent) : QObject(parent)
{

    // This function do nothing but could make Qt Creator Autotests plugin recognize QuickTests
    auto ref =[=]() {
        QTest::qExec(this, 0, 0);
    };
    Q_UNUSED(ref);

}

void Example::example_Timer_timeout()
{
    /*
    QTimer timer;
    timer.setInterval(50);

    // Convert a signal from object into QFuture
    QFuture<void> future = observe(&timer,
                                   &QTimer::timeout).future();

    observe(future).subscribe([]() {
        // onCompleted. The future finish and not canceled
        qDebug() << "onCompleted";
    },[]() {
        // onCanceled
        qDebug() << "onCancel";
    });

    timer.start();
    */
}

