#include "MainDashboard.h"
#include "ui_MainDashboard.h"

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLegend>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QToolTip>
#include <QCursor>

#include <QCalendarWidget>
#include <QMessageBox>
#include <QHeaderView>
#include <QRandomGenerator>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMenu>
#include <QAction>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QTableView>
#include <QTableWidget>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QMenuBar>
#include <QInputDialog>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QStyle>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QApplication>
#include <QSettings>
#include <QPainter>
#include <QDateTimeEdit>
#include <QToolButton>
#include <QTimer>
#include <limits>
#include <optional>
#include <algorithm>
#include <array>
#include <cmath>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QEvent>
#include <QModelIndex>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QTabWidget>
#include <QSet>
#include <QSpacerItem>
#include <QFrame>
#include <type_traits>
#include <QDialog>
#include <QSplitter>

#include "DBBridge.h"
#include "LogTableModel.h"
#include "CustomFilterProxyModel.h"
#include "MapDialog.h"
#ifdef HAVE_AI_DIALOGS
#include "AIDialogs.h"
#endif
#include "UserMgmtDialog.h"
#include "AlertRulesDialog.h"
#include "LanguageManager.h"
#include "Thresholds.h"
#include "AuditLogDialog.h"
#include "SessionService.h"
#include "CurrentSessionsDialog.h"
#include "LocationPopupDialog.h"

