#include "AlertRulesDialog.h"
#include "DBBridge.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QCloseEvent>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <QLabel>
#include <algorithm>
#include <cmath>
#include <limits>

static double percentile(QVector<double> v, double p) {
    if (v.isEmpty()) return std::numeric_limits<double>::quiet_NaN();
    std::sort(v.begin(), v.end());
    const double idx = (v.size() - 1) * p;
    const int lo = int(std::floor(idx));
    const int hi = int(std::ceil(idx));
    if (lo == hi) return v[lo];
    const double t = idx - lo;
    return v[lo] * (1.0 - t) + v[hi] * t;
}
static double mean(const QVector<double>& v){ if(v.isEmpty()) return NAN; double s=0; for(double x:v)s+=x; return s/v.size(); }
static double stdev(const QVector<double>& v){ if(v.size()<2) return NAN; double m=mean(v), s2=0; for(double x:v){ double d=x-m; s2+=d*d; } return std::sqrt(s2/(v.size()-1)); }

static void prepTypeTable(QTableWidget* t) {
    t->setColumnCount(4);
    t->setHorizontalHeaderLabels({"Type","Warn","High","Unit"});
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
}

static void prepSensorTable(QTableWidget* t) {
    t->setColumnCount(5);
    t->setHorizontalHeaderLabels({"SensorId","Type","Warn","High","Unit"});
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
}

AlertRulesDialog::AlertRulesDialog(DBBridge* db, const QString& actor, QWidget* parent)
    : QDialog(parent), db_(db), actor_(actor)
{
    initUi();
    loadType();
    loadSensor();
}

QWidget* AlertRulesDialog::makeTypeCombo(const QString& current) const {
    auto *c = new QComboBox; c->addItems({"TEMP","VIB","INTR"});
    int idx = c->findText(current); if (idx<0) idx=0; c->setCurrentIndex(idx);
    return c;
}
QWidget* AlertRulesDialog::makeSensorTypeCombo(const QString& current) const {
    return makeTypeCombo(current);
}
QString AlertRulesDialog::comboValueAt(QTableWidget* tbl, int row, int col) const {
    if (!tbl) return {};
    if (auto *c = qobject_cast<QComboBox*>(tbl->cellWidget(row,col)))
        return c->currentText();
    if (tbl->item(row,col)) return tbl->item(row,col)->text().trimmed();
    return {};
}

void AlertRulesDialog::initUi() {
    auto *lay = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    // By Type
    auto *pageType = new QWidget(tabs);
    auto *vl1 = new QVBoxLayout(pageType);
    tblType_ = new QTableWidget(pageType);
    prepTypeTable(tblType_);
    vl1->addWidget(tblType_);
    pageType->setLayout(vl1);

    // By Sensor
    auto *pageSensor = new QWidget(tabs);
    auto *vl2 = new QVBoxLayout(pageSensor);

    auto *hl = new QHBoxLayout();
    comboType_ = new QComboBox(pageSensor);
    comboType_->addItems({"All","TEMP","VIB","INTR"});
    editFilter_ = new QLineEdit(pageSensor);

    hl->addWidget(new QLabel("Filter Type", pageSensor));
    hl->addWidget(comboType_);
    hl->addWidget(new QLabel("SensorId contains", pageSensor));
    hl->addWidget(editFilter_);
    hl->addStretch(1);
    vl2->addLayout(hl);

    tblSensor_ = new QTableWidget(pageSensor);
    prepSensorTable(tblSensor_);
    vl2->addWidget(tblSensor_);
    pageSensor->setLayout(vl2);

    tabs->addTab(pageType, "By Type");
    tabs->addTab(pageSensor, "By Sensor");
    lay->addWidget(tabs);

    // Buttons
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    auto *btnSuggestTypes   = new QPushButton("Suggest (14d, Type)", this);
    auto *btnSuggestSensors = new QPushButton("Suggest (14d, Sensor)", this);
    auto *btnSave = new QPushButton("Save", this);
    auto *btnCancel = new QPushButton("Cancel", this);

    btnRow->addWidget(btnSuggestTypes);
    btnRow->addWidget(btnSuggestSensors);
    btnRow->addWidget(btnSave);
    btnRow->addWidget(btnCancel);
    lay->addLayout(btnRow);

    connect(btnSave, &QPushButton::clicked, this, &AlertRulesDialog::onSave);
    connect(btnCancel,&QPushButton::clicked, this, &AlertRulesDialog::onCancel);
    connect(comboType_, &QComboBox::currentTextChanged, this, &AlertRulesDialog::onFilterChanged);
    connect(editFilter_, &QLineEdit::textChanged, this, &AlertRulesDialog::onFilterChanged);

    setMinimumSize(900, 560);
    setModal(true);

    connect(btnSuggestTypes,   &QPushButton::clicked, this, &AlertRulesDialog::onSuggestTypes);
    connect(btnSuggestSensors, &QPushButton::clicked, this, &AlertRulesDialog::onSuggestSensors);
}

