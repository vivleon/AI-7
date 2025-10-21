#include "LocationPopupDialog.h"
#include "LogTableModel.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QAbstractAxis>

#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QDateTime>
#include <QAbstractItemView>
#include <QPainter>
#include <QCheckBox>            // ← 여기에 포함
#include <limits>
#include <algorithm>
#include <QSignalBlocker>



// ── 장소 근사 테이블 (메인과 동일 후보)
struct Place { const char* name; double lat; double lng; };
static const Place kPlaces[] = {
    {"경복궁", 37.579617,126.977041}, {"창덕궁", 37.579414,126.991102},
    {"창경궁", 37.582604,126.994546}, {"덕수궁", 37.565804,126.975146},
    {"경희궁", 37.571785,126.959206}, {"경복궁-서문", 37.578645,126.976829},
    {"경복궁-북측", 37.579965,126.978345},{"창덕궁-서측", 37.581112,126.992230},
    {"덕수궁-동측", 37.564990,126.976000},{"경희궁-남측", 37.572500,126.960800},
};
static QString nearestPlace(double lat, double lng) {
    int best=-1; double bestD=std::numeric_limits<double>::infinity();
    const int N = int(sizeof(kPlaces)/sizeof(kPlaces[0]));
    for (int i=0;i<N;++i) {
        const double dx = lat - kPlaces[i].lat;
        const double dy = lng - kPlaces[i].lng;
        const double d2 = dx*dx + dy*dy;
        if (d2 < bestD) { bestD = d2; best = i; }
    }
    return best>=0 ? QString::fromUtf8(kPlaces[best].name) : QStringLiteral("Unknown");
}
static bool matchLocation(const QString& want, const QString& approxName) {
    return approxName == want;
}
static void autoRange(QLineSeries* s) {
    if (!s) return;

    const auto pts = s->points();        // ✅ Qt6 권장 API
    if (pts.isEmpty()) return;

    const qreal xmin = pts.first().x();
    const qreal xmax = pts.last().x();

    double ymin =  std::numeric_limits<double>::infinity();
    double ymax = -std::numeric_limits<double>::infinity();
    for (const auto& p : pts) {
        ymin = std::min(ymin, double(p.y()));
        ymax = std::max(ymax, double(p.y()));
    }
    if (!std::isfinite(ymin) || !std::isfinite(ymax)) return;

    const double pad = std::max(1.0, (ymax - ymin) * 0.10);

    // 시리즈에 "붙어있는" 축만 안전하게 조정
    for (QAbstractAxis* ax : s->attachedAxes()) {
        if (auto* axX = qobject_cast<QDateTimeAxis*>(ax)) {
            axX->setRange(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(xmin)),
                          QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(xmax)));
        } else if (auto* axY = qobject_cast<QValueAxis*>(ax)) {
            axY->setRange(ymin - pad, ymax + pad);
        }
    }
}