namespace {
inline qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

static void styleCalendar(QDateTimeEdit* dt)
{
    if (!dt) return;
    dt->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    if (QCalendarWidget* cal = dt->calendarWidget()) {
        cal->setStyleSheet(R"(
            QCalendarWidget {
                background: #ffffff; color: #111;
                border: 1px solid rgba(0,0,0,0.15);
                border-radius: 8px;
            }
            QCalendarWidget QWidget { background: #ffffff; color: #111; }
            QCalendarWidget QToolButton {
                background: #f6f7fb; color: #111;
                border: 1px solid #e6e8ee; border-radius: 6px;
                padding: 4px 8px;
            }
            QCalendarWidget QToolButton::hover { background: #ffffff; }
            QCalendarWidget QMenu {
                background: #ffffff; color: #111;
                border: 1px solid #e6e8ee; border-radius: 6px;
            }
            QCalendarWidget QAbstractItemView {
                background: #ffffff; color: #111;
                selection-background-color: rgba(74,144,226,0.20);
                selection-color: #111; outline: none;
                gridline-color: #eaecef;
            }
        )");
        QPalette pal = cal->palette();
        pal.setColor(QPalette::Base, Qt::white);
        pal.setColor(QPalette::Text, QColor("#111111"));
        pal.setColor(QPalette::ButtonText, QColor("#111111"));
        cal->setPalette(pal);
    }
}

static void stylizeSeries(QLineSeries* s, const QColor& c)
{ QPen p(c); p.setWidthF(2.0); s->setPen(p); s->setPointsVisible(true); }

static void setupAxes(QDateTimeAxis* ax, QValueAxis* ay, qint64 windowMs)
{
    auto& L = LanguageManager::inst();
    ax->setTitleText(L.t("Time"));
    ax->setFormat("HH:mm:ss");
    ax->setTickCount(8);
    ay->setTitleText(L.t("Value"));
    ay->setMinorTickCount(4);
    const qint64 n = nowMs();
    ax->setRange(QDateTime::fromMSecsSinceEpoch(n - windowMs),
                 QDateTime::fromMSecsSinceEpoch(n));
}

static QLineSeries* makeGuide(const QColor& c)
{
    auto* g = new QLineSeries();
    QPen pen(c); pen.setStyle(Qt::DashLine); pen.setWidthF(1.5);
    g->setPen(pen);
    return g;
}

static void styleLegend(QChart* chart)
{
    if (!chart) return;
    auto *lg = chart->legend();
    lg->setVisible(true);
    lg->setAlignment(Qt::AlignTop);
    lg->setBackgroundVisible(false);
    lg->setMarkerShape(QLegend::MarkerShapeFromSeries);
    QFont f = lg->font(); f.setPointSize(std::max(9, f.pointSize()-1));
    lg->setFont(f);
    lg->setLabelColor(QColor("#0b1220"));
}
} // namespace

// ─────────────────────── Realtime Geo (Dialog + Panel 공용) ───────────────────────
template <class Base>
class RealtimeGeoBase : public Base {
public:
    explicit RealtimeGeoBase(QWidget* parent, LogTableModel* model)
        : Base(parent), model_(model)
    {
        if constexpr (std::is_base_of_v<QDialog, Base>) {
            this->setAttribute(Qt::WA_DeleteOnClose);
            this->setWindowTitle("🗺️ Realtime Geo Dashboard");
            this->resize(1000, 680);
        }
        auto *v = new QVBoxLayout(this);

        // 표
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
        table_->horizontalHeader()->setStretchLastSection(false);          // ← 고정폭 제어
        table_->setStyleSheet(R"(
            QTableWidget { background:white; gridline-color:#edf0f6; font-size:12px; }
            QHeaderView::section {
                background:#f7f9fc; border:1px solid #edf0f6; padding:6px; font-weight:600;
            }
        )");

        QObject::connect(table_, &QTableWidget::cellDoubleClicked, this,
                         [this](int row, int /*col*/) {
                             if (row < 0 || row >= (int)places_.size()) return;
                             const auto& p = places_[row];

                             // 장소명으로 상세 팝업 (토글 + 표 + TEMP/VIB/INTR 차트)
                             auto *dlg = new LocationPopupDialog(p.name, model_, this);
                             dlg->setModal(false);

                             // (선택) 장소별 OFF 키를 기억했다면 여기서 내려줄 수 있습니다.
                             // QSet<QString> off = /* ... load from settings per place ... */;
                             // dlg->setInitialOffKeys(off);

                             // (선택) 토글 변경 시 메인에 통지
                             QObject::connect(dlg, &LocationPopupDialog::sensorToggleChanged, this,
                                              [this](const QString& place, const QString& key, bool on) {
                                                  // 여기서 place/key별 동작을 정의하세요.
                                                  // 예: statusBar() 메시지, per-place 필터링, 저장 등
                                                  if (auto *mw = qobject_cast<QMainWindow*>(this->window())) {
                                                      mw->statusBar()->showMessage(
                                                          QString("[%1] %2 → %3").arg(place, key, on ? "ON" : "OFF"), 3000);
                                                  }
                                              });

                             dlg->show(); dlg->raise(); dlg->activateWindow();
                         });

        // 막대차트
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

        // 좌:표 / 우:차트 — 드래그로 폭 조절 가능하도록 QSplitter 사용
        auto *split = new QSplitter(Qt::Horizontal, this);
        split->setChildrenCollapsible(false);
        split->setHandleWidth(8);
        split->addWidget(table_);
        split->addWidget(view_);

        // ✅ 1:1 기본 비율
        split->setStretchFactor(0, 1);
        split->setStretchFactor(1, 1);

        split->setSizes({1, 1});

        QTimer::singleShot(0, split, [split]{
            const int w = split->width() > 0 ? split->width() : 800;
            split->setSizes({ w/2, w/2 });
        });

        // prepopulate() 말미의 헤더 설정을 아래처럼 정리
        auto* hh = table_->horizontalHeader();
        hh->setStretchLastSection(false);
        hh->setSectionResizeMode(COL_LOC,  QHeaderView::Fixed);
        hh->setSectionResizeMode(COL_RISK, QHeaderView::Fixed);
        for (int c : {COL_TEMP, COL_VIB, COL_INTR, COL_HIGH, COL_LAST})
            hh->setSectionResizeMode(c, QHeaderView::Stretch);
        hh->resizeSection(COL_LOC, 160);
        hh->resizeSection(COL_RISK, 200);



        // 표 자체도 확장 정책
        table_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        // 필요시: 초기 한 번
        table_->resizeColumnsToContents();

        // ★ 마지막 컬럼 스트레치 끄기: ...
        table_->horizontalHeader()->setStretchLastSection(false);

        // ★ Location / Risk 컬럼 폭 및 리사이즈 모드 재설정
        table_->horizontalHeader()->setSectionResizeMode(COL_LOC,  QHeaderView::Interactive);
        table_->horizontalHeader()->setSectionResizeMode(COL_RISK, QHeaderView::Fixed);

        // ‘Location 폭을 지금의 1/4 수준으로’ ...
        table_->setColumnWidth(COL_LOC, 160);
        table_->setColumnWidth(COL_RISK, 100);




        if constexpr (std::is_base_of_v<QDialog, Base>) {
            v->addWidget(split);
        } else {
            v->setContentsMargins(0,0,0,0);
            v->addWidget(split);
        }

        prepopulate();

        if (model_) {
            QObject::connect(model_, &QAbstractItemModel::rowsInserted, this,
                             [this](const QModelIndex&, int first, int last){ ingestRows(first, last); });
            const int rc = model_->rowCount(QModelIndex());
            if (rc > 0) ingestRows(0, rc - 1);
        }
        QObject::connect(&coalesceTimer_, &QTimer::timeout, this, &RealtimeGeoBase::rebuild);
        coalesceTimer_.setSingleShot(true);
    }

    void forceRebuild() { rebuild(); }

private:
    struct Place { QString name; double lat; double lng; };
    const std::vector<Place> places_ = {
        {"경복궁", 37.579617,126.977041}, {"창덕궁", 37.579414,126.991102},
        {"창경궁", 37.582604,126.994546}, {"덕수궁", 37.565804,126.975146},
        {"경희궁", 37.571785,126.959206}, {"경복궁-서문", 37.578645,126.976829},
        {"경복궁-북측", 37.579965,126.978345},{"창덕궁-서측", 37.581112,126.992230},
        {"덕수궁-동측", 37.564990,126.976000},{"경희궁-남측", 37.572500,126.960800}
    };
    static constexpr int COL_LOC=0, COL_TEMP=1, COL_VIB=2, COL_INTR=3, COL_HIGH=4, COL_LAST=5, COL_RISK=6;

    struct Agg { int temp=0, vib=0, intr=0, high=0; qint64 lastTs=0; };
    QMap<QString, Agg> byLoc_;
    QMap<QString, int> rowForLoc_;
    QVector<QProgressBar*> riskBars_;

    LogTableModel* model_ = nullptr;
    QTableWidget* table_ = nullptr;
    QChart* chart_ = nullptr;
    QChartView* view_ = nullptr;
    QBarSet* seriesTemp_ = nullptr;
    QBarSet* seriesVib_  = nullptr;
    QBarSet* seriesIntr_ = nullptr;
    QBarSeries* barSeries_ = nullptr;
    QBarCategoryAxis* axX_ = nullptr;
    QValueAxis* axY_ = nullptr;
    QTimer coalesceTimer_;

    QString nearestPlace(double lat, double lng) const {
        QString best="Unknown"; double bestD=1e300;
        for (const auto& p : places_) {
            const double d2 = (lat-p.lat)*(lat-p.lat) + (lng-p.lng)*(lng-p.lng);
            if (d2 < bestD) { bestD = d2; best = p.name; }
        }
        return best;
    }

    void prepopulate() {
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

        // ── 열 폭/모드 조정: Location 1/4 수준으로 축소, Risk 절반 수준으로 축소
        auto *hh = table_->horizontalHeader();
        hh->setSectionResizeMode(COL_LOC,  QHeaderView::Fixed);
        hh->setSectionResizeMode(COL_TEMP, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(COL_VIB,  QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(COL_INTR, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(COL_HIGH, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(COL_LAST, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(COL_RISK, QHeaderView::Fixed);

        // 고정 폭 값(대략 기존 대비 1/4, 1/2 감각에 맞춰 보수적으로 설정)
        const int locW  = 120;     // Location
        const int riskW = 100;     // Risk bar
        hh->resizeSection(COL_LOC,  locW);
        hh->resizeSection(COL_RISK, riskW);

        // 나머지는 내용에 맞게
        table_->resizeColumnsToContents();

        // 막대차트 카테고리
        QStringList cats;
        for (const auto& p : places_) cats << p.name;
        axX_->clear(); axX_->append(cats);

        seriesTemp_->remove(0, seriesTemp_->count());
        seriesVib_->remove(0,  seriesVib_->count());
        seriesIntr_->remove(0, seriesIntr_->count());
        for (int i=0;i<N;++i) { *seriesTemp_ << 0; *seriesVib_ << 0; *seriesIntr_ << 0; }
    }

    void ingestRows(int first, int last) {
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

    void rebuild() {
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
    }
};

// ───────────────────────── ctor ─────────────────────────
MainDashboard::MainDashboard(DBBridge* db_,
                             const QString& loginUser,
                             Role loginRole,
                             QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainDashboard),
      worker(new ThreadWorker(this)),
      db(db_),
      currentUser(loginUser),
      currentRole(loginRole)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("VigilEdge - %1 (%2)")
                        .arg(loginUser, roleToString(loginRole)));
    // ‘좀비 세션’ 정리
    {
        QSettings s("VigilEdge","VigilEdge");
        const qint64 zombieId = s.value("session/id", 0).toLongLong();
        const QString zombieTk = s.value("session/token").toString();
        if (zombieId > 0 && !zombieTk.isEmpty()) {
            sessService.endSession(zombieId, zombieTk);
            s.remove("session/id"); s.remove("session/token");
        }
    }

    // 상태표시줄 권한 라벨
    lblRole = new QLabel(this);
    statusBar()->addPermanentWidget(lblRole);
    updateStatusBarRole();

    configureTablesAndStyle();
    applyGlassTheme();
    configureCharts();
    configureOperatorLayoutStretch();

    // ★ 메인보드(홈) 생성 & 기본 랜딩으로 설정
    buildMainBoard();
    ensureDefaultLandingPage();

    // 기간 기본값 / 캘린더 스타일
    {
        const QDateTime now = QDateTime::currentDateTime();
        ui->dtTo->setDateTime(now.addDays(+1));
        ui->dtFrom->setDateTime(now.addYears(-1));
        styleCalendar(ui->dtFrom);
        styleCalendar(ui->dtTo);
        aiTo = now; aiFrom = now.addDays(-1);
    }

    // Role 메뉴
    connect(ui->actionOperator, &QAction::triggered, this, &MainDashboard::setRoleOperator);
    connect(ui->actionAnalyst,  &QAction::triggered, this, &MainDashboard::setRoleAnalyst);
    connect(ui->actionAdmin,    &QAction::triggered, this, &MainDashboard::setRoleAdmin);
    connect(ui->actionViewer,   &QAction::triggered, this, &MainDashboard::setRoleViewer);

    // 상단 메뉴
    buildMenusAndToolbar();
    removeFilterSensorButtons();

    // 버튼
    connect(ui->btnTempOn,  &QPushButton::clicked, this, &MainDashboard::tempOn);
    connect(ui->btnTempOff, &QPushButton::clicked, this, &MainDashboard::tempOff);
    connect(ui->btnVibOn,   &QPushButton::clicked, this, &MainDashboard::vibOn);
    connect(ui->btnVibOff,  &QPushButton::clicked, this, &MainDashboard::vibOff);
    connect(ui->btnIntrOn,  &QPushButton::clicked, this, &MainDashboard::intrOn);
    connect(ui->btnIntrOff, &QPushButton::clicked, this, &MainDashboard::intrOff);
    beautifyButtons();

    // 필터/내보내기
    connect(ui->btnApplyFilter, &QPushButton::clicked, this, &MainDashboard::applyFilter);
    connect(ui->btnResetFilter, &QPushButton::clicked, this, &MainDashboard::resetFilter);
    if (ui->btnExport) {
        auto& L = LanguageManager::inst();
        QMenu *menu = new QMenu(ui->btnExport);
        QAction *actCsv = menu->addAction(L.t("Export CSV"));
        QAction *actTxt = menu->addAction(L.t("Export TXT"));
        ui->btnExport->setMenu(menu);
        connect(actCsv, &QAction::triggered, this, &MainDashboard::exportCSV);
        connect(actTxt, &QAction::triggered, this, &MainDashboard::exportTXT);
    }

    // 로그 더블클릭 → 지도 (기능 유지)
    connect(ui->logView, &QTableView::doubleClicked, this, &MainDashboard::onLogDoubleClicked);

    // 더미 데이터 타이머
    seedDummyPool();
    connect(&dummyTimer, &QTimer::timeout, this, &MainDashboard::tickDummy);
    dummyTimer.start(1000);

    // SuperAdmin: 암호 초기화
    if (ui->btnResetPwd)
        connect(ui->btnResetPwd, &QPushButton::clicked, this, &MainDashboard::resetPassword);

    applyRoleUI();

    // 언어/임계치 초기 적용
    {
        QSettings s("VigilEdge","VigilEdge");
        const QString lang = s.value("ui/lang","system").toString();
        if (lang=="ko") LanguageManager::inst().set(LanguageManager::Lang::Korean);
        else if (lang=="en") LanguageManager::inst().set(LanguageManager::Lang::English);
        else LanguageManager::inst().set(LanguageManager::Lang::System);

        connect(&LanguageManager::inst(), &LanguageManager::languageChanged,
                this, &MainDashboard::onLanguageChanged);

        reloadThresholdsAndApply();
        applyLanguageTexts();
    }

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]{
        if (sessionId > 0 && !sessionToken.isEmpty())
            sessService.endSession(sessionId, sessionToken);
    });

    statusBar()->showMessage(LanguageManager::inst().t("Dashboard initialized"));
}

MainDashboard::~MainDashboard()
{
    stopHeartbeat();
    delete ui;
}

void MainDashboard::onLanguageChanged()
{
    menuBar()->clear();
    buildMenusAndToolbar();
    rebuildExportMenu();
    applyLanguageTexts();
    updateCornerToggleTexts();
}

void MainDashboard::rebuildExportMenu()
{
    if (!ui->btnExport) return;
    auto& L = LanguageManager::inst();
    auto *menu = new QMenu(ui->btnExport);
    QAction *actCsv = menu->addAction(L.t("Export CSV"));
    QAction *actTxt = menu->addAction(L.t("Export TXT"));
    ui->btnExport->setMenu(menu);
    connect(actCsv, &QAction::triggered, this, &MainDashboard::exportCSV);
    connect(actTxt, &QAction::triggered, this, &MainDashboard::exportTXT);
}

void MainDashboard::updateCornerToggleTexts()
{
    auto& L = LanguageManager::inst();
    auto set = [&](QToolButton* b, const QString& keyName){
        if (!b) return;
        const QString name = L.t(keyName);
        const QString onoff = L.t(b->isChecked()? "ON" : "OFF");
        b->setText(name + " " + onoff);
        b->setToolTip(name + " " + (b->isChecked()? L.t("ON") : L.t("OFF")));
    };
    set(tbTemp, "TEMP");
    set(tbVib,  "VIB");
    set(tbIntr, "INTR");
}

// ───────────────────────── Glass 톤 ─────────────────────────
void MainDashboard::applyGlassTheme()
{
    const QString css = R"(
      QMainWindow {
        background: qlineargradient(x1:0,y1:0,x2:1,y2:1,
          stop:0 rgba(246,249,255,0.92), stop:1 rgba(235,240,250,0.92));
      }
      QMenuBar {
        background: rgba(255,255,255,0.55);
        border-bottom: 1px solid rgba(210,220,245,0.7);
      }
      QMenuBar::item {
        padding: 6px 10px; margin: 2px 4px; border-radius: 10px;
        color:#0b1220; font-size: 12px;
      }
      QMenuBar::item:selected { background: rgba(74,144,226,0.16); }
      QMenuBar::item:pressed  { background: rgba(74,144,226,0.26); }
      QMenu {
        background: rgba(255,255,255,0.92);
        border: 1px solid rgba(0,0,0,0.06);
        border-radius: 12px; padding: 6px;
      }
      QMenu::separator { height: 1px; background: rgba(0,0,0,0.06); margin: 6px 8px; }
      QMenu::item { padding: 6px 10px; border-radius: 8px; color:#0b1220; }
      QMenu::item:selected { background: rgba(74,144,226,0.16); }
      QStatusBar {
        background: rgba(255,255,255,0.50);
        border-top: 1px solid rgba(210,220,245,0.7);
      }
    )";
    setStyleSheet(css);

    auto addShadow = [](QWidget* w){
        if (!w) return;
        auto *shadow = new QGraphicsDropShadowEffect(w);
        shadow->setBlurRadius(32.0); shadow->setOffset(0, 12);
        shadow->setColor(QColor(0,0,0,28));
        w->setGraphicsEffect(shadow);
    };
    addShadow(ui->gbCharts);
    addShadow(ui->gbFilter);
    addShadow(ui->gbUsers);
}

void MainDashboard::updateStatusToggles()
{
    auto& L = LanguageManager::inst();
    statusBar()->showMessage(
        QString("TEMP %1 • VIB %2 • INTR %3")
            .arg(L.t(enableTemp ? "ON" : "OFF"))
            .arg(L.t(enableVib  ? "ON" : "OFF"))
            .arg(L.t(enableIntr ? "ON" : "OFF")));
}
void MainDashboard::updateStatusBarRole()
{
    if (!lblRole) return;
    auto& L = LanguageManager::inst();
    lblRole->setText(QString("%1: %2  |  %3: %4")
                         .arg(L.t("User"), currentUser, L.t("Role"), L.t(roleToString(currentRole))));
}
void MainDashboard::removeFilterSensorButtons()
{
    if (!ui->gbFilter) return;
    std::array<QPushButton*,6> btns{
        ui->btnTempOn, ui->btnTempOff,
        ui->btnVibOn,  ui->btnVibOff,
        ui->btnIntrOn, ui->btnIntrOff
    };
    if (QLayout* lay = ui->gbFilter->layout()) {
        for (auto* b : btns) {
            if (!b) continue;
            b->hide();
            lay->removeWidget(b);
        }
    }
}

// 언어 메뉴/적용
void MainDashboard::buildLanguageMenu(QMenu* mFile) {
    QMenu* mLang = mFile->addMenu(LanguageManager::inst().t("Language"));
    QAction* aSys = mLang->addAction(LanguageManager::inst().t("System Default"));
    QAction* aKo  = mLang->addAction(LanguageManager::inst().t("Korean"));
    QAction* aEn  = mLang->addAction(LanguageManager::inst().t("English"));
    connect(aSys,&QAction::triggered,this,&MainDashboard::onLangSystem);
    connect(aKo, &QAction::triggered,this,&MainDashboard::onLangKorean);
    connect(aEn, &QAction::triggered,this,&MainDashboard::onLangEnglish);
}
void MainDashboard::onLangSystem(){ QSettings s("VigilEdge","VigilEdge"); s.setValue("ui/lang","system"); LanguageManager::inst().set(LanguageManager::Lang::System); }
void MainDashboard::onLangKorean(){ QSettings s("VigilEdge","VigilEdge"); s.setValue("ui/lang","ko"); LanguageManager::inst().set(LanguageManager::Lang::Korean); }
void MainDashboard::onLangEnglish(){ QSettings s("VigilEdge","VigilEdge"); s.setValue("ui/lang","en"); LanguageManager::inst().set(LanguageManager::Lang::English); }

void MainDashboard::applyLanguageTexts()
{
    auto& L = LanguageManager::inst();
    const auto acts = menuBar()->actions();
    if (acts.size() > 0) acts[0]->setText(L.t("File"));
    if (acts.size() > 1) acts[1]->setText(L.t("View"));
    if (acts.size() > 2) acts[2]->setText(L.t("AI"));
    if (acts.size() > 3) acts[3]->setText(L.t("Admin"));

    if (chartTemp) chartTemp->setTitle(L.t("Temperature"));
    if (chartVib)  chartVib->setTitle(L.t("Vibration"));
    if (chartIntr) chartIntr->setTitle(L.t("Intrusion"));

    if (seriesTemp) seriesTemp->setName(L.t("Value"));
    if (seriesVib)  seriesVib->setName(L.t("Value"));
    if (seriesIntr) seriesIntr->setName(L.t("Value"));

    if (guideTempWarn) guideTempWarn->setName(QString("%1 (%2)").arg(L.t("Warning")).arg(thrTemp_.warn));
    if (guideTempHigh) guideTempHigh->setName(QString("%1 (%2)").arg(L.t("High")).arg(thrTemp_.high));
    if (guideVibWarn)  guideVibWarn->setName(QString("%1 (%2)").arg(L.t("Warning")).arg(thrVib_.warn));
    if (guideVibHigh)  guideVibHigh->setName(QString("%1 (%2)").arg(L.t("High")).arg(thrVib_.high));
    if (guideIntrWarn) guideIntrWarn->setName(QString("%1 (%2)").arg(L.t("Warning")).arg(thrIntr_.warn));
    if (guideIntrHigh) guideIntrHigh->setName(QString("%1 (%2)").arg(L.t("High")).arg(thrIntr_.high));

    if (modelLogs) modelLogs->notifyLanguageChanged();
    updateStatusBarRole();
}

void MainDashboard::prepViewerHome()
{
    if (!ui->pageViewer) return;
    if (viewerLogo) return;

    if (!ui->pageViewer->layout())
        ui->pageViewer->setLayout(new QVBoxLayout(ui->pageViewer));
    auto *lay = qobject_cast<QVBoxLayout*>(ui->pageViewer->layout());
    while (QLayoutItem* it = lay->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }

    viewerLogoBase = QPixmap(":/img/logo.png");
    if (viewerLogoBase.isNull()) {
        QPixmap tmp(900, 220); tmp.fill(Qt::transparent);
        QPainter p(&tmp);
        QFont f(qApp->font()); f.setPointSize(72); f.setBold(true);
        p.setRenderHint(QPainter::Antialiasing);
        p.setFont(f);
        p.setPen(QColor(0,0,0,28));
        p.drawText(tmp.rect(), Qt::AlignCenter, "VigilEdge");
        p.end();
        viewerLogoBase = tmp;
    }

    viewerLogo = new QLabel(ui->pageViewer);
    viewerLogo->setAlignment(Qt::AlignCenter);
    viewerLogo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *op = new QGraphicsOpacityEffect(viewerLogo);
    op->setOpacity(0.18);
    viewerLogo->setGraphicsEffect(op);

    lay->addStretch();
    lay->addWidget(viewerLogo, 0, Qt::AlignCenter);
    lay->addStretch();

    ui->pageViewer->setStyleSheet("QWidget#pageViewer { background: transparent; }");

    ui->pageViewer->installEventFilter(this);
    updateViewerLogo();
}

void MainDashboard::updateViewerLogo()
{
    if (!viewerLogo || viewerLogoBase.isNull()) return;
    const QSize area = ui->pageViewer->size();
    const QSize target = QSize(int(area.width()*0.45), int(area.height()*0.22));
    viewerLogo->setPixmap(viewerLogoBase.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

bool MainDashboard::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == ui->pageViewer && ev->type() == QEvent::Resize) {
        updateViewerLogo();
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainDashboard::configureTablesAndStyle()
{
    modelLogs = new LogTableModel(this);
    proxyLogs = new CustomFilterProxyModel(this);
    proxyLogs->setSourceModel(modelLogs);
    proxyLogs->setDynamicSortFilter(true);

    ui->logView->setModel(proxyLogs);
    ui->logView->setAlternatingRowColors(true);
    ui->logView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->logView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->logView->setSortingEnabled(true);
    ui->logView->horizontalHeader()->setStretchLastSection(true);
    ui->logView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);          // Value
    ui->logView->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents); // Lat
    ui->logView->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents); // Lng
    // 🔽 Create a Location combo if it doesn't exist in the .ui
    if (!comboLocation) {
        // if the .ui is later updated with a combo named "comboLocation", pick it up:
        comboLocation = this->findChild<QComboBox*>("comboLocation");
        if (!comboLocation) {
            comboLocation = new QComboBox(ui->gbFilter);
            comboLocation->setObjectName("comboLocation");
            comboLocation->addItem("All");
            if (QLayout* lay = ui->gbFilter ? ui->gbFilter->layout() : nullptr) {
                lay->addWidget(new QLabel(LanguageManager::inst().t("Location"), ui->gbFilter));
                lay->addWidget(comboLocation);
            }
        }
        connect(comboLocation, &QComboBox::currentTextChanged, this, [this](const QString& s){
            if (proxyLogs) proxyLogs->setLocationFilter(s);
        });
    }

}

void MainDashboard::tuneTimeAxis(QDateTimeAxis* ax)
{
    if (!ax) return;
    ax->setFormat("HH:mm:ss");
    ax->setTickCount(8);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 2, 0))
    ax->setLabelsAngle(-45);
#endif
    ax->setLabelsVisible(true);
}

void MainDashboard::configureCharts()
{
    auto makeScatter = [](const QColor& c){
        auto *s = new QScatterSeries();
        s->setMarkerSize(8.0);
        QPen p = s->pen(); p.setWidthF(1.0); p.setColor(c.darker(120));
        s->setPen(p);
        s->setBrush(c);
        return s;
    };

    auto wireScatter = [&](QScatterSeries* sc){
        connect(sc, &QScatterSeries::hovered, this, &MainDashboard::onScatterHovered);
        connect(sc, &QScatterSeries::clicked, this, &MainDashboard::onScatterClicked);
    };

    // TEMP
    seriesTemp = new QLineSeries(this); stylizeSeries(seriesTemp, QColor(46,204,113));
    seriesTemp->setName("Value");
    scatterTempWarn = makeScatter(QColor(241,196,15)); scatterTempWarn->setName("Warn"); wireScatter(scatterTempWarn);
    scatterTempHigh = makeScatter(QColor(231,76,60));  scatterTempHigh->setName("High"); wireScatter(scatterTempHigh);

    chartTemp = new QChart(); chartTemp->setTheme(QChart::ChartThemeLight);
    chartTemp->setTitle("Temperature");
    chartTemp->addSeries(seriesTemp);
    chartTemp->addSeries(scatterTempWarn);
    chartTemp->addSeries(scatterTempHigh);

    axisXTemp = new QDateTimeAxis(this); axisYTemp = new QValueAxis(this);
    setupAxes(axisXTemp, axisYTemp, WINDOW_MS); tuneTimeAxis(axisXTemp);
    chartTemp->addAxis(axisXTemp, Qt::AlignBottom); chartTemp->addAxis(axisYTemp, Qt::AlignLeft);
    for (auto s : chartTemp->series()) { s->attachAxis(axisXTemp); s->attachAxis(axisYTemp); }
    guideTempWarn = makeGuide(QColor(241,196,15)); guideTempHigh = makeGuide(QColor(231,76,60));
    guideTempWarn->setName("Warning (auto)"); guideTempHigh->setName("High (auto)");
    chartTemp->addSeries(guideTempWarn); chartTemp->addSeries(guideTempHigh);
    guideTempWarn->attachAxis(axisXTemp); guideTempHigh->attachAxis(axisXTemp);
    guideTempWarn->attachAxis(axisYTemp); guideTempHigh->attachAxis(axisYTemp);
    styleLegend(chartTemp);
    ui->chartTemp->setRenderHint(QPainter::Antialiasing); ui->chartTemp->setChart(chartTemp);

    // VIB
    seriesVib = new QLineSeries(this); stylizeSeries(seriesVib, QColor(52,152,219));
    seriesVib->setName("Value");
    scatterVibWarn = makeScatter(QColor(241,196,15)); scatterVibWarn->setName("Warn"); wireScatter(scatterVibWarn);
    scatterVibHigh = makeScatter(QColor(231,76,60));  scatterVibHigh->setName("High"); wireScatter(scatterVibHigh);

    chartVib = new QChart(); chartVib->setTheme(QChart::ChartThemeLight);
    chartVib->setTitle("Vibration");
    chartVib->addSeries(seriesVib); chartVib->addSeries(scatterVibWarn); chartVib->addSeries(scatterVibHigh);
    axisXVib = new QDateTimeAxis(this); axisYVib = new QValueAxis(this);
    setupAxes(axisXVib, axisYVib, WINDOW_MS); tuneTimeAxis(axisXVib);
    chartVib->addAxis(axisXVib, Qt::AlignBottom); chartVib->addAxis(axisYVib, Qt::AlignLeft);
    for (auto s : chartVib->series()) { s->attachAxis(axisXVib); s->attachAxis(axisYVib); }
    guideVibWarn = makeGuide(QColor(241,196,15)); guideVibHigh = makeGuide(QColor(231,76,60));
    guideVibWarn->setName("Warning (auto)"); guideVibHigh->setName("High (auto)");
    chartVib->addSeries(guideVibWarn); chartVib->addSeries(guideVibHigh);
    guideVibWarn->attachAxis(axisXVib); guideVibHigh->attachAxis(axisXVib);
    guideVibWarn->attachAxis(axisYVib);  guideVibHigh->attachAxis(axisYVib);
    styleLegend(chartVib);
    ui->chartVib->setRenderHint(QPainter::Antialiasing); ui->chartVib->setChart(chartVib);

    // INTR
    seriesIntr = new QLineSeries(this);  stylizeSeries(seriesIntr, QColor(155,89,182));
    seriesIntr->setName("Value");
    scatterIntrWarn = makeScatter(QColor(241,196,15)); scatterIntrWarn->setName("Warn"); wireScatter(scatterIntrWarn);
    scatterIntrHigh = makeScatter(QColor(231,76,60));  scatterIntrHigh->setName("High"); wireScatter(scatterIntrHigh);

    chartIntr = new QChart();  chartIntr->setTheme(QChart::ChartThemeLight);
    chartIntr->setTitle("Intrusion");
    chartIntr->addSeries(seriesIntr);
    chartIntr->addSeries(scatterIntrWarn);
    chartIntr->addSeries(scatterIntrHigh);

    axisXIntr = new QDateTimeAxis(this); axisYIntr = new QValueAxis(this);
    setupAxes(axisXIntr, axisYIntr, WINDOW_MS); tuneTimeAxis(axisXIntr);
    chartIntr->addAxis(axisXIntr, Qt::AlignBottom); chartIntr->addAxis(axisYIntr, Qt::AlignLeft);
    for (auto s : chartIntr->series()) { s->attachAxis(axisXIntr); s->attachAxis(axisYIntr); }

    guideIntrWarn = makeGuide(QColor(241,196,15)); guideIntrHigh = makeGuide(QColor(231,76,60));
    guideIntrWarn->setName("Warning (auto)"); guideIntrHigh->setName("High (auto)");
    chartIntr->addSeries(guideIntrWarn); chartIntr->addSeries(guideIntrHigh);
    guideIntrWarn->attachAxis(axisXIntr); guideIntrHigh->attachAxis(axisXIntr);
    guideIntrWarn->attachAxis(axisYIntr); guideIntrHigh->attachAxis(axisYIntr);
    styleLegend(chartIntr);
    ui->chartIntr->setRenderHint(QPainter::Antialiasing); ui->chartIntr->setChart(chartIntr);

    auto initGuides = [&](QLineSeries* g1, QLineSeries* g2){
        const qint64 t = nowMs();
        g1->replace({ QPointF(t - WINDOW_MS, 50.0), QPointF(t, 50.0) });
        g2->replace({ QPointF(t - WINDOW_MS, 80.0), QPointF(t, 80.0) });
    };
    initGuides(guideTempWarn, guideTempHigh);
    initGuides(guideVibWarn,  guideVibHigh);
    initGuides(guideIntrWarn, guideIntrHigh);
}

void MainDashboard::configureOperatorLayoutStretch()
{
    if (auto *v = qobject_cast<QVBoxLayout*>(ui->pageOperator->layout())) {
        v->setStretch(0, 5); // Charts
        v->setStretch(1, 1); // Filter
        v->setStretch(2, 3); // Logs
    }
}

void MainDashboard::seedDummyPool()
{
    palaceCoords = {
        {"경복궁", 37.579617,126.977041}, {"창덕궁", 37.579414,126.991102},
        {"창경궁", 37.582604,126.994546}, {"덕수궁", 37.565804,126.975146},
        {"경희궁", 37.571785,126.959206}, {"경복궁-서문", 37.578645,126.976829},
        {"경복궁-북측", 37.579965,126.978345},{"창덕궁-서측", 37.581112,126.992230},
        {"덕수궁-동측", 37.564990,126.976000},{"경희궁-남측", 37.572500,126.960800}
    };


    // ✅ Location 필터 콤보 초기 세팅
    if (comboLocation) {
        QStringList items; items << "All";
        for (const auto& p : palaceCoords) items << p.name;
        items.removeDuplicates();
        comboLocation->clear();
        comboLocation->addItems(items);
        comboLocation->setCurrentIndex(0);
    }
}

QString MainDashboard::nearestPalaceName(double lat, double lng) const
{
    QString best="Unknown"; double bestD2=1e18;
    for (const auto& p : palaceCoords) {
        const double dLat=lat-p.lat, dLng=lng-p.lng, d2=dLat*dLat+dLng*dLng;
        if (d2<bestD2){ bestD2=d2; best=p.name; }
    }
    return best;
}

void MainDashboard::tickDummy()
{
    SensorData d;
    d.sensorId = QString("S-%1").arg(QRandomGenerator::global()->bounded(1000));
    const int t = QRandomGenerator::global()->bounded(3);
    d.type  = (t==0 ? "TEMP" : (t==1 ? "VIB" : "INTR"));
    d.value = QRandomGenerator::global()->bounded(100.0);
    d.level = (d.value > 80.0) ? "HIGH" : (d.value > 50.0 ? "MEDIUM" : "LOW");
    d.timestamp = nowMs();
    const auto& base = palaceCoords.at(QRandomGenerator::global()->bounded(palaceCoords.size()));
    d.latitude  = base.lat + ((QRandomGenerator::global()->bounded(1000)/100000.0) - 0.005);
    d.longitude = base.lng + ((QRandomGenerator::global()->bounded(1000)/100000.0) - 0.005);

    if ((d.type=="TEMP" && !enableTemp) || (d.type=="VIB" && !enableVib) || (d.type=="INTR" && !enableIntr)) return;
    updateChart(d);
}

void MainDashboard::updateChart(SensorData d)
{
    QLineSeries* s=nullptr; QDateTimeAxis* ax=nullptr; QValueAxis* ay=nullptr;
    QLineSeries *gWarn=nullptr, *gHigh=nullptr;
    QScatterSeries *scWarn=nullptr, *scHigh=nullptr;
    TypeThreshold thr;

    QVector<SensorData>* bufWarn=nullptr; QVector<SensorData>* bufHigh=nullptr;
    if (d.type=="TEMP"){
        s=seriesTemp; ax=axisXTemp; ay=axisYTemp; gWarn=guideTempWarn; gHigh=guideTempHigh; scWarn=scatterTempWarn; scHigh=scatterTempHigh; thr=thrTemp_;
        bufWarn=&tempWarnBuf_; bufHigh=&tempHighBuf_;
    } else if (d.type=="VIB"){
        s=seriesVib; ax=axisXVib; ay=axisYVib; gWarn=guideVibWarn; gHigh=guideVibHigh; scWarn=scatterVibWarn; scHigh=scatterVibHigh; thr=thrVib_;
        bufWarn=&vibWarnBuf_; bufHigh=&vibHighBuf_;
    } else {
        s=seriesIntr; ax=axisXIntr; ay=axisYIntr; gWarn=guideIntrWarn; gHigh=guideIntrHigh; scWarn=scatterIntrWarn; scHigh=scatterIntrHigh; thr=thrIntr_;
        bufWarn=&intrWarnBuf_; bufHigh=&intrHighBuf_;
    }
    if (!s) return;

    s->append(d.timestamp, d.value);
    if (s->count() > MAX_POINTS_PER_SERIES) {
        s->removePoints(0, s->count() - MAX_POINTS_PER_SERIES);
    }

    if (scWarn && scHigh) {
        if (d.value >= thr.high) {
            scHigh->append(d.timestamp, d.value);
            if (bufHigh) bufHigh->append(d);
            if (scHigh->count() > MAX_SCATTER_POINTS) {
                int rm = scHigh->count() - MAX_SCATTER_POINTS;
                scHigh->removePoints(0, rm);
                if (bufHigh && bufHigh->size() >= rm) bufHigh->erase(bufHigh->begin(), bufHigh->begin()+rm);
            }
        } else if (d.value >= thr.warn) {
            scWarn->append(d.timestamp, d.value);
            if (bufWarn) bufWarn->append(d);
            if (scWarn->count() > MAX_SCATTER_POINTS) {
                int rm = scWarn->count() - MAX_SCATTER_POINTS;
                scWarn->removePoints(0, rm);
                if (bufWarn && bufWarn->size() >= rm) bufWarn->erase(bufWarn->begin(), bufWarn->begin()+rm);
            }
        }
    }

    appendLogRow(d);
    if (db) db->insertSensorData(d);

    const qint64 t = d.timestamp;
    ax->setRange(QDateTime::fromMSecsSinceEpoch(t - WINDOW_MS),
                 QDateTime::fromMSecsSinceEpoch(t));

    if (gWarn) gWarn->replace({ QPointF(t - WINDOW_MS, thr.warn), QPointF(t, thr.warn) });
    if (gHigh) gHigh->replace({ QPointF(t - WINDOW_MS, thr.high), QPointF(t, thr.high) });

    autoScaleYAxis(s, ay, thr, t);
}

void MainDashboard::appendLogRow(const SensorData& d)
{
    if (modelLogs) modelLogs->append(d);
    statusBar()->showMessage(QString("%1 %2 %3 %4")
                                 .arg(d.sensorId, d.type, d.level,
                                      QDateTime::fromMSecsSinceEpoch(d.timestamp).toString("yyyy-MM-dd HH:mm:ss")));
}

void MainDashboard::onAnomaly(SensorData d)
{
    QMessageBox::warning(this, "Anomaly Detected",
                         QString("Sensor %1 anomaly: %2").arg(d.sensorId).arg(d.value));
}

void MainDashboard::onLogDoubleClicked(const QModelIndex& proxyIndex)
{
    if (!proxyIndex.isValid()) return;
    const QModelIndex src = proxyLogs->mapToSource(proxyIndex);
    if (!src.isValid()) return;
    const int srcRow = src.row();
    if (srcRow < 0 || srcRow >= modelLogs->rowCount(QModelIndex())) return;

    const SensorData& d = modelLogs->at(srcRow);

    if (!mapDlg) {
        mapDlg = new MapDialog(this);
        mapDlg->setModal(false);
        mapDlg->resize(800, 520);
        connect(mapDlg, &QObject::destroyed, this, [this]{ mapDlg = nullptr; });
    }

    const QString place = nearestPalaceName(d.latitude, d.longitude);
    const QString info = QString("%1  |  %2 %3  •  %4")
                             .arg(place)
                             .arg(d.type)
                             .arg(d.value, 0, 'f', 2)
                             .arg(QDateTime::fromMSecsSinceEpoch(d.timestamp).toString("yyyy-MM-dd HH:mm:ss"));
    mapDlg->setInfoText(info);
    mapDlg->setLocation(d.latitude, d.longitude);
    mapDlg->show(); mapDlg->raise(); mapDlg->activateWindow();
}

// 센서 ON/OFF
void MainDashboard::tempOn(){ if(canSeeOperator()) enableTemp = true; updateStatusToggles(); }
void MainDashboard::tempOff(){ if(canSeeOperator()) enableTemp = false; updateStatusToggles(); }
void MainDashboard::vibOn(){ if(canSeeOperator()) enableVib = true; updateStatusToggles(); }
void MainDashboard::vibOff(){ if(canSeeOperator()) enableVib = false; updateStatusToggles(); }
void MainDashboard::intrOn(){ if(canSeeOperator()) enableIntr = true; updateStatusToggles(); }
void MainDashboard::intrOff(){ if(canSeeOperator()) enableIntr = false; updateStatusToggles(); }

// 공용 헬퍼: 표시문자(다국어) -> 표준 코드(or 빈값)
static QString canonCode(const QString& s) {
    const QString t = s.trimmed();
    if (t.isEmpty() || t.compare("All", Qt::CaseInsensitive)==0 || t==u8"전체")
        return QString(); // 무필터
    return t.toUpper();   // TEMP/VIB/INTR, LOW/MEDIUM/HIGH
}


void MainDashboard::applyFilter()
{
    // 1) 타입/레벨/위치
    const QString tcode = canonCode(ui->comboType ? ui->comboType->currentText() : QString());
    const QString lcode = canonCode(ui->comboLevel ? ui->comboLevel->currentText() : QString());
    const QString loc   = canonCode(comboLocation ? comboLocation->currentText() : QString());

    proxyLogs->setTypeFilter(tcode);        // 빈 문자열이면 '무필터'로 처리되게 프록시에서 해주세요
    proxyLogs->setLevelFilter(lcode);
    proxyLogs->setLocationFilter(loc);

    // 2) ID 부분일치
    proxyLogs->setIdSubstring(ui->lineSearch ? ui->lineSearch->text().trimmed() : QString());

    // 3) 값 범위
    bool okMin=false, okMax=false;
    const double vmin = ui->lineMin->text().toDouble(&okMin);
    const double vmax = ui->lineMax->text().toDouble(&okMax);
    proxyLogs->setValueMin(okMin ? std::optional<double>(vmin) : std::nullopt);
    proxyLogs->setValueMax(okMax ? std::optional<double>(vmax) : std::nullopt);

    // 4) 시간 범위 — 경계/활성 안전화
    QDateTime from = ui->dtFrom->dateTime();
    QDateTime to   = ui->dtTo->dateTime();
    if (from.isValid() && to.isValid() && from > to)
        std::swap(from, to);  // 역전된 경우 자동 보정

    const bool useTime = (from.isValid() && to.isValid());
    proxyLogs->setFromTo(from, to, useTime);

    // 보조: 적용 후 현재 행수 표시(디버그에 유용)
    statusBar()->showMessage(tr("Filter applied, rows = %1").arg(proxyLogs->rowCount()));
}

void MainDashboard::resetFilter()
{
    // UI 초기화
    if (ui->comboType)  ui->comboType->setCurrentIndex(0);
    if (ui->comboLevel) ui->comboLevel->setCurrentIndex(0);
    if (comboLocation)  comboLocation->setCurrentIndex(0);
    ui->lineMin->clear(); ui->lineMax->clear(); ui->lineSearch->clear();

    // 시간: 1년~현재
    const QDateTime now = QDateTime::currentDateTime();
    ui->dtTo->setDateTime(now);
    ui->dtFrom->setDateTime(now.addYears(-1));

    // 프록시에 '무필터' 전달(빈 문자열/널옵션)
    proxyLogs->setTypeFilter(QString());
    proxyLogs->setLevelFilter(QString());
    proxyLogs->setLocationFilter(QString());
    proxyLogs->setIdSubstring(QString());
    proxyLogs->setValueMin(std::nullopt);
    proxyLogs->setValueMax(std::nullopt);

    // 시간 필터는 활성(true) + 올바른 범위
    proxyLogs->setFromTo(ui->dtFrom->dateTime(), ui->dtTo->dateTime(), true);

    statusBar()->showMessage(tr("Filter cleared (1y~now), rows = %1").arg(proxyLogs->rowCount()));
}


void MainDashboard::exportCSV()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export CSV"), QString(), tr("CSV Files (*.csv)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export"), tr("Cannot open file."));
        return;
    }

