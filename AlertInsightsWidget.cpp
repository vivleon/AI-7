#include "AlertInsightsWidget.h"
#include "LogTableModel.h"
#include "LanguageManager.h"

#include <QVBoxLayout>
#include <QDateTime>
#include <QVector>
#include <QMap>
#include <algorithm>
#include <cmath>

namespace {
inline qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

static void styleLegend(QChart* chart) {
    if (!chart) return;
    auto *lg = chart->legend();
    lg->setVisible(true);
    lg->setAlignment(Qt::AlignTop);
    lg->setBackgroundVisible(false);
}

static QColor typeColor(const QString& t) {
    if (t=="TEMP") return QColor(46,204,113);   // green
    if (t=="VIB")  return QColor(52,152,219);   // blue
    return QColor(155,89,182);                  // purple (INTR)
}
}

AlertInsightsWidget::AlertInsightsWidget(QWidget* parent, LogTableModel* model)
    : QWidget(parent), model_(model)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10,10,10,10);
    root->setSpacing(10);

    // ── (1) 최근 12시간 타입별 타임라인 (막대)
    chartTimeline_ = new QChart();
    chartTimeline_->setTitle(LanguageManager::inst().t("Last 12 hours by Type"));
    setTemp_ = new QBarSet("TEMP"); setTemp_->setColor(typeColor("TEMP"));
    setVib_  = new QBarSet("VIB");  setVib_->setColor(typeColor("VIB"));
    setIntr_ = new QBarSet("INTR"); setIntr_->setColor(typeColor("INTR"));
    seriesTimeline_ = new QBarSeries();
    seriesTimeline_->append(setTemp_);
    seriesTimeline_->append(setVib_);
    seriesTimeline_->append(setIntr_);

    axX_ = new QBarCategoryAxis();
    axY_ = new QValueAxis(); axY_->setTitleText("Count"); axY_->setRange(0, 10);

    chartTimeline_->addSeries(seriesTimeline_);
    chartTimeline_->addAxis(axX_, Qt::AlignBottom);
    chartTimeline_->addAxis(axY_, Qt::AlignLeft);
    seriesTimeline_->attachAxis(axX_);
    seriesTimeline_->attachAxis(axY_);
    styleLegend(chartTimeline_);

    viewTimeline_ = new QChartView(chartTimeline_);
    viewTimeline_->setRenderHint(QPainter::Antialiasing);

    // ── (2) 심각도 도넛 (24h)
    chartSeverity_ = new QChart();
    chartSeverity_->setTitle(LanguageManager::inst().t("Severity (24h)"));
    pie_ = new QPieSeries();
    pie_->setHoleSize(0.55);
    chartSeverity_->addSeries(pie_);
    styleLegend(chartSeverity_);
    viewSeverity_ = new QChartView(chartSeverity_);
    viewSeverity_->setRenderHint(QPainter::Antialiasing);

    // 배치
    root->addWidget(viewTimeline_,  1);
    root->addWidget(viewSeverity_,  1);

    // 모델 반영(증분)
    if (model_) {
        connect(model_, &QAbstractItemModel::rowsInserted, this,
                [this](const QModelIndex&, int, int){
                    if (!coalesce_.isActive()) coalesce_.start(50);
                });
    }
    connect(&coalesce_, &QTimer::timeout, this, &AlertInsightsWidget::rebuild);
    coalesce_.setSingleShot(true);

    rebuild();
}

void AlertInsightsWidget::rebuild()
{
    if (!model_) return;

    // ── (1) 최근 12시간 카테고리
    const QDateTime now = QDateTime::currentDateTime();
    const int H = 12;
    QStringList cats; cats.reserve(H);
    QVector<int> cTemp(H,0), cVib(H,0), cIntr(H,0);

    // 시간 슬롯: [now-H+1, now] 시간별
    QList<QPair<qint64,qint64>> hourBins; hourBins.reserve(H);
    for (int i=H-1; i>=0; --i) {
        QDateTime h0 = now.addSecs(-3600*i).toUTC();
        h0.setTime(QTime(h0.time().hour(), 0, 0));
        QDateTime h1 = h0.addSecs(3600);
        hourBins.push_back({h0.toMSecsSinceEpoch(), h1.toMSecsSinceEpoch()-1});
        cats << now.addSecs(-3600*i).toString("HH:mm");
    }

    // 모델 순회
    for (int r=0; r<model_->rowCount(QModelIndex()); ++r) {
        const SensorData& d = model_->at(r);
        const qint64 ts = d.timestamp;
        // 어떤 시간 슬롯에 속하는지
        for (int k=0; k<H; ++k) {
            if (ts >= hourBins[k].first && ts <= hourBins[k].second) {
                if      (d.type=="TEMP") ++cTemp[k];
                else if (d.type=="VIB")  ++cVib[k];
                else if (d.type=="INTR") ++cIntr[k];
                break;
            }
        }
    }

    // 차트 업데이트
    axX_->clear(); axX_->append(cats);
    setTemp_->remove(0, setTemp_->count());
    setVib_->remove(0,  setVib_->count());
    setIntr_->remove(0, setIntr_->count());
    int ymax = 1;
    for (int k=0; k<H; ++k) {
        *setTemp_ << cTemp[k];
        *setVib_  << cVib[k];
        *setIntr_ << cIntr[k];
        ymax = std::max(ymax, std::max({cTemp[k], cVib[k], cIntr[k]}));
    }
    axY_->setRange(0, std::max(5, ymax + 1));

    // ── (2) 24h 심각도 도넛
    const qint64 t0 = now.addDays(-1).toMSecsSinceEpoch();
    int cHigh=0, cMed=0, cLow=0;
    for (int r=0; r<model_->rowCount(QModelIndex()); ++r) {
        const SensorData& d = model_->at(r);
        if (d.timestamp < t0) continue;
        if      (d.level=="HIGH")   ++cHigh;
        else if (d.level=="MEDIUM") ++cMed;
        else                        ++cLow;
    }
    pie_->clear();
    auto *sHigh = pie_->append("HIGH",   cHigh);
    auto *sMed  = pie_->append("MEDIUM", cMed);
    auto *sLow  = pie_->append("LOW",    cLow);

    // 가독성: HIGH는 약간 튀어나오게
    if (sHigh) sHigh->setExploded(true), sHigh->setLabelVisible(true);
    if (sMed)  sMed->setLabelVisible(true);
    if (sLow)  sLow->setLabelVisible(true);
}
