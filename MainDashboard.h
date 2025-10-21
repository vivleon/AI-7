#pragma once
#ifndef MAINDASHBOARD_H
#define MAINDASHBOARD_H

#include <QMainWindow>
#include <QMetaObject>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QChartView>
#include <QTimer>
#include <QVector>
#include <QModelIndex>
#include <QDateTime>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QPixmap>
#include <QMap>
#include <QPointF>
#include <QPointer>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QComboBox>


#include "Role.h"
#include "Thresholds.h"
#include "ThreadWorker.h"
#include "AuditLogDialog.h"
#include "SessionService.h"   // ✅ 세션
#include "CurrentSessionsDialog.h"
#include "AlertInsightsWidget.h"   // ★ 추가


QT_BEGIN_NAMESPACE
namespace Ui { class MainDashboard; }
QT_END_NAMESPACE

class DBBridge;
class LogTableModel;
class CustomFilterProxyModel;
class MapDialog;
class QAction;
class UserMgmtDialog;
class QMenu;
class QTableWidget;
class QTabWidget;
class QWidget;
class QDialog;

// Realtime Geo (표+차트 공용 컴포넌트)
template <class Base> class RealtimeGeoBase;
using RealtimeGeoPanel     = RealtimeGeoBase<QWidget>;
using RealtimeGeoDashboard = RealtimeGeoBase<QDialog>;


class MainDashboard : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainDashboard(DBBridge* db,
                           const QString& loginUser,
                           Role loginRole,
                           QWidget *parent = nullptr);
    ~MainDashboard() override;

    // 세션 정보 주입(heartbeat 시작)
    void setSession(qint64 sessionId, const QString& token);

private slots:
    // 역할 페이지
    void setRoleOperator();
    void setRoleAnalyst();
    void setRoleAdmin();
    void setRoleViewer();

    // 센서 ON/OFF
    void tempOn(); void tempOff();
    void vibOn();  void vibOff();
    void intrOn(); void intrOff();

    // 실시간/더미
    void updateChart(SensorData data);
    void onAnomaly(SensorData data);
    void onLogDoubleClicked(const QModelIndex& proxyIndex);
    void tickDummy();

    // 필터/내보내기
    void applyFilter();
    void resetFilter();
    void exportCSV();
    void exportTXT();

    // SuperAdmin: 암호 초기화
    void resetPassword();

    // View 전환
    void viewSplit();
    void viewFocusTemp();
    void viewFocusVib();
    void viewFocusIntr();
    void viewFocusLogs();

    // 보안/세션
    void lockScreen();
    void unlockScreen();
    void logout();

    // AI 팝업
    void generateKPI();
    void generateKpiSummary();
    void generateTrend();
    void generateXAI();
    void generateReport();
    void generateRuleSim();
    void generateRCA();
    void generateNLQ();
    void generateHealthDiag();
    void generateTuningImpact();

    // 언어
    void onLangSystem();
    void onLangKorean();
    void onLangEnglish();
    void onLanguageChanged();

    // Admin
    void openUserMgmt();
    void openThresholds();
    void openAuditLogs();
    void openActiveSessions(); // ✅ 신규

    // Scatter 인터랙션
    void onScatterHovered(const QPointF& point, bool state);
    void onScatterClicked(const QPointF& point);

    void viewHome();            // View 메뉴에서 홈으로 이동