    QTextStream out(&f);

    // ✅ logView가 쓰는 현재 모델을 직접 사용 (프록시/소스 무엇이든 OK)
    QAbstractItemModel* model = ui->logView ? ui->logView->model() : nullptr;
    if (!model) { f.close(); return; }

    // 헤더
    {
        QStringList hdrs;
        for (int c = 0; c < model->columnCount(); ++c)
            hdrs << model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
        out << hdrs.join(",") << "\n";
    }

    // 데이터
    for (int r = 0; r < model->rowCount(); ++r) {
        QStringList cols;
        for (int c = 0; c < model->columnCount(); ++c) {
            QString cell = model->index(r, c).data().toString();
            cell.replace('"', "\"\"");
            if (cell.contains(',') || cell.contains('"'))
                cell = "\"" + cell + "\"";
            cols << cell;
        }
        out << cols.join(",") << "\n";
    }
    f.close();
    statusBar()->showMessage(LanguageManager::inst().t("CSV exported"));
}

void MainDashboard::exportTXT()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export TXT"), QString(), tr("Text Files (*.txt)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export"), tr("Cannot open file."));
        return;
    }

    QTextStream out(&f);

    // ✅ 현재 뷰 모델 사용
    QAbstractItemModel* model = ui->logView ? ui->logView->model() : nullptr;
    if (!model) { f.close(); return; }

    for (int r = 0; r < model->rowCount(); ++r) {
        QStringList cols;
        for (int c = 0; c < model->columnCount(); ++c)
            cols << model->index(r, c).data().toString();
        out << cols.join("\t") << "\n";
    }
    f.close();
    statusBar()->showMessage(LanguageManager::inst().t("TXT exported"));
}

