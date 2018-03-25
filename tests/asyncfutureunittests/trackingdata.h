#ifndef TRACKINGDATA_H
#define TRACKINGDATA_H

#include <QSharedDataPointer>

class TrackingDataPriv;

class TrackingData
{
public:
    TrackingData();
    TrackingData(const TrackingData &);
    TrackingData &operator=(const TrackingData &);
    ~TrackingData();

    static int aliveCount();

    int value() const;
    void setValue(int value);

private:
    QSharedDataPointer<TrackingDataPriv> data;
};

#endif // TRACKINGDATA_H
