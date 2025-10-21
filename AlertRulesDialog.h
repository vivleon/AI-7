#ifndef ALERTRULESDIALOG_H
#define ALERTRULESDIALOG_H

#include <QDialog>
#include <QVector>
#include <QString>

#include "Thresholds.h"

class QTableWidget;
class QComboBox;
class QLineEdit;
class DBBridge;

class AlertRulesDialog : public QDialog {
    Q_OBJECT
public:
    explicit AlertRulesDialog(DBBridge* db, const QString& actor, QWidget* parent=nullptr);

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onSave();
    void onCancel();
    void onFilterChanged();
    void onSuggestTypes();
    void onSuggestSensors();

private:
    void initUi();
    void loadType();
    void loadSensor();
    bool collectType(QVector<TypeThreshold>& out) const;
    bool collectSensor(QVector<SensorThreshold>& out) const;
    bool confirmLose(const QString& what);
    void markDirty(bool d);

    QWidget* makeTypeCombo(const QString& current) const;
    QWidget* makeSensorTypeCombo(const QString& current) const;
    QString  comboValueAt(QTableWidget* tbl, int row, int col) const;

    DBBridge* db_{};
    QString actor_;
    QTableWidget* tblType_{};
    QTableWidget* tblSensor_{};
    QComboBox* comboType_{};
    QLineEdit* editFilter_{};
    bool dirty_{false};
};

#endif // ALERTRULESDIALOG_H