// SuperAdmin: 암호 초기화
void MainDashboard::resetPassword()
{
    if (!isSuperAdmin()){ QMessageBox::warning(this,"권한 없음","SuperAdmin만 가능합니다."); return; }
    if (!ui->tblUsers){ QMessageBox::warning(this,"오류","사용자 목록이 없습니다."); return; }

    const int row = ui->tblUsers->currentRow();
    if (row < 0){ QMessageBox::information(this,"선택 필요","대상 사용자를 선택하세요."); return; }
    const QString target = ui->tblUsers->item(row,0)->text();

    bool ok=false;
    const QString tempPw = QInputDialog::getText(this, "암호 초기화",
                                                 QString("사용자 [%1]의 새 임시 비밀번호:").arg(target),
                                                 QLineEdit::Password, QString(), &ok);
    if (!ok || tempPw.isEmpty()) return;

    QString err;
    if (!db->resetUserPassword(currentUser, target, tempPw, &err)){
        QMessageBox::warning(this,"실패", err.isEmpty()? "암호 초기화 실패" : err);
        return;
    }
    QMessageBox::information(this, "완료",
                             QString("사용자 [%1] 암호가 초기화되었습니다.\n(로그인 시 새 비밀번호 사용)").arg(target));
}

// View 전환
void MainDashboard::applyFocus(QWidget* focus)
{
    auto* grid = qobject_cast<QGridLayout*>(ui->gbCharts->layout());
    if (!grid) return;

    auto showSplit = [&](){
        ui->gbCharts->show();
        ui->chartTemp->show();
        ui->chartVib->show();
        ui->chartIntr->show();

        ui->gbFilter->setVisible(false);
        ui->logView->show();

        grid->removeWidget(ui->chartTemp);
        grid->removeWidget(ui->chartVib);
        grid->removeWidget(ui->chartIntr);

        grid->addWidget(ui->chartTemp, 0, 0, 1, 1);
        grid->addWidget(ui->chartVib,  0, 1, 1, 1);
        grid->addWidget(ui->chartIntr, 1, 0, 1, 1);
        auto* filler = new QWidget(ui->gbCharts); // 빈 칸
        filler->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        grid->addWidget(filler,        1, 1, 1, 1);

    };

    if (focus == nullptr) { showSplit(); return; }



    if (focus == ui->logView) {
        ui->gbCharts->hide();
        ui->logView->show();
        ui->gbFilter->setVisible(true);
        removeFilterSensorButtons();
        return;
    }
    showSplit();
}

