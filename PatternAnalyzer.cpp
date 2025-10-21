#include "PatternAnalyzer.h"
#include <QDebug>

PatternAnalyzer::PatternAnalyzer(QObject *parent) : QObject(parent) {}

void PatternAnalyzer::process(SensorData data)
{
    if (data.value > 80.0) {
        qDebug() << "[PatternAnalyzer] anomaly detected!"
                 << " type=" << data.type << " value=" << data.value;
        emit anomalyDetected(data);
    }
}
