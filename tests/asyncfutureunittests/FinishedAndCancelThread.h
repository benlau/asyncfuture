#ifndef FINISHEDANDCANCELTHREAD_H
#define FINISHEDANDCANCELTHREAD_H

#include <QObject>
#include <QThread>
#include <QtConcurrent>
#include <QDebug>

#include "asyncfuture.h"

class FinishedAndCancelThread : public QThread
{
    Q_OBJECT
public:
    FinishedAndCancelThread() {
        setObjectName("FinishedAndCancelThread");
    }

    ~FinishedAndCancelThread() {
        if(contextObject) {
            delete contextObject;
        }
    }

    void doWork() {
        m_runCount++;

        m_concurrentFuture = QtConcurrent::run([]() {
            QThread::msleep(100);
        });

        Q_ASSERT(contextObject->thread() == this);
        Q_ASSERT(QThread::currentThread() != this);

        m_observableFuture = AsyncFuture::observe(m_concurrentFuture).context(contextObject, [this]() {
            m_finishCount++;
            quit();
        },
        [this] {
            m_cancelCount++;
            quit();
        }).future();
    }

    void run() {
        contextObject = new QObject();
        emit startedSubTask();
        exec();
    }

    QObject* contextObject = nullptr;
    QFuture<void> m_concurrentFuture;
    QFuture<void> m_observableFuture;
    int m_finishCount = 0;
    int m_cancelCount = 0;
    int m_runCount = 0;

signals:
    void startedSubTask();

public slots:
};

#endif // FINISHEDANDCANCELTHREAD_H
