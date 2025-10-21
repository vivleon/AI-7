#ifndef THREADWORKER_H
#define THREADWORKER_H

#include <QObject>
#include <QThread>
#include "SensorManager.h"
#include "PatternAnalyzer.h"

class ThreadWorker : public QObject
{
    Q_OBJECT
public:
    explicit ThreadWorker(QObject *parent = nullptr);
    ~ThreadWorker();

    void start();
    void stop();

    SensorManager*   getSensor()   { return sensor; }
    PatternAnalyzer* getAnalyzer() { return analyzer; }

signals:
    void sensorReady(SensorManager*);
    void analyzerReady(PatternAnalyzer*);

private:
    QThread sensorThread;
    QThread analyzerThread;
    SensorManager  *sensor  {nullptr};
    PatternAnalyzer*analyzer{nullptr};
};

#endif // THREADWORKER_H