// ─────────────────────────────────────────────────────────────
// ctor
// ─────────────────────────────────────────────────────────────
LocationPopupDialog::LocationPopupDialog(const QString& location,
                                         LogTableModel* model,
                                         QWidget* parent)
    : QDialog(parent),
    loc_(location),
    model_(model)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral(u"📍 %1 - Details").arg(loc_));
    resize(980, 680);

    auto *root = new QVBoxLayout(this);

    // [A] 토글 패널 (TEMP/VIB/INTR만)
    {
        auto *panel = new QWidget(this);
        auto *g = new QGridLayout(panel);
        g->setContentsMargins(0,0,0,0);
        int r = 0;

        auto *title = new QLabel(tr("Sensors On/Off"), panel);
        QFont f = title->font(); f.setBold(true);
        title->setFont(f);
        g->addWidget(title, r++, 0, 1, 2);

        addRow(g, r++, QStringLiteral("TEMP"));
        addRow(g, r++, QStringLiteral("VIB"));
        addRow(g, r++, QStringLiteral("INTR"));

        auto *btnClose = new QPushButton(tr("Close"), panel);
        connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
        g->addWidget(btnClose, r, 1, Qt::AlignRight);

        root->addWidget(panel, 0);
    }

    // [B] 로그 표 — 멤버로 생성/보관
    tbl_ = new QTableWidget(this);
    tbl_->setColumnCount(6);
    tbl_->setHorizontalHeaderLabels({ "Time","SensorId","Type","Value","Level","Place" });
    tbl_->horizontalHeader()->setStretchLastSection(true);
    tbl_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl_->setAlternatingRowColors(true);
    root->addWidget(tbl_, 1);

    // [C] 하단 3차트
    auto make = [this]() {
        auto *chart  = new QChart();
        auto *series = new QLineSeries(chart);
        chart->addSeries(series);

        auto *axX = new QDateTimeAxis(chart);
        auto *axY = new QValueAxis(chart);
        axX->setFormat("HH:mm:ss");
        axY->setTitleText("Value");
        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        series->attachAxis(axX);
        series->attachAxis(axY);

        auto *view = new QChartView(chart, this);
        view->setRenderHint(QPainter::Antialiasing);
        view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        return std::pair<QLineSeries*, QChartView*>{series, view};
    };

    std::tie(sTemp_, vTemp_) = make();
    std::tie(sVib_,  vVib_)  = make();
    std::tie(sIntr_, vIntr_) = make();

    auto *h = new QHBoxLayout();
    h->setContentsMargins(0,0,0,0);
    h->setSpacing(8);
    h->addWidget(vTemp_, 1);
    h->addWidget(vVib_,  1);
    h->addWidget(vIntr_, 1);
    h->setStretch(0,1); h->setStretch(1,1); h->setStretch(2,1);   // ← 대칭 보장
    root->addLayout(h, 1);

    // ── 초기 로드 + 실시간 갱신
    if (model_) {
        const int n = model_->rowCount(QModelIndex());
        tbl_->setRowCount(0);
        for (int i=0; i<n; ++i)
            appendOne(model_->at(i));            // ← 멤버 함수 호출

        // ❌ Qt::QueuedConnection 제거 (기본 Auto = 같은 스레드면 Direct)
        connect(model_, &QAbstractItemModel::rowsInserted,
                this,   &LocationPopupDialog::onRowsInserted);
    }

    autoRange(sTemp_); autoRange(sVib_); autoRange(sIntr_);
}

// ── 초기 OFF 키 내려줄 때 호출(체크 해제)
void LocationPopupDialog::setInitialOffKeys(const QSet<QString>& offKeys)
{
    for (auto it = boxes_.begin(); it != boxes_.end(); ++it) {
        QSignalBlocker guard(it.value());          // 🔒 시그널 차단
        it.value()->setChecked(!offKeys.contains(it.key()));
    }
}


// 헤더와 구현 둘 다: const QString& → QString (값)
void LocationPopupDialog::addRow(QGridLayout* g, int row, QString key)
{
    auto *cb = new QCheckBox(key, this);
    cb->setChecked(true);
    g->addWidget(cb, row, 0);
    boxes_.insert(key, cb);

    // 안전: 람다 캡처에서 QString을 '값'으로 보관
    connect(cb, &QCheckBox::toggled, this, [this, key](bool on){
        if (key == "TEMP" && vTemp_) vTemp_->setVisible(on);
        if (key == "VIB"  && vVib_)  vVib_->setVisible(on);
        if (key == "INTR" && vIntr_) vIntr_->setVisible(on);
        emit sensorToggleChanged(loc_, key, on);
    });
}

void LocationPopupDialog::appendOne(const SensorData& d)
{
    const QString place = nearestPlace(d.latitude, d.longitude);
    if (!matchLocation(loc_, place)) return;
    if (!tbl_) return;

    const int row = tbl_->rowCount();
    tbl_->insertRow(row);
    tbl_->setItem(row,0,new QTableWidgetItem(QDateTime::fromMSecsSinceEpoch(d.timestamp).toString("yyyy-MM-dd HH:mm:ss")));
    tbl_->setItem(row,1,new QTableWidgetItem(d.sensorId));
    tbl_->setItem(row,2,new QTableWidgetItem(d.type));
    tbl_->setItem(row,3,new QTableWidgetItem(QString::number(d.value,'f',2)));
    tbl_->setItem(row,4,new QTableWidgetItem(d.level));
    tbl_->setItem(row,5,new QTableWidgetItem(place));

    if      (d.type=="TEMP") sTemp_->append(d.timestamp, d.value);
    else if (d.type=="VIB")  sVib_->append(d.timestamp, d.value);
    else if (d.type=="INTR") sIntr_->append(d.timestamp, d.value);
}

void LocationPopupDialog::onRowsInserted(const QModelIndex&, int first, int last)
{
    if (!model_) return;
    const int n = model_->rowCount(QModelIndex());
    first = std::max(0, first);
    last  = std::min(n-1, last);
    for (int r = first; r <= last; ++r)
        appendOne(model_->at(r));

    autoRange(sTemp_);
    autoRange(sVib_);
    autoRange(sIntr_);
}
