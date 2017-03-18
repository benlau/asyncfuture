#pragma once
#include <QObject>

class Example : public QObject
{
    Q_OBJECT
public:
    explicit Example(QObject *parent = 0);

private slots:
    void example_Timer_timeout();
};

