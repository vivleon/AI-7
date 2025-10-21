#ifndef LOCATIONPOPUPDIALOG_H
#define LOCATIONPOPUPDIALOG_H
#pragma once

#include <QDialog>
#include <QMap>
#include <QSet>

class QGridLayout;
class QCheckBox;
class QTableWidget;          // ← 추가: 전방선언
class LogTableModel;
class QModelIndex;     // ← 전방 선언
struct SensorData;     // ← 전방 선언

class QChartView;
class QLineSeries;
class QDateTimeAxis;
class QValueAxis;

class LocationPopupDialog : public QDialog {
    Q_OBJECT
public:
    explicit LocationPopupDialog(const QString& location,
                                 LogTableModel* model,
                                 QWidget* parent = nullptr);

    void setInitialOffKeys(const QSet<QString>& offKeys);

signals:
    void sensorToggleChanged(const QString& place, const QString& key, bool on);

private:
    void addRow(QGridLayout* g, int row, QString key);       // 값으로 캡처
    void appendOne(const SensorData& d);                     // 멤버 헬퍼

private slots:
    void onRowsInserted(const QModelIndex& parent, int first, int last); // 모델 콜백

private:
    QString        loc_;
    LogTableModel* model_ = nullptr;

    QMap<QString, QCheckBox*> boxes_;
    QTableWidget* tbl_ = nullptr;     // ← 멤버 포인터

    // 차트(온도/진동/침입)
    QLineSeries* sTemp_ = nullptr;
    QLineSeries* sVib_  = nullptr;
    QLineSeries* sIntr_ = nullptr;
    QChartView*  vTemp_ = nullptr;
    QChartView*  vVib_  = nullptr;
    QChartView*  vIntr_ = nullptr;
};

#endif // LOCATIONPOPUPDIALOG_H