void MainDashboard::viewSplit()      { if (!canSeeOperator()) { QMessageBox::warning(this, "권한 없음", "Operator 이상만 접근 가능합니다."); return; } ui->stackRoles->setCurrentWidget(ui->pageOperator); applyFocus(nullptr); }
void MainDashboard::viewFocusTemp()  { if (!canSeeOperator()) { QMessageBox::warning(this, "권한 없음", "Operator 이상만 접근 가능합니다."); return; } ui->stackRoles->setCurrentWidget(ui->pageOperator); applyFocus(ui->chartTemp); }
void MainDashboard::viewFocusVib()   { if (!canSeeOperator()) { QMessageBox::warning(this, "권한 없음", "Operator 이상만 접근 가능합니다."); return; } ui->stackRoles->setCurrentWidget(ui->pageOperator); applyFocus(ui->chartVib); }
void MainDashboard::viewFocusIntr()  { if (!canSeeOperator()) { QMessageBox::warning(this, "권한 없음", "Operator 이상만 접근 가능합니다."); return; } ui->stackRoles->setCurrentWidget(ui->pageOperator); applyFocus(ui->chartIntr); }
void MainDashboard::viewFocusLogs()  { if (!canSeeOperator()) { QMessageBox::warning(this, "권한 없음", "Operator 이상만 접근 가능합니다."); return; } ui->stackRoles->setCurrentWidget(ui->pageOperator); applyFocus(ui->logView); }

void MainDashboard::ensureDefaultLandingPage()
{
    // 항상 홈(메인 대시보드)로 시작
    viewHome();
}

// 상단 메뉴 & 툴바
void MainDashboard::buildMenusAndToolbar()
{
    menuBar()->clear();

    menuBar()->setAttribute(Qt::WA_AlwaysShowToolTips, true);

    // 1) File
    QMenu* mFile = menuBar()->addMenu(LanguageManager::inst().t("File"));
    QAction* actLock   = mFile->addAction(LanguageManager::inst().t("🔒 Lock"));
    QAction* actExport = mFile->addAction(LanguageManager::inst().t("⬇️ Export (CSV/TXT)"));
    mFile->addSeparator();
    buildLanguageMenu(mFile);
    QAction* actLogout = mFile->addAction(LanguageManager::inst().t("🚪 Logout"));
    QAction* actExit   = mFile->addAction(LanguageManager::inst().t("⏻ Exit"));
    connect(actLock,   &QAction::triggered, this, &MainDashboard::lockScreen);
    connect(actExport, &QAction::triggered, this, [this]{ exportCSV(); });
    connect(actLogout, &QAction::triggered, this, &MainDashboard::logout);
    connect(actExit,   &QAction::triggered, this, &MainDashboard::close);

    // 2) View
    QMenu* mView = menuBar()->addMenu(LanguageManager::inst().t("View"));
    QAction* actHome  = mView->addAction(LanguageManager::inst().t("🏠 Home"));
    QAction* actSplit = mView->addAction(LanguageManager::inst().t("🪟 Split"));
    QAction* actTemp  = mView->addAction(LanguageManager::inst().t("🌡️ Focus: Temp"));
    QAction* actVib   = mView->addAction(LanguageManager::inst().t("📳 Focus: Vib"));
    QAction* actIntr  = mView->addAction(LanguageManager::inst().t("🛡️ Focus: Intr"));
    QAction* actLogs  = mView->addAction(LanguageManager::inst().t("📜 Focus: Logs"));
    QAction* actGeo   = mView->addAction(LanguageManager::inst().t("🗺️ Realtime Geo Dashboard"));
    actHome->setShortcut(QKeySequence("Ctrl+0"));
    actSplit->setShortcut(QKeySequence("Ctrl+1"));
    actTemp->setShortcut(QKeySequence("Ctrl+2"));
    actVib->setShortcut(QKeySequence("Ctrl+3"));
    actIntr->setShortcut(QKeySequence("Ctrl+4"));
    actLogs->setShortcut(QKeySequence("Ctrl+5"));
    connect(actHome,  &QAction::triggered, this, &MainDashboard::viewHome);
    connect(actSplit, &QAction::triggered, this, &MainDashboard::viewSplit);
    connect(actTemp,  &QAction::triggered, this, &MainDashboard::viewFocusTemp);
    connect(actVib,   &QAction::triggered, this, &MainDashboard::viewFocusVib);
    connect(actIntr,  &QAction::triggered, this, &MainDashboard::viewFocusIntr);
    connect(actLogs,  &QAction::triggered, this, &MainDashboard::viewFocusLogs);

    // 실시간 위치/종류 대시보드
    connect(actGeo, &QAction::triggered, this, [this]{
        auto *d = new RealtimeGeoDashboard(this, modelLogs);
        d->show(); d->raise(); d->activateWindow();
    });

    // 3) AI
    QMenu* mAI = menuBar()->addMenu(LanguageManager::inst().t("AI"));
    actKpi   = mAI->addAction(LanguageManager::inst().t("KPI (Charts)"));            connect(actKpi,   &QAction::triggered, this, &MainDashboard::generateKPI);
    actKps   = mAI->addAction(LanguageManager::inst().t("KPI Summary (Text)"));      connect(actKps,   &QAction::triggered, this, &MainDashboard::generateKpiSummary);
    actTrend = mAI->addAction(LanguageManager::inst().t("Trend + Forecast"));        connect(actTrend, &QAction::triggered, this, &MainDashboard::generateTrend);
    actXAI   = mAI->addAction(LanguageManager::inst().t("XAI Notes"));               connect(actXAI,   &QAction::triggered, this, &MainDashboard::generateXAI);
    actRpt   = mAI->addAction(LanguageManager::inst().t("Report Editor (PDF)"));     connect(actRpt,   &QAction::triggered, this, &MainDashboard::generateReport);
    mAI->addSeparator();
    mAI->addAction(LanguageManager::inst().t("Rule Simulator"), this, &MainDashboard::generateRuleSim);
    mAI->addAction(LanguageManager::inst().t("RCA (Co-occurrence)"), this, &MainDashboard::generateRCA);
    mAI->addAction(LanguageManager::inst().t("NL Query…"), this, &MainDashboard::generateNLQ);
    mAI->addAction(LanguageManager::inst().t("Sensor Health"), this, &MainDashboard::generateHealthDiag);
    mAI->addSeparator();
    mAI->addAction(LanguageManager::inst().t("Tuning Impact (14d)"), this, &MainDashboard::generateTuningImpact);

    // 4) Admin
    QMenu* mAdmin = menuBar()->addMenu(LanguageManager::inst().t("Admin"));
    actUsers = mAdmin->addAction(LanguageManager::inst().t("👤 User Management"));
    connect(actUsers, &QAction::triggered, this, &MainDashboard::openUserMgmt);
    QAction* actThr = mAdmin->addAction(LanguageManager::inst().t("Alert Thresholds"));
    connect(actThr, &QAction::triggered, this, &MainDashboard::openThresholds);
    actAudit = mAdmin->addAction(LanguageManager::inst().t("🔍 Audit Logs"));
    connect(actAudit, &QAction::triggered, this, &MainDashboard::openAuditLogs);
    actSessions = mAdmin->addAction(LanguageManager::inst().t("Active Sessions"));
    connect(actSessions, &QAction::triggered, this, &MainDashboard::openActiveSessions);

    buildCornerToggles();
}

void MainDashboard::viewHome()
{
    if (!pageHome) buildMainBoard();
    ui->stackRoles->setCurrentWidget(pageHome);
    statusBar()->showMessage(LanguageManager::inst().t("Home"));
}

static QColor typeColor(const QString& t) {
    if (t=="TEMP") return QColor(46,204,113);
    if (t=="VIB")  return QColor(52,152,219);
    return QColor(155,89,182);
}

