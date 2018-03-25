#include "trackingdata.h"


static int s_livingCount = 0;

class TrackingDataPriv : public QSharedData
{
public:
    int value;

    TrackingDataPriv() {
        value = 0;
        s_livingCount++;
    }

    ~TrackingDataPriv() {
        s_livingCount--;
    }

};

TrackingData::TrackingData() : data(new TrackingDataPriv)
{

}

TrackingData::TrackingData(const TrackingData &rhs) : data(rhs.data)
{

}

TrackingData &TrackingData::operator=(const TrackingData &rhs)
{
    if (this != &rhs)
        data.operator=(rhs.data);
    return *this;
}

TrackingData::~TrackingData()
{

}

int TrackingData::aliveCount()
{
    return s_livingCount;
}

int TrackingData::value() const
{
    return data->value;
}

void TrackingData::setValue(int value)
{
    data->value = value;
}
