#ifndef PATTERNANALYZER_H
#define PATTERNANALYZER_H

#include <QObject>
#include "SensorManager.h"

class PatternAnalyzer : public QObject
{
    Q_OBJECT
public:
    explicit PatternAnalyzer(QObject *parent = nullptr);

signals:
    void anomalyDetected(SensorData data);

public slots:
    void process(SensorData data);  // 간단한 임계치 룰
};

#endif // PATTERNANALYZER_H
