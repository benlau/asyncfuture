#ifndef BUGTESTS_H
#define BUGTESTS_H

#include <QObject>

class BugTests : public QObject
{
    Q_OBJECT
public:
    explicit BugTests(QObject *parent = 0);

signals:

public slots:

private slots:
    void test_nested_context();

    void test_nested_subscribe_in_thread();
};

#endif // BUGTESTS_H
