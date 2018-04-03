#pragma once
#include <QObject>

class Example : public QObject
{
    Q_OBJECT
public:
    explicit Example(QObject *parent = 0);

private slots:
    void example_Timer_timeout();

    void example_combine_multiple_future();

    void example_worker_context();

    void example_context_return_future();

    void example_promise_like();

    void example_promise_like_complete_future();

    void example_promise_like_timeout();

    void example_fileactor();

    void example_mapped_with_lambda();

    void example_CancellationToken();

    void example_Combinator_timeout();
};

