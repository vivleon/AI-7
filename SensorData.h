#ifndef SENSORDATA_H
#define SENSORDATA_H
#pragma once
#include <QString>
#include <QtGlobal>

struct SensorData {
    QString sensorId;
    QString type;      // TEMP / VIB / INTR
    double  value {0.0};
    QString level;     // LOW / MEDIUM / HIGH
    qint64  timestamp {0}; // epoch ms
    double  latitude {0.0};
    double  longitude {0.0};
};

#endif // SENSORDATA_H