private:
    // 구성/유틸
    void configureTablesAndStyle();
    void configureCharts();
    void configureOperatorLayoutStretch();
    void seedDummyPool();
    void applyRoleUI();
    void applyGlassTheme();
    void tuneTimeAxis(QDateTimeAxis* ax);
    void buildMenusAndToolbar();
    void beautifyButtons();
    void applyFocus(QWidget* focus);
    void appendLogRow(const SensorData& d);
    QString nearestPalaceName(double lat, double lng) const;

    bool canSeeOperator() const { return currentRole >= Role::Operator; }
    bool canSeeAnalyst()  const { return currentRole >= Role::Analyst; }
    bool canSeeAdmin()    const { return currentRole >= Role::Admin; }
    bool isSuperAdmin()   const { return currentRole == Role::SuperAdmin; }

    void updateStatusToggles();
    void removeFilterSensorButtons();

    void buildLanguageMenu(QMenu* mFile);
    void applyLanguageTexts();
    void reloadThresholdsAndApply();

    void applyThresholdToChart(const TypeThreshold& thr,
                               QChart* chart, QValueAxis* ay,
                               QLineSeries* warnGuide, QLineSeries* highGuide);

    void autoScaleYAxis(QLineSeries* s, QValueAxis* ay,
                        const TypeThreshold& thr, qint64 tNow);

    void applyRoleVisibility();
    void setEnabledVisible(QWidget* w, bool on);
    void buildCornerToggles();
    void syncCornerTogglesEnabled();
    void prepViewerHome();
    void updateViewerLogo();
    void ensureDefaultLandingPage();

    void startHeartbeat();
    void stopHeartbeat();
    void updateStatusBarRole();

    QWidget* chartViewForSeries(const QObject* s) const;
    bool findScatterIndex(QScatterSeries* s, const QPointF& p, int* outIdx) const;

    void rebuildExportMenu();
    void updateCornerToggleTexts();

    QComboBox* comboLocation {nullptr};

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    Ui::MainDashboard *ui {};
    ThreadWorker *worker {};
    DBBridge* db {};

    QString currentUser;
    Role    currentRole{Role::Viewer};

    // 세션
    qint64  sessionId {-1};
    QString sessionToken;
    QTimer  sessionPingTimer;
    SessionService sessService;
    QLabel* lblRole {nullptr};

    // 차트(Operator 페이지)
    QChart *chartTemp{}, *chartVib{}, *chartIntr{};
    QLineSeries *seriesTemp{}, *seriesVib{}, *seriesIntr{};
    QScatterSeries *scatterTempWarn{}, *scatterTempHigh{};
    QScatterSeries *scatterVibWarn{},  *scatterVibHigh{};
    QScatterSeries *scatterIntrWarn{}, *scatterIntrHigh{};

    QDateTimeAxis *axisXTemp{}, *axisXVib{}, *axisXIntr{};
    QValueAxis *axisYTemp{}, *axisYVib{}, *axisYIntr{};
    QLineSeries *guideTempWarn{}, *guideTempHigh{};
    QLineSeries *guideVibWarn{},  *guideVibHigh{};
    QLineSeries *guideIntrWarn{}, *guideIntrHigh{};

    // 임계치(타입별)
    TypeThreshold thrTemp_, thrVib_, thrIntr_;

    // 로그 모델/프록시
    LogTableModel*          modelLogs {};
    CustomFilterProxyModel* proxyLogs {};

    QPointer<MapDialog> mapDlg = nullptr;

    // AI 메뉴 액션
    QAction* actKpi   {nullptr};
    QAction* actKps   {nullptr};
    QAction* actTrend {nullptr};
    QAction* actXAI   {nullptr};
    QAction* actRpt   {nullptr};

    // Admin 메뉴 액션
    QAction* actUsers {nullptr};
    QAction* actAudit {nullptr};
    QAction* actSessions {nullptr};

    // corner toggles
    QWidget*     cornerBox {nullptr};
    QToolButton *tbTemp {nullptr}, *tbVib {nullptr}, *tbIntr {nullptr};

    // 상태
    QTimer dummyTimer;
    bool enableTemp {true}, enableVib {true}, enableIntr {true};
    const qint64 WINDOW_MS = 120000;
    const int    MAX_POINTS_PER_SERIES = 600;
    const int    MAX_SCATTER_POINTS    = 600;

    struct Palace { QString name; double lat; double lng; };
    QVector<Palace> palaceCoords;

    // AI 기간
    QDateTime aiFrom, aiTo;

    // 잠금 오버레이
    QWidget*   lockShield  {nullptr};
    QLineEdit* lockPwEdit  {nullptr};
    QLabel*    lockMsg     {nullptr};

    QLabel*  viewerLogo {nullptr};
    QPixmap  viewerLogoBase;

    // scatter 원본 버퍼
    QVector<SensorData> tempWarnBuf_, tempHighBuf_;
    QVector<SensorData> vibWarnBuf_,  vibHighBuf_;
    QVector<SensorData> intrWarnBuf_, intrHighBuf_;

    // ✅ 역할 버튼 연결 유지용
    QMetaObject::Connection connAssignRole;

    // ── 메인보드(홈) 위젯/요소
    QWidget* pageHome {nullptr};
    QTableWidget* tblHome {nullptr};
    QTimer homeRefreshTimer;

    // 카드: 숫자 라벨
    QLabel *lblCardTemp {nullptr}, *lblCardVib {nullptr}, *lblCardIntr {nullptr};

    // 카드: 링 게이지(도넛)
    QChartView *ringViewTemp {nullptr}, *ringViewVib {nullptr}, *ringViewIntr {nullptr};
    QPieSeries *ringTemp {nullptr}, *ringVib {nullptr}, *ringIntr {nullptr};
    QPieSlice  *ringTempFill {nullptr}, *ringVibFill {nullptr}, *ringIntrFill {nullptr};

    // 카드: 미니 스파크라인
    QChartView *sparkViewTemp {nullptr}, *sparkViewVib {nullptr}, *sparkViewIntr {nullptr};
    QLineSeries *sparkTemp {nullptr}, *sparkVib {nullptr}, *sparkIntr {nullptr};
    QDateTimeAxis *sparkXTemp {nullptr}, *sparkXVib {nullptr}, *sparkXIntr {nullptr};
    QValueAxis   *sparkYTemp {nullptr}, *sparkYVib {nullptr}, *sparkYIntr {nullptr};

    // 메인보드 UI/갱신
    void buildMainBoard();
    void refreshMainBoard();

    // 홈 KPI & 패널
    QLabel *kpiTotal {nullptr}, *kpiHighPct {nullptr},
        *kpiActive {nullptr}, *kpiTodayVs {nullptr};
    QTabWidget* homeTabs {nullptr};
    RealtimeGeoPanel* geoPanel {nullptr};

    // ── Insights 탭(맵 대체)
    QChartView *insLevelPieView {nullptr};
    QPieSeries *insLevelPie {nullptr};
    QChartView *insTopLocView {nullptr};
    QBarSeries *insTopLocSeries {nullptr};
    QBarCategoryAxis *insTopLocAxX {nullptr};
    QValueAxis *insTopLocAxY {nullptr};
};

#endif // MAINDASHBOARD_H
