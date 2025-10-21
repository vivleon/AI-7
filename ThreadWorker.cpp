#include "ThreadWorker.h"

ThreadWorker::ThreadWorker(QObject *parent) : QObject(parent)
{
    // analyzer
    connect(&analyzerThread, &QThread::started, this, [this]() {
        analyzer = new PatternAnalyzer();
        analyzer->moveToThread(&analyzerThread);
        emit analyzerReady(analyzer);
    });
    connect(&analyzerThread, &QThread::finished, this, [this]() {
        if (analyzer) analyzer->deleteLater();
    });

    // sensor
    connect(&sensorThread, &QThread::started, this, [this]() {
        sensor = new SensorManager();
        sensor->moveToThread(&sensorThread);
        emit sensorReady(sensor);
        sensor->start();
    });
    connect(&sensorThread, &QThread::finished, this, [this]() {
        if (sensor) sensor->deleteLater();
    });

    // 파이프라인은 두 객체 준비 이후 MainDashboard에서 연결하게 했으므로 여기선 생략
}

ThreadWorker::~ThreadWorker() { stop(); }

void ThreadWorker::start() {
    analyzerThread.start();
    sensorThread.start();
}

void ThreadWorker::stop() {
    if (sensor) sensor->stop();
    sensorThread.quit();
    analyzerThread.quit();
    sensorThread.wait();
    analyzerThread.wait();
}
