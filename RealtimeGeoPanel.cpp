#include "RealtimeGeoPanel.h"
#include "LogTableModel.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QDateTime>
#include <QPainter>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <algorithm>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>



RealtimeGeoPanel::RealtimeGeoPanel(QWidget* parent, LogTableModel* model)
    : QWidget(parent),
    model_(model),
    places_({
        {"경복궁", 37.579617,126.977041}, {"창덕궁", 37.579414,126.991102},
        {"창경궁", 37.582604,126.994546}, {"덕수궁", 37.565804,126.975146},
        {"경희궁", 37.571785,126.959206}, {"경복궁-서문", 37.578645,126.976829},
        {"경복궁-북측", 37.579965,126.978345},{"창덕궁-서측", 37.581112,126.992230},
        {"덕수궁-동측", 37.564990,126.976000},{"경희궁-남측", 37.572500,126.960800}
    })
{
    buildUi();
    prepopulate();

    if (model_) {
        connect(model_, &QAbstractItemModel::rowsInserted, this,
                [this](const QModelIndex&, int first, int last){ ingestRows(first, last); });
        const int rc = model_->rowCount(QModelIndex());
        if (rc > 0) ingestRows(0, rc - 1);
    }
    connect(&coalesceTimer_, &QTimer::timeout, this, &RealtimeGeoPanel::rebuild);
    coalesceTimer_.setSingleShot(true);

    QTimer::singleShot(0, this, [this]{
        // 좌:표 / 우:차트 = 1:3
        if (split_) {
            const int w = std::max(1, split_->width());
            split_->setSizes({ w/4, w*3/4 });
        }
        applyColumnWidths();
    });
}

void RealtimeGeoPanel::buildUi()
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0,0,0,0);

    table_ = new QTableWidget(this);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setShowGrid(true);
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({ "Location","TEMP","VIB","INTR","HIGH","Last","RISK" });
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    seriesTemp_ = new QBarSet("TEMP");
    seriesVib_  = new QBarSet("VIB");
    seriesIntr_ = new QBarSet("INTR");
    barSeries_  = new QBarSeries();
    barSeries_->append(seriesTemp_);
    barSeries_->append(seriesVib_);
    barSeries_->append(seriesIntr_);
    barSeries_->setBarWidth(0.85);

    chart_ = new QChart();
    chart_->addSeries(barSeries_);
    chart_->setTitle("Counts by Type");
    chart_->legend()->setVisible(true);
    chart_->setAnimationOptions(QChart::SeriesAnimations);

    axX_ = new QBarCategoryAxis();
    axY_ = new QValueAxis(); axY_->setTitleText("Count"); axY_->setRange(0, 5);
    chart_->addAxis(axX_, Qt::AlignBottom);
    chart_->addAxis(axY_, Qt::AlignLeft);
    barSeries_->attachAxis(axX_); barSeries_->attachAxis(axY_);

    view_ = new QChartView(chart_); view_->setRenderHint(QPainter::Antialiasing);

    split_ = new QSplitter(Qt::Horizontal, this);
    split_->setChildrenCollapsible(false);
    split_->addWidget(table_);
    split_->addWidget(view_);
    split_->setStretchFactor(0, 1);   // 표 1
    split_->setStretchFactor(1, 3);   // 차트 3

    v->addWidget(split_);
}

QString RealtimeGeoPanel::nearestPlace(double lat, double lng) const
{
    QString best="Unknown"; double bestD=1e300;
    for (const auto& p : places_) {
        const double d2 = (lat-p.lat)*(lat-p.lat) + (lng-p.lng)*(lng-p.lng);
        if (d2 < bestD) { bestD = d2; best = p.name; }
    }
    return best;
}

