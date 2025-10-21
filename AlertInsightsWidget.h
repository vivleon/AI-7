#pragma once
#ifndef ALERTINSIGHTSWIDGET_H
#define ALERTINSIGHTSWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarSeries>
#include <QtCharts/QStackedBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>

class LogTableModel;

class AlertInsightsWidget : public QWidget {
    Q_OBJECT
public:
    explicit AlertInsightsWidget(QWidget* parent, LogTableModel* model);

public slots:
    void rebuild();

private:
    LogTableModel* model_ {};

    // 타임라인 (최근 12h)
    QChartView* viewTimeline_ {};
    QChart*     chartTimeline_ {};
    QBarSeries* seriesTimeline_ {};
    QBarSet*    setTemp_ {};
    QBarSet*    setVib_ {};
    QBarSet*    setIntr_ {};
    QBarCategoryAxis* axX_ {};
    QValueAxis*       axY_ {};

    // 심각도 도넛 (24h)
    QChartView* viewSeverity_ {};
    QChart*     chartSeverity_ {};
    QPieSeries* pie_ {};

    QTimer coalesce_;
};

#endif // ALERTINSIGHTSWIDGET_H
