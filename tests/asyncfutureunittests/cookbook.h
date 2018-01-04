#ifndef COOKBOOK_H
#define COOKBOOK_H

#include <QObject>

/// Collection of example available in the "AsyncFuture Cookbook" article. This test prove they are working

class Cookbook : public QObject
{
    Q_OBJECT
public:
    explicit Cookbook(QObject *parent = nullptr);

signals:


private slots:

    void run_mapped();

};

#endif // COOKBOOK_H