void AlertRulesDialog::markDirty(bool d){ dirty_ = d; }

void AlertRulesDialog::closeEvent(QCloseEvent* e) {
    if (dirty_ && !confirmLose("close")) { e->ignore(); return; }
    QDialog::closeEvent(e);
}

bool AlertRulesDialog::confirmLose(const QString& what) {
    const auto r = QMessageBox::question(this, "Confirm",
                                         QString("Are you sure you want to %1 without saving?").arg(what));
    return (r == QMessageBox::Yes);
}

void AlertRulesDialog::loadType() {
    tblType_->clearContents();
    auto rows = db_->listTypeThresholds();
    tblType_->setRowCount(rows.size());
    for (int i=0;i<rows.size();++i){
        const auto &r = rows[i];
        tblType_->setCellWidget(i,0, makeTypeCombo(r.type));
        tblType_->setItem(i,1,new QTableWidgetItem(QString::number(r.warn)));
        tblType_->setItem(i,2,new QTableWidgetItem(QString::number(r.high)));
        tblType_->setItem(i,3,new QTableWidgetItem(r.unit));
    }
    markDirty(false);
}

void AlertRulesDialog::loadSensor() {
    tblSensor_->clearContents();
    const QString typ = comboType_->currentText();
    const QString like = editFilter_->text().trimmed();
    auto rows = db_->listSensorThresholds(typ=="All"?QString():typ, like);
    tblSensor_->setRowCount(rows.size());
    for (int i=0;i<rows.size();++i){
        const auto &r = rows[i];
        tblSensor_->setItem(i,0,new QTableWidgetItem(r.sensorId));
        tblSensor_->setCellWidget(i,1, makeSensorTypeCombo(r.type));
        tblSensor_->setItem(i,2,new QTableWidgetItem(QString::number(r.warn)));
        tblSensor_->setItem(i,3,new QTableWidgetItem(QString::number(r.high)));
        tblSensor_->setItem(i,4,new QTableWidgetItem(r.unit));
    }
    markDirty(false);
}

void AlertRulesDialog::onFilterChanged() { loadSensor(); }

bool AlertRulesDialog::collectType(QVector<TypeThreshold>& out) const {
    out.clear();
    for (int r=0;r<tblType_->rowCount();++r){
        TypeThreshold t;
        t.type = comboValueAt(tblType_, r, 0);
        t.warn = tblType_->item(r,1) ? tblType_->item(r,1)->text().toDouble() : 0.0;
        t.high = tblType_->item(r,2) ? tblType_->item(r,2)->text().toDouble() : 0.0;
        t.unit = tblType_->item(r,3) ? tblType_->item(r,3)->text().trimmed() : QString();
        if (t.type.isEmpty()) return false;
        out.push_back(t);
    }
    return true;
}

bool AlertRulesDialog::collectSensor(QVector<SensorThreshold>& out) const {
    out.clear();
    for (int r=0;r<tblSensor_->rowCount();++r){
        if (!tblSensor_->item(r,0)) continue;
        SensorThreshold t;
        t.sensorId = tblSensor_->item(r,0)->text().trimmed();
        t.type     = comboValueAt(tblSensor_, r, 1);
        t.warn     = tblSensor_->item(r,2) ? tblSensor_->item(r,2)->text().toDouble() : 0.0;
        t.high     = tblSensor_->item(r,3) ? tblSensor_->item(r,3)->text().toDouble() : 0.0;
        t.unit     = tblSensor_->item(r,4) ? tblSensor_->item(r,4)->text().trimmed() : QString();
        if (t.sensorId.isEmpty() || t.type.isEmpty()) return false;
        out.push_back(t);
    }
    return true;
}

