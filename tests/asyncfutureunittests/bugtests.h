#ifndef BUGTESTS_H
#define BUGTESTS_H

#include <QObject>

class BugTests : public QObject
{
    Q_OBJECT
public:
    explicit BugTests(QObject *parent = nullptr);

signals:

public slots:

private slots:
    void test_nested_context();

    void test_nested_subscribe_in_thread();
    void test_nested_context_in_thread();

    void test_issue4();

    void test_canceled_before_finished();

    void test_finished_and_cancel_in_other_thread();
};

#endif // BUGTESTS_H
