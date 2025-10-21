#ifndef THRESHOLDS_H
#define THRESHOLDS_H

#include <QString>

// yMin/yMax 제거: 축 자동화 전환
struct TypeThreshold {
    QString type;   // TEMP / VIB / INTR
    double warn{50};
    double high{80};
    QString unit;   // °C, mm/s RMS, score
};

struct SensorThreshold {
    QString sensorId;
    QString type;
    double warn{50};
    double high{80};
    QString unit;
};

#endif
