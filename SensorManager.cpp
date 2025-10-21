// SensorManager.cpp
#include "SensorManager.h"
#include <QDateTime>
#include <QRandomGenerator>

SensorManager::SensorManager(QObject *parent)
    : QObject(parent)
{
    timer.setInterval(1000);
    connect(&timer, &QTimer::timeout, this, &SensorManager::generateData);
}

void SensorManager::start() {
    if (!timer.isActive())
        timer.start();
}

void SensorManager::stop() {
    timer.stop();
}

void SensorManager::generateData() {
    SensorData d;
    d.sensorId  = QString("S-%1").arg(counter++);   // 고유 ID 텍스트로 대체
    d.type      = "TEMP";
    d.value     = QRandomGenerator::global()->bounded(100.0);
    d.level = (d.value > 80) ? "HIGH" : (d.value > 50 ? "MEDIUM" : "LOW");
    d.latitude  = 37.5665;     // 예시 좌표(서울)
    d.longitude = 126.9780;
    d.timestamp = QDateTime::currentMSecsSinceEpoch();
    emit newData(d);
}
