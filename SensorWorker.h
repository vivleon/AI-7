#ifndef SENSORWORKER_H
#define SENSORWORKER_H

#include <QObject>
#include <QTimer>
#include <QRandomGenerator>

class SensorWorker : public QObject
{
    Q_OBJECT
public:
    explicit SensorWorker(QObject *parent = nullptr);

signals:
    void newSensorData(double temperature, double humidity, bool motion);

public slots:
    void start();
    void stop();

private:
    QTimer *timer;
};

#endif // SENSORWORKER_H
