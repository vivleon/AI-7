// SensorManager.h
#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <QObject>
#include <QTimer>
#include "SensorData.h"     // ⬅️ 여기를 포함, struct 중복 삭제

class SensorManager : public QObject
{
    Q_OBJECT
public:
    explicit SensorManager(QObject *parent = nullptr);
    void start();
    void stop();

signals:
    void newData(SensorData data);

private slots:
    void generateData();

private:
    QTimer timer;
    int counter {0};
};

#endif // SENSORMANAGER_H