void AlertRulesDialog::onSuggestSensors()
{
    const QDateTime to   = QDateTime::currentDateTime();
    const QDateTime from = to.addDays(-14);

    const auto sensors = db_->listKnownSensors();

    for (int r=0; r<tblSensor_->rowCount(); ++r) {
        if (!tblSensor_->item(r,0)) continue;
        const QString sid  = tblSensor_->item(r,0)->text().trimmed();
        const QString type = comboValueAt(tblSensor_, r, 1);
        if (sid.isEmpty() || type.isEmpty()) continue;
        if (!sensors.contains(sid)) continue;

        auto vals = db_->sampleValues(type, sid, from, to, 5000);
        if (vals.size() < 30) continue;

        const double q1 = percentile(vals, 0.25);
        const double q3 = percentile(vals, 0.75);
        const double iqr = q3 - q1;
        double warn = q3;
        double high = q3 + 1.5 * iqr;

        if (iqr < 1e-6) {
            const double mu  = mean(vals);
            const double sig = stdev(vals);
            warn = mu + 1.0 * sig;
            high = mu + 2.0 * sig;
        }

        if (!tblSensor_->item(r,2)) tblSensor_->setItem(r,2,new QTableWidgetItem);
        if (!tblSensor_->item(r,3)) tblSensor_->setItem(r,3,new QTableWidgetItem);
        tblSensor_->item(r,2)->setText(QString::number(warn, 'f', 2));
        tblSensor_->item(r,3)->setText(QString::number(high, 'f', 2));
    }
    markDirty(true);
    QMessageBox::information(this, "Suggested", "센서별 임계치 추천값을 채웠습니다. 저장으로 반영하세요.");
}

void AlertRulesDialog::onSuggestTypes()
{
    const QDateTime to   = QDateTime::currentDateTime();
    const QDateTime from = to.addDays(-14);

    for (int r=0; r<tblType_->rowCount();++r) {
        const QString type = comboValueAt(tblType_, r, 0);
        if (type.isEmpty()) continue;

        auto vals = db_->sampleValues(type, QString(), from, to, 5000);
        if (vals.size() < 30) continue;

        const double q1 = percentile(vals, 0.25);
        const double q3 = percentile(vals, 0.75);
        const double iqr = q3 - q1;
        double warn = q3;
        double high = q3 + 1.5 * iqr;

        if (iqr < 1e-6) {
            const double mu  = mean(vals);
            const double sig = stdev(vals);
            warn = mu + 1.0 * sig;
            high = mu + 2.0 * sig;
        }

        if (!tblType_->item(r,1)) tblType_->setItem(r,1,new QTableWidgetItem);
        if (!tblType_->item(r,2)) tblType_->setItem(r,2,new QTableWidgetItem);
        tblType_->item(r,1)->setText(QString::number(warn, 'f', 2));
        tblType_->item(r,2)->setText(QString::number(high, 'f', 2));
    }
    markDirty(true);
    QMessageBox::information(this, "Suggested", "타입별 임계치 추천값을 채웠습니다. 저장으로 반영하세요.");
}

void AlertRulesDialog::onSave() {
    QVector<TypeThreshold> types;
    QVector<SensorThreshold> sensors;
    if (!collectType(types) || !collectSensor(sensors)) {
        QMessageBox::warning(this,"Invalid","Please fill required fields.");
        return;
    }
    const auto r = QMessageBox::question(this,"Confirm Save",
                                         QString("Apply %1 type rules and %2 sensor overrides?")
                                             .arg(types.size()).arg(sensors.size()));
    if (r != QMessageBox::Yes) return;

    QString err;
    if (!db_->saveTypeThresholds(actor_, types, &err)) {
        QMessageBox::warning(this,"Save failed (type)", err);
        return;
    }
    if (!db_->saveSensorThresholds(actor_, sensors, &err)) {
        QMessageBox::warning(this,"Save failed (sensor)", err);
        return;
    }

    const QDateTime to   = QDateTime::currentDateTime();
    const QDateTime from = to.addDays(-14);

    for (const auto& t : types) {
        const int sampleCount = db_->sampleValues(t.type, QString(), from, to, 5000).size();
        db_->insertThresholdTuning(actor_, "TYPE", t.type, "IQR", t.warn, t.high, sampleCount,
                                   QString("from=%1,to=%2").arg(from.toString(Qt::ISODate)).arg(to.toString(Qt::ISODate)));
    }
    for (const auto& s : sensors) {
        const int sampleCount = db_->sampleValues(s.type, s.sensorId, from, to, 5000).size();
        db_->insertThresholdTuning(actor_, "SENSOR", s.sensorId, "IQR", s.warn, s.high, sampleCount,
                                   QString("type=%1;from=%2;to=%3").arg(s.type, from.toString(Qt::ISODate), to.toString(Qt::ISODate)));
    }

    markDirty(false);
    accept();
}

void AlertRulesDialog::onCancel() {
    if (dirty_ && !confirmLose("cancel")) return;
    reject();
}
