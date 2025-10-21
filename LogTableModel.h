#ifndef LOGTABLEMODEL_H
#define LOGTABLEMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include "SensorData.h"


class LogTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit LogTableModel(QObject* parent=nullptr);

    int rowCount(const QModelIndex&) const override;
    int columnCount(const QModelIndex&) const override;
    QVariant headerData(int section, Qt::Orientation, int role) const override;
    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;

    void append(const SensorData& d);
    const SensorData& at(int r) const { return rows[r]; }

    // called when language changed
    void notifyLanguageChanged() {
        if (rows.isEmpty()) {
            emit headerDataChanged(Qt::Horizontal, 0, columnCount({})-1);
        } else {
            emit dataChanged(index(0,0), index(rows.size()-1, columnCount({})-1));
            emit headerDataChanged(Qt::Horizontal, 0, columnCount({})-1);
        }
    }
    enum Col {
        COL_ID = 0,
        COL_LOCATION,          // ✅ 새로 추가
        COL_TYPE,
        COL_VALUE,
        COL_LEVEL,
        COL_TIME,
        COL_DATETIME,
        COL_LAT,
        COL_LNG,
        COL__COUNT
    };

private:
    QVector<SensorData> rows;
};

#endif
