#pragma once

#include <QObject>

class SignalProxy : public QObject {
    Q_OBJECT
public:
    inline SignalProxy(QObject* parent = 0) : QObject(parent) {
    }

signals:
    void proxy0();
    void proxy1(int);
};

class Spec : public QObject
{
    Q_OBJECT
public:
    explicit Spec(QObject *parent = 0);

signals:

public slots:

private slots:
    void initTestCase();

    void cleanup();

    void test_QFuture_cancel();

    void test_QFuture_isResultReadyAt();

    void test_QFutureWatcher_in_thread();

    void test_QtConcurrent_exception();

#if QT_VERSION > QT_VERSION_CHECK(5, 7, 1)
    void test_QtConcurrent_map();
#endif

    void test_function_traits();

    void test_private_DeferredFuture();

    void test_private_run();

    void test_observe_future_future();

    void test_Observable_context();
    void test_Observable_context_destroyed();
    void test_Observable_context_in_thread();
    void test_Observable_context_in_different_thread();

    void test_Observable_context_return_future();

    void test_Observable_signal();
    void test_Observable_signal_with_argument();

    void test_Observable_signal_by_signature();


    void test_Observable_signal_destroyed();

    void test_Observable_subscribe();

    void test_Observable_subscribe_in_thread();

    void test_Observable_subscribe_return_future();

    void test_Observable_subscribe_return_canceledFuture();

    void test_Observable_subscribe_return_mappedFuture();

    void test_Observable_subscribe_exception();

    void test_Observable_onProgress();

    void test_Observable_onCanceled();

    void test_Observable_onCanceled_deferred();

    void test_Observable_onCanceled_future();

    void test_Observable_onCompleted();

    void test_Observable_setProgressValue();

    void test_Deferred();
    void test_Deferred_complete_future();
    void test_Deferred_complete_future_future();

    void test_Deferred_complete_list();
    void test_Deferred_cancel_future();

    void test_Deferred_future_cancel();

    void test_Deferred_across_thread();
    void test_Deferred_inherit();
    void test_Deferred_track();
    void test_Deferred_track_started();

    void test_Deferred_setProgress();

    void test_Combinator();

    void test_Combinator_add_to_already_finished();

    void test_Combinator_progressValue();

    void test_alive();

};