void MainDashboard::buildMainBoard()
{
    if (pageHome) return;

    pageHome = new QWidget(this);
    auto *root = new QVBoxLayout(pageHome);
    root->setContentsMargins(12,12,12,12);
    root->setSpacing(12);

    auto& L = LanguageManager::inst();

    // 헤더
    {
        auto *hl = new QHBoxLayout();
        auto *title = new QLabel(L.t("Main Dashboard"), pageHome);
        QFont f = title->font(); f.setBold(true); f.setPointSize(f.pointSize()+2);
        title->setFont(f); title->setStyleSheet("color:#0b1220;");
        auto *sub = new QLabel("Realtime Geo + Insights", pageHome);
        sub->setStyleSheet("color:#667085;");
        hl->addWidget(title, 1, Qt::AlignLeft | Qt::AlignVCenter);
        hl->addWidget(sub,   0, Qt::AlignRight| Qt::AlignVCenter);
        root->addLayout(hl);
    }

    // KPI 4개
    auto makeKpiCard = [&](const QString& name, QLabel** out){
        auto *card = new QFrame(pageHome);
        card->setObjectName("kpiNumCard");
        card->setStyleSheet(R"(
            QFrame#kpiNumCard {
              background: qlineargradient(x1:0,y1:0,x2:1,y2:1,
                 stop:0 rgba(255,255,255,0.92), stop:1 rgba(245,248,255,0.92));
              border:1px solid rgba(0,0,0,0.06);
              border-radius:16px;
            }
        )");
        auto *v = new QVBoxLayout(card);
        v->setContentsMargins(16,14,16,14);
        v->setSpacing(4);

        auto *cap = new QLabel(name, card);
        cap->setStyleSheet("color:#667085; font-weight:600;");
        auto *val = new QLabel("--", card);
        QFont fv = val->font(); fv.setBold(true); fv.setPointSize(fv.pointSize()+10);
        val->setFont(fv); val->setStyleSheet("color:#0b1220;");

        v->addWidget(cap);
        v->addWidget(val, 1, Qt::AlignLeft | Qt::AlignVCenter);
        if (out) *out = val;
        return card;
    };

    auto *kpis = new QHBoxLayout(); kpis->setSpacing(12);
    kpis->addWidget(makeKpiCard(L.t("Total Alerts"), &kpiTotal));
    kpis->addWidget(makeKpiCard(L.t("HIGH %"),       &kpiHighPct));
    kpis->addWidget(makeKpiCard(L.t("Active Sensors"), &kpiActive));
    kpis->addWidget(makeKpiCard(L.t("Today vs Last Week"), &kpiTodayVs));
    root->addLayout(kpis);

    // 하단 탭: Charts(좌표/차트 패널) + Insights(맵 대체)
    homeTabs = new QTabWidget(pageHome);
    homeTabs->setStyleSheet(R"(
        QTabWidget::pane { border:1px solid rgba(0,0,0,0.06); border-radius:14px; background:white; }
        QTabBar::tab { padding:8px 14px; margin:6px; border-radius:10px; background:#f7f9fc; }
        QTabBar::tab:selected { background:white; border:1px solid #e6e8ee; }
    )");

    // Charts 탭 = 좌 표 / 우 차트 (GeoPanel)
    QWidget *tabCharts = new QWidget(homeTabs);
    auto *chartsLay = new QVBoxLayout(tabCharts);
    chartsLay->setContentsMargins(10,10,10,10);
    geoPanel = new RealtimeGeoPanel(tabCharts, modelLogs);
    chartsLay->addWidget(geoPanel);
    homeTabs->addTab(tabCharts, L.t("Charts"));

    // 🔄 Map 탭 제거 → Insights 탭 추가
    QWidget *tabInsights = new QWidget(homeTabs);
    auto *insLay = new QGridLayout(tabInsights);
    insLay->setContentsMargins(10,10,10,10);
    insLay->setSpacing(12);

    // (A) 24h Alert Level 분포 도넛
    insLevelPie = new QPieSeries(this);
    insLevelPie->setHoleSize(0.55);
    auto *pieChart = new QChart();
    pieChart->addSeries(insLevelPie);
    pieChart->setTitle(L.t("Alert Level Mix (24h)"));
    pieChart->legend()->setVisible(true);
    insLevelPieView = new QChartView(pieChart);
    insLevelPieView->setRenderHint(QPainter::Antialiasing);

    // (B) 위치별 HIGH Top N (수평 막대)
    insTopLocSeries = new QBarSeries(this);
    auto *barChart = new QChart();
    barChart->addSeries(insTopLocSeries);
    barChart->setTitle(L.t("Top Locations by HIGH (24h)"));
    insTopLocAxX = new QBarCategoryAxis(this);
    insTopLocAxY = new QValueAxis(this);
    insTopLocAxY->setTitleText(L.t("Count"));
    barChart->addAxis(insTopLocAxX, Qt::AlignBottom);
    barChart->addAxis(insTopLocAxY, Qt::AlignLeft);
    insTopLocSeries->attachAxis(insTopLocAxX);
    insTopLocSeries->attachAxis(insTopLocAxY);
    insTopLocView = new QChartView(barChart);
    insTopLocView->setRenderHint(QPainter::Antialiasing);

    // 배치: 좌(도넛) 우(Top N 막대)
    insLay->addWidget(insLevelPieView, 0, 0, 1, 1);
    insLay->addWidget(insTopLocView,   0, 1, 1, 1);
    insLay->setColumnStretch(0, 1);
    insLay->setColumnStretch(1, 1);

    homeTabs->addTab(tabInsights, L.t("Insights"));

    root->addWidget(homeTabs, 1);
    ui->stackRoles->addWidget(pageHome);

    // KPI/인사이트 주기 갱신
    connect(&homeRefreshTimer, &QTimer::timeout, this, &MainDashboard::refreshMainBoard);
    homeRefreshTimer.start(3000);
    refreshMainBoard();
}

void MainDashboard::refreshMainBoard()
{
    if (!db) return;

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime from24h = now.addDays(-1);
    const QDateTime today0 = QDateTime(QDate::currentDate(), QTime(0,0,0));
    const QDateTime lastWeek0 = today0.addDays(-7);
    const QDateTime lastWeekNow = now.addDays(-7);

    // 타입별 KPI 합산
    auto types = QStringList{"TEMP","VIB","INTR"};
    long long totalCnt = 0, totalHigh = 0;
    long long todayCnt = 0, lastWeekCnt = 0;

    for (const auto& t : types) {
        KPIStats k{}; db->kpiForType(t, from24h, now, &k);
        totalCnt  += k.count;
        totalHigh += k.high;

        KPIStats ktoday{}; db->kpiForType(t, today0, now, &ktoday);
        KPIStats klw{};    db->kpiForType(t, lastWeek0, lastWeekNow, &klw);
        todayCnt   += ktoday.count;
        lastWeekCnt+= klw.count;
    }

    // Total Alerts
    if (kpiTotal)   kpiTotal->setText(QString::number(totalCnt));

    // HIGH %
    int pct = (totalCnt>0) ? qBound(0, int(std::round(100.0 * totalHigh / totalCnt)), 100) : 0;
    if (kpiHighPct) kpiHighPct->setText(QString::number(pct) + " %");

    // Active Sensors (24h): 모델 기준 distinct
    auto countActiveFromModel = [&](const QDateTime& from, const QDateTime& to)->int{
        if (!modelLogs) return 0;
        QSet<QString> s;
        for (int r=0; r<modelLogs->rowCount(QModelIndex()); ++r) {
            const SensorData& d = modelLogs->at(r);
            const qint64 ts = d.timestamp;
            if (ts >= from.toMSecsSinceEpoch() && ts <= to.toMSecsSinceEpoch())
                s.insert(d.sensorId);
        }
        return s.size();
    };
    int active = countActiveFromModel(from24h, now);
    if (kpiActive)  kpiActive->setText(QString::number(active));

    // Today vs Last Week
    if (kpiTodayVs) {
        const bool up = todayCnt >= lastWeekCnt;
        const QString arrow = up ? "↑" : "↓";
        const QString col   = up ? "#16a34a" : "#ef4444";
        kpiTodayVs->setText(
            QString("<span style='font-weight:700;'>%1</span> "
                    "<span style='color:%2;'>%3</span> "
                    "<span style='color:#667085;'>%4</span>")
                .arg(todayCnt)
                .arg(col)
                .arg(arrow)
                .arg(lastWeekCnt));
    }

    // 패널(표/차트) 강제 리빌드
    if (geoPanel) geoPanel->forceRebuild();

    // ── Insights 탭 데이터 갱신 (24h분)
    if (modelLogs) {
        long long low=0, mid=0, hi=0;
        QMap<QString,int> highByLoc;

        for (int r=0; r<modelLogs->rowCount(QModelIndex()); ++r) {
            const SensorData& d = modelLogs->at(r);
            if (d.timestamp < from24h.toMSecsSinceEpoch()) continue;

            if (d.level=="HIGH") ++hi;
            else if (d.level=="MEDIUM") ++mid;
            else ++low;

            if (d.level=="HIGH") {
                const QString loc = nearestPalaceName(d.latitude, d.longitude);
                highByLoc[loc] += 1;
            }
        }

        // (A) 도넛
        if (insLevelPie) {
            insLevelPie->clear();
            auto add = [&](const QString& name, int v, const QColor& col){
                if (v<=0) return;
                auto *sl = new QPieSlice(name, v);
                sl->setLabelVisible(true);
                sl->setPen(QPen(col.darker(120)));
                sl->setBrush(col);
                insLevelPie->append(sl);
            };
            add("LOW",    int(low), QColor(46,204,113));
            add("MEDIUM", int(mid), QColor(241,196,15));
            add("HIGH",   int(hi),  QColor(231,76,60));
        }

        // (B) 위치별 HIGH Top N
        if (insTopLocSeries && insTopLocAxX && insTopLocAxY) {
            // 정렬
            QVector<QPair<QString,int>> vec;
            vec.reserve(highByLoc.size());
            for (auto it=highByLoc.cbegin(); it!=highByLoc.cend(); ++it)
                vec.push_back({it.key(), it.value()});
            std::sort(vec.begin(), vec.end(),
                      [](auto& a, auto& b){ return a.second > b.second; });
            const int N = std::min(8, int(vec.size()));

            insTopLocSeries->clear();
            auto *set = new QBarSet("HIGH (24h)");
            QStringList cats;
            int ymax = 1;
            for (int i=0;i<N;++i) {
                cats << vec[i].first;
                *set << vec[i].second;
                ymax = std::max(ymax, vec[i].second);
            }
            insTopLocSeries->append(set);
            insTopLocAxX->clear();
            insTopLocAxX->append(cats);
            insTopLocAxY->setRange(0, std::max(5, ymax+1));
        }
    }
}

// 우측 상단 토글
void MainDashboard::buildCornerToggles()
{
    if (cornerBox) {
        menuBar()->setCornerWidget(cornerBox, Qt::TopRightCorner);
        return;
    }

    cornerBox = new QWidget(this);
    auto *hl  = new QHBoxLayout(cornerBox);
    hl->setContentsMargins(0,0,6,0);
    hl->setSpacing(6);

    const int targetH = std::max(24, menuBar()->sizeHint().height() - 2);

    auto applyText = [](QToolButton* b, const QString& nameKey){
        auto& L = LanguageManager::inst();
        const bool on = b->isChecked();
        const QString name = L.t(nameKey);
        b->setText(on ? (name + " " + L.t("ON")) : (name + " " + L.t("OFF")));
        b->setToolTip(on ? (name + " " + L.t("ON")) : (name + " " + L.t("OFF")));
    };

    auto make = [&](const QString& name, bool checked, auto onToggled){
        auto *b = new QToolButton(cornerBox);
        b->setCheckable(true);
        b->setChecked(checked);
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        b->setAutoRaise(true);
        b->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        b->setMinimumHeight(targetH);
        b->setMaximumHeight(targetH);

        b->setStyleSheet(R"(
          QToolButton {
            padding: 2px 10px;
            border-radius: 14px;
            font-weight: 600;
            font-size: 12px;
            letter-spacing: 0.2px;
            border: 1px solid rgba(0,0,0,0.06);
            background: qlineargradient(x1:0,y1:0, x2:0,y2:1,
                        stop:0 rgba(255,255,255,0.70),
                        stop:1 rgba(255,255,255,0.50));
            color: #0b1220;
        }
          QToolButton:checked {
            background: rgba(46,204,113,0.20);
            border-color: rgba(46,204,113,0.40);
          }
          QToolButton:hover:checked { background: rgba(46,204,113,0.30); }
          QToolButton:!checked {
            background: rgba(231,76,60,0.16);
            border-color: rgba(231,76,60,0.40);
          }
          QToolButton:hover:!checked { background: rgba(231,76,60,0.26); }
          QToolButton:disabled { opacity: 0.55; }
        )");

        connect(b, &QToolButton::toggled, this, [=](bool on){
            onToggled(on);
            applyText(b, name);
            updateStatusToggles();
        });
        applyText(b, name);
        return b;
    };

    tbTemp = make("TEMP", enableTemp, [this](bool on){ enableTemp = on; });
    tbVib  = make("VIB",  enableVib,  [this](bool on){ enableVib  = on; });
    tbIntr = make("INTR", enableIntr, [this](bool on){ enableIntr = on; });

    hl->addWidget(tbTemp);
    hl->addWidget(tbVib);
    hl->addWidget(tbIntr);
    cornerBox->setLayout(hl);

    menuBar()->setCornerWidget(cornerBox, Qt::TopRightCorner);
    updateStatusToggles();
}

void MainDashboard::syncCornerTogglesEnabled()
{
    const bool op = canSeeOperator();
    if (tbTemp) tbTemp->setEnabled(op);
    if (tbVib)  tbVib->setEnabled(op);
    if (tbIntr) tbIntr->setEnabled(op);
}

void MainDashboard::reloadThresholdsAndApply() {
    TypeThreshold t;
    thrTemp_ = (db && db->getTypeThreshold("TEMP",&t)) ? t : TypeThreshold{"TEMP",50,80,"°C"};
    thrVib_  = (db && db->getTypeThreshold("VIB",&t))  ? t : TypeThreshold{"VIB",10,20,"mm/s RMS"};
    thrIntr_ = (db && db->getTypeThreshold("INTR",&t)) ? t : TypeThreshold{"INTR",50,80,"score"};

    applyThresholdToChart(thrTemp_, chartTemp, axisYTemp, guideTempWarn, guideTempHigh);
    applyThresholdToChart(thrVib_,  chartVib,  axisYVib,  guideVibWarn,  guideVibHigh);
    applyThresholdToChart(thrIntr_, chartIntr, axisYIntr, guideIntrWarn, guideIntrHigh);

    applyLanguageTexts();
}

void MainDashboard::autoScaleYAxis(QLineSeries* s, QValueAxis* ay,
                                   const TypeThreshold& thr, qint64 tNow)
{
    if (!s || !ay) return;
    const auto pts = s->points();
    if (pts.isEmpty()) return;

    const qreal tMin = tNow - WINDOW_MS;
    double mn =  std::numeric_limits<double>::infinity();
    double mx = -std::numeric_limits<double>::infinity();
    for (const auto& p : pts) {
        if (p.x() < tMin) continue;
        mn = std::min(mn, double(p.y()));
        mx = std::max(mx, double(p.y()));
    }
    if (!std::isfinite(mn) || !std::isfinite(mx)) { mn = 0; mx = 100; }

    mn = std::min(mn, std::min(thr.warn, thr.high));
    mx = std::max(mx, std::max(thr.warn, thr.high));

    const double pad = std::max(1.0, (mx - mn) * 0.10);
    ay->setRange(mn - pad, mx + pad);
}

void MainDashboard::applyThresholdToChart(const TypeThreshold& thr,
                                          QChart* /*chart*/, QValueAxis* ay,
                                          QLineSeries* warnGuide, QLineSeries* highGuide)
{
    const qint64 t = QDateTime::currentMSecsSinceEpoch();
    const qint64 window = WINDOW_MS;

    if (warnGuide) warnGuide->replace({ QPointF(t-window, thr.warn), QPointF(t, thr.warn) });
    if (highGuide) highGuide->replace({ QPointF(t-window, thr.high), QPointF(t, thr.high) });

    if (ay) {
        const double mn = std::min(thr.warn, thr.high);
        const double mx = std::max(thr.warn, thr.high);
        const double pad = std::max(1.0, (mx - mn) * 0.25);
        ay->setRange(mn - pad, mx + pad);
    }
}

void MainDashboard::beautifyButtons()
{
    auto pretty = [&](QPushButton* b, QStyle::StandardPixmap sp){
        if (!b) return;
        b->setIcon(style()->standardIcon(sp));
        b->setIconSize(QSize(18,18));
        b->setMinimumHeight(34);
        b->setStyleSheet(R"(
          QPushButton {
            font-weight:600; padding:8px 14px; border-radius:12px;
            border:1px solid rgba(255,255,255,0.60);
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                        stop:0 rgba(255,255,255,0.85),
                        stop:1 rgba(255,255,255,0.58));
          }
          QPushButton:hover   { background: rgba(255,255,255,0.90); }
          QPushButton:pressed { background: rgba(255,255,255,0.95); }
          QPushButton:disabled{ opacity:0.6; }
        )");
    };
    pretty(ui->btnTempOn,  QStyle::SP_MediaPlay);
    pretty(ui->btnTempOff, QStyle::SP_MediaStop);
    pretty(ui->btnVibOn,   QStyle::SP_MediaPlay);
    pretty(ui->btnVibOff,  QStyle::SP_MediaStop);
    pretty(ui->btnIntrOn,  QStyle::SP_MediaPlay);
    pretty(ui->btnIntrOff, QStyle::SP_MediaStop);
    pretty(ui->btnApplyFilter, QStyle::SP_DialogApplyButton);
    pretty(ui->btnResetFilter, QStyle::SP_BrowserReload);
    pretty(ui->btnExport,      QStyle::SP_DialogSaveButton);
}