void RealtimeGeoPanel::prepopulate()
{
    const int N = int(places_.size());
    table_->setRowCount(N);
    riskBars_.resize(N);

    for (int i=0;i<N;++i) {
        const QString& loc = places_[i].name;
        rowForLoc_[loc] = i;
        byLoc_[loc] = Agg{};

        auto setNum = [&](int col, int val){
            auto *it = new QTableWidgetItem(QString::number(val));
            it->setTextAlignment(Qt::AlignCenter);
            table_->setItem(i, col, it);
        };
        auto *nameIt = new QTableWidgetItem(loc);
        nameIt->setFlags(Qt::ItemIsEnabled);
        table_->setItem(i, COL_LOC, nameIt);
        setNum(COL_TEMP,0); setNum(COL_VIB,0); setNum(COL_INTR,0); setNum(COL_HIGH,0);

        auto *last = new QTableWidgetItem("-");
        last->setTextAlignment(Qt::AlignCenter);
        table_->setItem(i, COL_LAST, last);

        auto *bar = new QProgressBar(table_);
        bar->setRange(0,100); bar->setValue(0); bar->setTextVisible(true);
        bar->setFormat("%p%"); bar->setFixedHeight(18);
        bar->setStyleSheet(R"(
            QProgressBar {
                border:1px solid #e6e8ee; border-radius:9px; background:#f5f7fb;
                text-align:center; padding:0 4px; color:#0b1220; font-weight:600;
            }
            QProgressBar::chunk {
                border-radius:9px;
                background: qlineargradient(x1:0,y1:0, x2:1,y2:0,
                           stop:0 #6dd5a9, stop:0.5 #f2c94c, stop:1 #eb5757);
            }
        )");
        table_->setCellWidget(i, COL_RISK, bar);
        riskBars_[i] = bar;
    }

    QStringList cats;
    for (const auto& p : places_) cats << p.name;
    axX_->clear(); axX_->append(cats);

    seriesTemp_->remove(0, seriesTemp_->count());
    seriesVib_->remove(0,  seriesVib_->count());
    seriesIntr_->remove(0, seriesIntr_->count());
    for (int i=0;i<int(places_.size());++i) { *seriesTemp_ << 0; *seriesVib_ << 0; *seriesIntr_ << 0; }

    applyColumnWidths();
}

void RealtimeGeoPanel::ingestRows(int first, int last)
{
    if (!model_) return;
    for (int r = first; r <= last; ++r) {
        const SensorData& d = model_->at(r);
        const QString loc = nearestPlace(d.latitude, d.longitude);
        Agg &a = byLoc_[loc];
        if      (d.type == "TEMP") ++a.temp;
        else if (d.type == "VIB")  ++a.vib;
        else if (d.type == "INTR") ++a.intr;
        if (d.level == "HIGH") ++a.high;
        a.lastTs = std::max<qint64>(a.lastTs, d.timestamp);
    }
    if (!coalesceTimer_.isActive()) coalesceTimer_.start(60);
}

void RealtimeGeoPanel::rebuild()
{
    int ymax = 1;
    for (int i=0;i<int(places_.size());++i) {
        const QString loc = places_[i].name;
        const Agg a = byLoc_.value(loc, Agg{});
        auto setNum = [&](int col, int val){
            if (auto *it = table_->item(i, col)) it->setText(QString::number(val));
        };
        setNum(COL_TEMP, a.temp); setNum(COL_VIB, a.vib); setNum(COL_INTR, a.intr); setNum(COL_HIGH, a.high);
        if (auto *it = table_->item(i, COL_LAST)) {
            it->setText(a.lastTs>0 ? QDateTime::fromMSecsSinceEpoch(a.lastTs).toString("MM-dd HH:mm:ss") : "-");
        }
        const int total = a.temp + a.vib + a.intr;
        const int riskScore = a.high*2 + (total-a.high);
        const int pct = (total>0) ? qBound(0, int((riskScore * 100.0) / (total*2)), 100) : 0;
        if (auto *bar = riskBars_.value(i, nullptr)) {
            auto *anim = new QPropertyAnimation(bar, "value", bar);
            anim->setDuration(240);
            anim->setStartValue(bar->value());
            anim->setEndValue(pct);
            anim->setEasingCurve(QEasingCurve::InOutCubic);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
            bar->setToolTip(QString("Risk %1%  (T:%2 V:%3 I:%4, HIGH:%5)")
                                .arg(pct).arg(a.temp).arg(a.vib).arg(a.intr).arg(a.high));
        }
        ymax = std::max(ymax, std::max(a.temp, std::max(a.vib, a.intr)));
    }
    seriesTemp_->remove(0, seriesTemp_->count());
    seriesVib_->remove(0,  seriesVib_->count());
    seriesIntr_->remove(0, seriesIntr_->count());
    for (const auto& p : places_) {
        const Agg a = byLoc_.value(p.name, Agg{});
        *seriesTemp_ << a.temp; *seriesVib_ << a.vib; *seriesIntr_ << a.intr;
    }
    axY_->setRange(0, std::max(5, ymax + 2));

    applyColumnWidths();
}

void RealtimeGeoPanel::applyColumnWidths()
{
    if (!table_) return;
    auto *hdr = table_->horizontalHeader();
    const int W = table_->viewport()->width();
    if (W <= 0) return;

    // 원하는 느낌:
    //  - Location: "지금의 1/4" 수준으로 대폭 축소 → 전체의 ~12%
    //  - Risk: "1/2" 수준으로 축소 → 전체의 ~12~14%
    //  - 나머지(TEMP,VIB,INTR,HIGH,Last)가 공간을 균등 분할
    const int wLoc  = std::max(90, int(W * 0.12));
    const int wRisk = std::max(100, int(W * 0.13));

    // 남는 영역 균등분할
    const int restCount = 5; // TEMP,VIB,INTR,HIGH,Last
    const int restW = std::max(100, (W - wLoc - wRisk) / restCount);

    hdr->setSectionResizeMode(QHeaderView::Interactive); // 사용자 조절 가능
    hdr->resizeSection(COL_LOC,  wLoc);
    hdr->resizeSection(COL_RISK, wRisk);
    hdr->resizeSection(COL_TEMP, restW);
    hdr->resizeSection(COL_VIB,  restW);
    hdr->resizeSection(COL_INTR, restW);
    hdr->resizeSection(COL_HIGH, restW);
    hdr->resizeSection(COL_LAST, restW);
}

void RealtimeGeoPanel::forceRebuild() { rebuild(); }

void RealtimeGeoPanel::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    QTimer::singleShot(0, this, [this]{ applyColumnWidths(); });
}

void RealtimeGeoPanel::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    QTimer::singleShot(0, this, [this]{
        if (split_) {
            const int w = std::max(1, split_->width());
            split_->setSizes({ w/4, w*3/4 });
        }
        applyColumnWidths();
    });
}
