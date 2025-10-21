#pragma once
#ifndef REALTIMEGEOPANEL_H
#define REALTIMEGEOPANEL_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QTimer>
#include <QTableWidget>
#include <QProgressBar>

QT_BEGIN_NAMESPACE
class QSplitter;
QT_END_NAMESPACE

namespace QtCharts {
class QChart;
class QChartView;
class QBarSet;
class QBarSeries;
class QBarCategoryAxis;
class QValueAxis;
}

class LogTableModel;

class RealtimeGeoPanel : public QWidget {
    Q_OBJECT
public:
    explicit RealtimeGeoPanel(QWidget* parent, LogTableModel* model);
    void forceRebuild();

protected:
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    struct Place { QString name; double lat; double lng; };
    struct Agg   { int temp=0, vib=0, intr=0, high=0; qint64 lastTs=0; };

    // UI
    QSplitter*                  split_  = nullptr;
    QTableWidget*               table_  = nullptr;
    QtCharts::QChart*           chart_  = nullptr;
    QtCharts::QChartView*       view_   = nullptr;
    QtCharts::QBarSet*          seriesTemp_ = nullptr;
    QtCharts::QBarSet*          seriesVib_  = nullptr;
    QtCharts::QBarSet*          seriesIntr_ = nullptr;
    QtCharts::QBarSeries*       barSeries_  = nullptr;
    QtCharts::QBarCategoryAxis* axX_   = nullptr;
    QtCharts::QValueAxis*       axY_   = nullptr;

    // Data
    LogTableModel* model_ = nullptr;
    const std::vector<Place> places_;
    QMap<QString, Agg> byLoc_;
    QMap<QString, int> rowForLoc_;
    QVector<QProgressBar*> riskBars_;
    QTimer coalesceTimer_;

    // Col index
    static constexpr int COL_LOC=0, COL_TEMP=1, COL_VIB=2, COL_INTR=3, COL_HIGH=4, COL_LAST=5, COL_RISK=6;

    // Helpers
    QString nearestPlace(double lat, double lng) const;
    void buildUi();
    void prepopulate();
    void ingestRows(int first, int last);
    void rebuild();

    // Width control (Location: very small, Risk: smaller)
    void applyColumnWidths(); // called on show/resize/ingest
};

#endif // REALTIMEGEOPANEL_H