// 보안: 잠금/로그아웃
void MainDashboard::lockScreen()
{
    if (!lockShield) {
        lockShield = new QWidget(this);
        lockShield->setObjectName("lockShield");
        lockShield->setStyleSheet("#lockShield{background:rgba(0,0,0,0.35);}");
        lockShield->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        lockShield->setFocusPolicy(Qt::StrongFocus);
        lockShield->setGeometry(rect());

        auto *panel = new QWidget(lockShield);
        panel->setStyleSheet("background:white;border-radius:12px;");
        panel->setFixedWidth(360);
        panel->setMinimumHeight(180);
        panel->move((width()-panel->width())/2, (height()-panel->height())/2);

        auto *v = new QVBoxLayout(panel);
        v->setContentsMargins(20,20,20,20);
        v->setSpacing(10);

        lockMsg = new QLabel(QString("🔒 화면 잠금\n사용자: %1").arg(currentUser), panel);
        lockMsg->setAlignment(Qt::AlignCenter);
        lockMsg->setWordWrap(true);
        lockMsg->setStyleSheet("font-weight:700;");

        lockPwEdit = new QLineEdit(panel);
        lockPwEdit->setEchoMode(QLineEdit::Password);
        lockPwEdit->setPlaceholderText("비밀번호 입력 후 Enter");

        auto *hint = new QLabel("로그는 계속 갱신되지만\n해제 전까지 모든 클릭이 차단됩니다.", panel);
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet("color:#444;");

        v->addWidget(lockMsg);
        v->addWidget(lockPwEdit);
        v->addWidget(hint);

        connect(lockPwEdit, &QLineEdit::returnPressed, this, &MainDashboard::unlockScreen);
    }
    lockPwEdit->clear();
    lockShield->setGeometry(rect());
    lockShield->raise();
    lockShield->show();
    lockPwEdit->setFocus();
}

void MainDashboard::unlockScreen()
{
    const QString pw = lockPwEdit ? lockPwEdit->text() : QString();
    if (pw.isEmpty()) return;

    AuthResult r = db->authenticate(currentUser, pw);
    if (!r.ok) {
        QMessageBox::warning(this, "해제 실패", "비밀번호가 올바르지 않습니다.");
        lockPwEdit->selectAll();
        lockPwEdit->setFocus();
        return;
    }
    if (lockShield) lockShield->hide();
}

// Admin/Users/Thresholds
void MainDashboard::openUserMgmt()
{
    if (!isSuperAdmin()) {
        QMessageBox::warning(this,"권한 없음","SuperAdmin만 접근 가능합니다.");
        return;
    }
    UserMgmtDialog dlg(db, currentUser, this);
    dlg.exec();
}

void MainDashboard::openThresholds() {
    if (!canSeeAdmin()) {
        QMessageBox::warning(this, "권한 없음", "Admin 이상만 접근 가능합니다.");
        return;
    }
    AlertRulesDialog dlg(db, currentUser, this);
    if (dlg.exec() == QDialog::Accepted) {
        reloadThresholdsAndApply();
        statusBar()->showMessage(LanguageManager::inst().t("Thresholds updated"));
    }
}

void MainDashboard::openActiveSessions()
{
    if (!canSeeAdmin()) {
        QMessageBox::warning(this, "권한 없음", "Admin 이상만 접근 가능합니다.");
        return;
    }
    auto* d = new CurrentSessionsDialog(this);
    d->setModal(false);
    d->show();
}

// 세션/하트비트
void MainDashboard::setSession(qint64 sid, const QString& token)
{
    sessionId = sid; sessionToken = token;
    startHeartbeat();
    QSettings s("VigilEdge","VigilEdge");
    s.setValue("session/id", sessionId);
    s.setValue("session/token", sessionToken);
}

void MainDashboard::startHeartbeat()
{
    stopHeartbeat();
    connect(&sessionPingTimer, &QTimer::timeout, this, [this](){
        if (sessionId > 0 && !sessionToken.isEmpty())
            sessService.heartbeat(sessionId, sessionToken);
    });
    sessionPingTimer.start(30000); // 30초마다 하트비트
}
void MainDashboard::stopHeartbeat()
{
    if (sessionPingTimer.isActive()) sessionPingTimer.stop();
}

void MainDashboard::logout()
{
    // ✅ 즉시 세션 종료 후 앱 종료 → main()에서 필요한 처리 (예: 로그인 화면 복귀)
        if (sessionId > 0 && !sessionToken.isEmpty()) {
            sessService.endSession(sessionId, sessionToken);
        }
    stopHeartbeat();
    qApp->exit(777);
    QSettings s("VigilEdge","VigilEdge");
    s.remove("session/id");
    s.remove("session/token");
}


// Role UI
void MainDashboard::setEnabledVisible(QWidget* w, bool on) {
    if (!w) return;
    w->setEnabled(on);
    w->setVisible(on);
}

void MainDashboard::applyRoleVisibility()
{
    if (ui->actionAdmin)    ui->actionAdmin->setEnabled(canSeeAdmin());
    if (ui->actionAnalyst)  ui->actionAnalyst->setEnabled(canSeeAnalyst());
    if (ui->actionOperator) ui->actionOperator->setEnabled(canSeeOperator());
    if (ui->actionViewer)   ui->actionViewer->setEnabled(true);

    const bool op = canSeeOperator();
    setEnabledVisible(ui->btnTempOn,  op);
    setEnabledVisible(ui->btnTempOff, op);
    setEnabledVisible(ui->btnVibOn,   op);
    setEnabledVisible(ui->btnVibOff,  op);
    setEnabledVisible(ui->btnIntrOn,  op);
    setEnabledVisible(ui->btnIntrOff, op);

    const bool viewerCanExport = true;
    const bool viewerCanFilter = true;
    setEnabledVisible(ui->gbFilter,   op || viewerCanFilter);
    if (ui->btnApplyFilter) ui->btnApplyFilter->setEnabled(op || viewerCanFilter);
    if (ui->btnResetFilter) ui->btnResetFilter->setEnabled(op || viewerCanFilter);
    if (ui->btnExport)      ui->btnExport->setEnabled(op || viewerCanExport);

    const bool admin = canSeeAdmin();
    if (ui->gbUsers) ui->gbUsers->setVisible(admin);
    if (ui->tblUsers) ui->tblUsers->setEnabled(admin);

    const bool super = isSuperAdmin();
    setEnabledVisible(ui->btnAssignRole, super);
    setEnabledVisible(ui->btnResetPwd,   super);

    if (actKpi)  actKpi->setEnabled(true);
    if (actKps)  actKps->setEnabled(true);
    if (actTrend)actTrend->setEnabled(canSeeAnalyst());
    if (actXAI)  actXAI->setEnabled(canSeeAnalyst());
    if (actRpt)  actRpt->setEnabled(canSeeAnalyst());

    if (actUsers) actUsers->setEnabled(isSuperAdmin());
    if (actAudit)  actAudit->setEnabled(canSeeAnalyst());
    if (actSessions) actSessions->setEnabled(canSeeAdmin());

    syncCornerTogglesEnabled();
}

void MainDashboard::applyRoleUI()
{
    ui->actionViewer->setEnabled(true);
    ui->actionOperator->setEnabled(canSeeOperator());
    ui->actionAnalyst->setEnabled(canSeeAnalyst());
    ui->actionAdmin->setEnabled(canSeeAdmin());

    applyRoleVisibility();

    if (ui->btnAssignRole) ui->btnAssignRole->setEnabled(isSuperAdmin());
    if (ui->btnResetPwd)   ui->btnResetPwd->setEnabled(isSuperAdmin());

    if (canSeeAdmin() && ui->tblUsers) {
        ui->tblUsers->setRowCount(0);
        ui->tblUsers->setColumnCount(3);
        ui->tblUsers->setHorizontalHeaderLabels({"Username","Role","Active"});

        QString err;
        const auto list = db->listUsersEx(&err);
        if (!err.isEmpty()) statusBar()->showMessage("유저목록 오류: " + err);

        ui->tblUsers->setRowCount(list.size());
        for (int i = 0; i < list.size(); ++i) {
            ui->tblUsers->setItem(i, 0, new QTableWidgetItem(list[i].username));
            ui->tblUsers->setItem(i, 1, new QTableWidgetItem(roleToString(list[i].role)));
            auto *it = new QTableWidgetItem(list[i].active ? "ON" : "OFF");
            it->setTextAlignment(Qt::AlignCenter);
            it->setForeground(list[i].active ? QBrush(QColor("#0a7d24")) : QBrush(Qt::red));
            ui->tblUsers->setItem(i, 2, it);
        }
        ui->tblUsers->horizontalHeader()->setStretchLastSection(true);

        if (ui->btnAssignRole) {
            if (connAssignRole) { QObject::disconnect(connAssignRole); connAssignRole = {}; }
            connAssignRole = connect(ui->btnAssignRole, &QPushButton::clicked, this, [this](){
                if (!isSuperAdmin()){ QMessageBox::warning(this,"권한 없음","SuperAdmin만 역할 변경 가능"); return; }
                const int row = ui->tblUsers->currentRow();
                if (row < 0){ QMessageBox::information(this,"선택 필요","대상 사용자를 선택하세요."); return; }

                const QString target = ui->tblUsers->item(row,0)->text();
                const QString roleStr = ui->comboNewRole ? ui->comboNewRole->currentText() : "Viewer";

                Role newRole = Role::Viewer;
                if (roleStr=="Operator") newRole=Role::Operator;
                else if (roleStr=="Analyst") newRole=Role::Analyst;
                else if (roleStr=="Admin") newRole=Role::Admin;
                else if (roleStr=="SuperAdmin") newRole=Role::SuperAdmin;

                QString err;
                if (!db->setUserRole(currentUser, target, newRole, &err)){
                    QMessageBox::warning(this,"실패", err.isEmpty() ? "역할 변경 실패" : err);
                    return;
                }
                ui->tblUsers->item(row,1)->setText(roleToString(newRole));
                QMessageBox::information(this,"완료","역할이 변경되었습니다.");
            });
        }

        ui->tblUsers->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(ui->tblUsers, &QTableWidget::customContextMenuRequested,
                this, [this](const QPoint &pos){
                    if (!isSuperAdmin()) return;
                    const int row = ui->tblUsers->rowAt(pos.y());
                    if (row < 0) return;

                    const QString target = ui->tblUsers->item(row,0)->text();
                    const bool currentlyOn = (ui->tblUsers->item(row,2)->text() == "ON");

                    QMenu m(this);
                    QAction *actToggle = m.addAction(currentlyOn ? "비활성화" : "활성화");
                    QAction *actReset  = m.addAction("비밀번호 초기화…");
                    QAction *picked = m.exec(ui->tblUsers->viewport()->mapToGlobal(pos));
                    if (!picked) return;

                    if (picked == actToggle) {
                        QString err;
                        if (!db->setUserActive(currentUser, target, !currentlyOn, &err)) {
                            QMessageBox::warning(this,"실패", err.isEmpty()? "상태 변경 실패" : err);
                            return;
                        }
                        ui->tblUsers->item(row,2)->setText(!currentlyOn ? "ON" : "OFF");
                        ui->tblUsers->item(row,2)->setForeground(!currentlyOn ? QBrush(QColor("#0a7d24")) : QBrush(Qt::red));
                    } else if (picked == actReset) {
                        resetPassword();
                    }
                });
    }

    // ✅ 사용자/권한 라벨 갱신
    updateStatusBarRole();
}

