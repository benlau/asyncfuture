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
};

#endif // BUGTESTS_H
