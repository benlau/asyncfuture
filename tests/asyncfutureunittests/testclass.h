#ifndef TESTCLASS_H
#define TESTCLASS_H

#include <QObject>

class TestClass : public QObject
{
    Q_OBJECT
public:
    TestClass(QObject* parent = nullptr);

    void emitSomeTestSignal() {
        emit someTestSignal();
    }

signals:
    void someTestSignal();
    void someOtherTestSignal();
};

#endif // TESTCLASS_H