// Role 페이지 전환
void MainDashboard::setRoleOperator(){
    if (!canSeeOperator()) { QMessageBox::warning(this,"권한 없음","Operator 이상만 접근 가능합니다."); return; }
    ui->stackRoles->setCurrentWidget(ui->pageOperator);
    statusBar()->showMessage(LanguageManager::inst().t("Role: Operator"));
    updateStatusBarRole();
}
void MainDashboard::setRoleAnalyst(){
    if (!canSeeAnalyst()) { QMessageBox::warning(this,"권한 없음","Analyst 이상만 접근 가능합니다."); return; }
    ui->stackRoles->setCurrentWidget(ui->pageAnalyst);
    statusBar()->showMessage(LanguageManager::inst().t("Role: Analyst"));
    updateStatusBarRole();
}
void MainDashboard::setRoleAdmin(){
    if (!canSeeAdmin()) { QMessageBox::warning(this,"권한 없음","Admin 이상만 접근 가능합니다."); return; }
    ui->stackRoles->setCurrentWidget(ui->pageAdmin);
    statusBar()->showMessage(LanguageManager::inst().t("Role: Admin"));
    applyRoleUI();
    updateStatusBarRole();
}
void MainDashboard::setRoleViewer() {
    ui->stackRoles->setCurrentWidget(ui->pageViewer);
    prepViewerHome();
    statusBar()->showMessage(LanguageManager::inst().t("Role: Viewer"));
    updateStatusBarRole();
}

void MainDashboard::openAuditLogs()
{
    if (!canSeeAnalyst()) {
        QMessageBox::warning(this, "권한 없음", "Analyst 이상만 접근 가능합니다.");
        return;
    }
    AuditLogDialog dlg(db, currentUser, currentRole, this);
    dlg.exec();
}


// ───────────────────────── Scatter Hover/Click 구현 ─────────────────────────
QWidget* MainDashboard::chartViewForSeries(const QObject* s) const {
    if (s==scatterTempWarn || s==scatterTempHigh) return ui->chartTemp;
    if (s==scatterVibWarn  || s==scatterVibHigh)  return ui->chartVib;
    if (s==scatterIntrWarn || s==scatterIntrHigh) return ui->chartIntr;
    return nullptr;
}
bool MainDashboard::findScatterIndex(QScatterSeries* s, const QPointF& point, int* outIdx) const {
    if (!s) return false;
    const auto v = s->points();
    int best=-1; double bestD=1e100;
    for (int i=0;i<v.size();++i){
        const double dx=v[i].x()-point.x(), dy=v[i].y()-point.y();
        const double d=dx*dx+dy*dy;
        if (d<bestD){ bestD=d; best=i; }
        if (qFuzzyCompare(v[i].x(), point.x()) && qFuzzyCompare(v[i].y(), point.y())){ best=i; break; }
    }
    if (best<0) return false;
    if (outIdx) *outIdx=best;
    return true;
}
void MainDashboard::onScatterHovered(const QPointF& point, bool state)
{
    auto *sc = qobject_cast<QScatterSeries*>(sender());
    if (!sc || !state) { QToolTip::hideText(); return; }

    int idx=-1; if (!findScatterIndex(sc, point, &idx)) return;
    const QVector<SensorData>* buf=nullptr;
    if (sc==scatterTempWarn) buf=&tempWarnBuf_; else if (sc==scatterTempHigh) buf=&tempHighBuf_;
    else if (sc==scatterVibWarn) buf=&vibWarnBuf_; else if (sc==scatterVibHigh) buf=&vibHighBuf_;
    else if (sc==scatterIntrWarn) buf=&intrWarnBuf_; else if (sc==scatterIntrHigh) buf=&intrHighBuf_;
    if (!buf || idx<0 || idx>=buf->size()) return;
    const SensorData& d = (*buf)[idx];

    auto& L = LanguageManager::inst();
    const QString tt = QString("%1 • %2 %3\n%4\n(%5, %6)")
                           .arg(d.sensorId)
                           .arg(d.type) // 타입 키는 센서 코드 그대로 노출
                           .arg(QString::number(d.value,'f',2))
                           .arg(QDateTime::fromMSecsSinceEpoch(d.timestamp).toString("yyyy-MM-dd HH:mm:ss"))
                           .arg(QString::number(d.latitude,'f',6))
                           .arg(QString::number(d.longitude,'f',6));
    QToolTip::showText(QCursor::pos(), tt, chartViewForSeries(sc));
}
void MainDashboard::onScatterClicked(const QPointF& point)
{
    auto *sc = qobject_cast<QScatterSeries*>(sender());
    if (!sc) return;
    int idx=-1; if (!findScatterIndex(sc, point, &idx)) return;
    const QVector<SensorData>* buf=nullptr;
    if (sc==scatterTempWarn) buf=&tempWarnBuf_; else if (sc==scatterTempHigh) buf=&tempHighBuf_;
    else if (sc==scatterVibWarn) buf=&vibWarnBuf_; else if (sc==scatterVibHigh) buf=&vibHighBuf_;
    else if (sc==scatterIntrWarn) buf=&intrWarnBuf_; else if (sc==scatterIntrHigh) buf=&intrHighBuf_;
    if (!buf || idx<0 || idx>=buf->size()) return;
    const SensorData d = (*buf)[idx];

    if (!mapDlg) {
        mapDlg = new MapDialog(this);
        mapDlg->setModal(false);
        mapDlg->resize(800,520);
        connect(mapDlg, &QObject::destroyed, this, [this]{ mapDlg = nullptr; });

    }
    const QString place = nearestPalaceName(d.latitude, d.longitude);
    const QString info = QString("%1  |  %2 %3  •  %4")
                             .arg(place)
                             .arg(d.type)
                             .arg(d.value,0,'f',2)
                             .arg(QDateTime::fromMSecsSinceEpoch(d.timestamp).toString("yyyy-MM-dd HH:mm:ss"));
    mapDlg->setInfoText(info);
    mapDlg->setLocation(d.latitude, d.longitude);
    mapDlg->show(); mapDlg->raise(); mapDlg->activateWindow();
}

// AI 팝업
void MainDashboard::generateKPI()
{
#ifdef HAVE_AI_DIALOGS
    KPIDialog* d = new KPIDialog(db, aiFrom, aiTo, this); d->show();
#else
    QMessageBox::information(this,"KPI","KPI 차트 팝업은 ENABLE_AI_DIALOGS=ON 빌드에서 사용 가능합니다.");
#endif
}
void MainDashboard::generateKpiSummary()
{
#ifdef HAVE_AI_DIALOGS
    KPISummaryDialog* d = new KPISummaryDialog(db, aiFrom, aiTo, this); d->show();
#else
    QMessageBox::information(this,"KPI Summary","요약 팝업은 AIDialogs를 추가하면 표시됩니다.");
#endif
}
void MainDashboard::generateTrend()
{
#ifdef HAVE_AI_DIALOGS
    if (!canSeeAnalyst()) { QMessageBox::warning(this, "권한 없음", "Analyst 이상만 사용 가능합니다."); return; }
    TrendDialog* d = new TrendDialog(db, aiFrom, aiTo, 30, this); d->show();
#else
    QMessageBox::information(this,"Trend","Trend 팝업은 AIDialogs를 추가하면 표시됩니다.");
#endif
}
void MainDashboard::generateXAI()
{
#ifdef HAVE_AI_DIALOGS
    if (!canSeeAnalyst()) { QMessageBox::warning(this, "권한 없음", "Analyst 이상만 사용 가능합니다."); return; }
    XAIDialog* d = new XAIDialog(db, aiFrom, aiTo, this); d->show();
#else
    QMessageBox::information(this,"XAI","XAI 팝업은 AIDialogs를 추가하면 표시됩니다.");
#endif
}
void MainDashboard::generateReport() {
#ifdef HAVE_AI_DIALOGS
    auto *dlg = new ReportDialog(db, aiFrom, aiTo, this);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
#else
    QMessageBox::information(this, "Unavailable", "AI dialogs are not built in this configuration.");
#endif
}

void MainDashboard::generateRuleSim(){
    auto *d = new RuleSimDialog(db, aiFrom, aiTo, this); d->show();
}
void MainDashboard::generateRCA(){
    auto *d = new RCADialog(db, aiFrom, aiTo, QString(), this); d->show();
}
void MainDashboard::generateNLQ(){
    auto &L = LanguageManager::inst();
    QDialog dlg(this);
    dlg.setWindowTitle(L.t("NL Query…"));
    dlg.resize(720, 420);
    auto *v = new QVBoxLayout(&dlg);
    auto *help = new QLabel(&dlg);
    help->setWordWrap(true);
    help->setText(
        L.t("Type a question about your data (one line).") + "\n\n" +
        L.t("Examples:") + "\n"
                           "• " + L.t("List HIGH VIB events last week") + "\n"
                                                  "• " + L.t("Average TEMP by sensor for yesterday") + "\n"
                                                        "• " + L.t("Top 10 sensors with HIGH INTR count in last 24h") + "\n"
                                                                   "• " + L.t("Show TEMP trend and forecast for next 30 min")
        );
    auto *edit = new QLineEdit(&dlg);
    edit->setPlaceholderText(L.t("e.g., last week VIB HIGH events"));
    edit->setClearButtonEnabled(true);

    auto *btns = new QHBoxLayout();
    btns->addStretch(1);
    auto *ok = new QPushButton(L.t("Run"), &dlg);
    auto *cancel = new QPushButton(L.t("Cancel"), &dlg);
    ok->setDefault(true);          // 엔터 키로 실행
    btns->addWidget(ok); btns->addWidget(cancel);

    v->addWidget(help);
    v->addWidget(edit, 0);         // 단일행
    v->addLayout(btns);

    // 엔터로 즉시 실행
    QObject::connect(edit,   &QLineEdit::returnPressed, &dlg, &QDialog::accept);
    QObject::connect(ok,     &QPushButton::clicked,     &dlg, &QDialog::accept);
    QObject::connect(cancel, &QPushButton::clicked,     &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    const QString q = edit->text().trimmed();
    if (q.isEmpty()) return;
    auto *d = new NLQDialog(db, q, this); d->show();
}
void MainDashboard::generateHealthDiag(){
    // ✅ HealthDiagDialog 시그니처 최신화 (typeFilter 인자 추가)
    auto *d = new HealthDiagDialog(db, aiFrom, aiTo, QString(), this);
    d->show();
}
void MainDashboard::generateTuningImpact(){
    #ifdef HAVE_AI_DIALOGS
        auto *d = new TuningImpactDialog(db, aiFrom, aiTo, this);
        d->show();
    #else
        QMessageBox::information(this, "Unavailable",
                                                            "Tuning Impact 는 ENABLE_AI_DIALOGS=ON 빌드에서 사용 가능합니다.");
    #endif
}
