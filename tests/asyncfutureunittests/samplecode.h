#ifndef SAMPLECODE_H
#define SAMPLECODE_H

#include <QObject>

/// Collection of sample code in document. And make sure they are working correctly.

class SampleCode : public QObject
{
    Q_OBJECT
public:
    explicit SampleCode(QObject *parent = nullptr);

signals:

private slots:
    void v0_4_release_note();

};

#endif // SAMPLECODE_H
