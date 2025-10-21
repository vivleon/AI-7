#include "SensorWorker.h"

SensorWorker::SensorWorker(QObject *parent)
    : QObject(parent), timer(new QTimer(this))
{
    connect(timer, &QTimer::timeout, this, [this]() {
        double temp = QRandomGenerator::global()->bounded(15.0, 35.0);
        double hum  = QRandomGenerator::global()->bounded(30.0, 70.0);
        bool motion = QRandomGenerator::global()->bounded(0, 2);
        emit newSensorData(temp, hum, motion);
    });
}

void SensorWorker::start() { timer->start(1000); }
void SensorWorker::stop()  { timer->stop(); }
