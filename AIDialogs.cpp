#include "AIDialogs.h"
#include "DBBridge.h"
#include "Thresholds.h"
#include "LanguageManager.h"

#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QPrinter>
#include <QFileDialog>
#include <QPrintDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDateTime>
#include <QMap>
#include <algorithm>
#include <cmath>
#include <QTableWidget>
#include <QHeaderView>
#include <QGridLayout>
#include <QRegularExpression>


#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QPieSeries>
#include <QPen>
#include <QRegularExpression>

// (선언만 먼저; 기존 parseSimpleQuery 정의는 아래에 있어도 됩니다)
static bool parseSimpleQuery(const QString& q, QString* type, QString* level,
                             QDateTime* from, QDateTime* to);

// 내부 전용 헬퍼는 익명 네임스페이스에 넣어 충돌 방지
namespace {

struct NLIntent {
    enum Kind { KPI, SUMMARY, TREND, RCA, RULESIM, HEALTH, REPORT, UNKNOWN } kind = UNKNOWN;
    QString type;     // "TEMP"/"VIB"/"INTR" or ""
    QString level;    // "HIGH"/"MEDIUM"/"LOW" or ""
    QDateTime from = QDateTime::currentDateTime().addDays(-1);
    QDateTime to   = QDateTime::currentDateTime();
    int bucketSec = 300; // for trend
};

static QString detectType(const QString& q) {
    const QString s = q.toLower();
    if (s.contains("temp") || s.contains(QStringLiteral("온도")))  return "TEMP";
    if (s.contains("vib")  || s.contains(QStringLiteral("진동")))  return "VIB";
    if (s.contains("intr") || s.contains(QStringLiteral("침입")))  return "INTR";
    return "";
}
static QString detectLevel(const QString& q) {
    const QString s = q.toLower();
    if (s.contains("high") || s.contains(QStringLiteral("높음")) || s.contains(QStringLiteral("위험"))) return "HIGH";
    if (s.contains("med")  || s.contains(QStringLiteral("보통")) || s.contains(QStringLiteral("중간")) || s.contains(QStringLiteral("경고"))) return "MEDIUM";
    if (s.contains("low")  || s.contains(QStringLiteral("낮음"))) return "LOW";
    return "";
}

static bool parseExplicitRange(const QString& q, QDateTime* from, QDateTime* to) {
    QRegularExpression re(
        R"((\d{4}-\d{2}-\d{2}(?:[ T]\d{1,2}:\d{2})?)\s*[~\-toTO]\s*(\d{4}-\d{2}-\d{2}(?:[ T]\d{1,2}:\d{2})?))",
        QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(q);
    if (!m.hasMatch()) return false;
    auto parseOne = [](const QString& s)->QDateTime{
        QDateTime dt = QDateTime::fromString(s.trimmed(), "yyyy-MM-dd HH:mm");
        if (!dt.isValid()) dt = QDateTime::fromString(s.trimmed(), "yyyy-MM-dd");
        if (!dt.isValid()) dt = QDateTime::fromString(s.trimmed().replace('T',' '), "yyyy-MM-dd HH:mm");
        if (!dt.isValid()) dt = QDateTime::fromString(s.trimmed(), Qt::ISODate);
        return dt;
    };
    const QDateTime a = parseOne(m.captured(1)), b = parseOne(m.captured(2));
    if (!a.isValid() || !b.isValid() || a>b) return false;
    *from = a; *to = b; return true;
}

static void parseRelativeRange(const QString& q, QDateTime* from, QDateTime* to) {
    const QDateTime now = QDateTime::currentDateTime();

    // ko: 지난 N 분/시간/일/주/달
    QRegularExpression krNumUnit(R"(지난\s*(\d+)\s*(분|시간|일|주|개월|달))");
    auto mk = krNumUnit.match(q);
    if (mk.hasMatch()) {
        int n = mk.captured(1).toInt();
        const QString u = mk.captured(2);
        if (u.contains(QStringLiteral("분")))       *from = now.addSecs(-60*n);
        else if (u.contains(QStringLiteral("시간"))) *from = now.addSecs(-3600*n);
        else if (u.contains(QStringLiteral("일")))   *from = now.addDays(-n);
        else if (u.contains(QStringLiteral("주")))   *from = now.addDays(-7*n);
        else                                         *from = now.addMonths(-n);
        *to = now; return;
    }
    if (q.contains(QStringLiteral("오늘")))    { *from = QDateTime(QDate::currentDate().startOfDay()); *to = now; return; }
    if (q.contains(QStringLiteral("어제")))    { *from = QDateTime(QDate::currentDate().addDays(-1).startOfDay()); *to = QDateTime(QDate::currentDate().startOfDay()); return; }
    if (q.contains(QStringLiteral("이번주")))  { QDate d=QDate::currentDate(); *from = QDateTime(d.addDays(-(d.dayOfWeek()-1)).startOfDay()); *to = now; return; }
    if (q.contains(QStringLiteral("지난주")))  { QDate d=QDate::currentDate().addDays(-7); *from = QDateTime(d.addDays(-(d.dayOfWeek()-1)).startOfDay()); *to = from->addDays(7); return; }
    if (q.contains(QStringLiteral("이번달")))  { QDate d=QDate::currentDate(); *from = QDateTime(QDate(d.year(), d.month(), 1).startOfDay()); *to = now; return; }
    if (q.contains(QStringLiteral("지난달")))  { QDate d=QDate::currentDate().addMonths(-1); *from = QDateTime(QDate(d.year(), d.month(), 1).startOfDay()); *to = QDateTime(QDate::currentDate().addDays(-QDate::currentDate().day()+1).startOfDay()); return; }

    // en: last/past N min/hour/day/week/month
    QRegularExpression enNumUnit(R"((?:last|past)\s*(\d+)\s*(min(?:ute)?s?|h(?:our)?s?|days?|weeks?|months?))",
                                 QRegularExpression::CaseInsensitiveOption);
    auto me = enNumUnit.match(q);
    if (me.hasMatch()) {
        int n = me.captured(1).toInt();
        const QString u = me.captured(2).toLower();
        if (u.startsWith("min")) *from = now.addSecs(-60*n);
        else if (u.startsWith("h")) *from = now.addSecs(-3600*n);
        else if (u.startsWith("day")) *from = now.addDays(-n);
        else if (u.startsWith("week")) *from = now.addDays(-7*n);
        else *from = now.addMonths(-n);
        *to = now; return;
    }
    if (q.contains("today", Qt::CaseInsensitive))     { *from = QDateTime(QDate::currentDate().startOfDay()); *to = now; return; }
    if (q.contains("yesterday", Qt::CaseInsensitive)) { *from = QDateTime(QDate::currentDate().addDays(-1).startOfDay()); *to = QDateTime(QDate::currentDate().startOfDay()); return; }

    *from = now.addDays(-1); *to = now;
}

static int parseBucketSec(const QString& q) {
    QRegularExpression kr(R"((\d+)\s*분)"); auto mk = kr.match(q);
    if (mk.hasMatch()) return mk.captured(1).toInt() * 60;
    QRegularExpression en(R"((\d+)\s*(m|min|h|hour))", QRegularExpression::CaseInsensitiveOption);
    auto me = en.match(q);
    if (me.hasMatch()) {
        int n = me.captured(1).toInt(); const QString u = me.captured(2).toLower();
        return (u.startsWith("h")) ? n*3600 : n*60;
    }
    return 300;
}

static NLIntent parseIntent(const QString& query) {
    NLIntent intent;
    const QString q = query.trimmed();

    if (q.contains(QStringLiteral("요약")) || q.contains("summary", Qt::CaseInsensitive)) intent.kind = NLIntent::SUMMARY;
    else if (q.contains(QStringLiteral("추세")) || q.contains(QStringLiteral("트렌드")) || q.contains("trend", Qt::CaseInsensitive) || q.contains("forecast", Qt::CaseInsensitive) || q.contains(QStringLiteral("예측"))) intent.kind = NLIntent::TREND;
    else if (q.contains("rca", Qt::CaseInsensitive) || q.contains(QStringLiteral("연관")) || q.contains(QStringLiteral("원인"))) intent.kind = NLIntent::RCA;
    else if (q.contains(QStringLiteral("튜닝")) || q.contains("rule", Qt::CaseInsensitive) || q.contains("simul", Qt::CaseInsensitive) || q.contains(QStringLiteral("시뮬"))) intent.kind = NLIntent::RULESIM;
    else if (q.contains("health", Qt::CaseInsensitive) || q.contains(QStringLiteral("상태"))) intent.kind = NLIntent::HEALTH;
    else if (q.contains("report", Qt::CaseInsensitive) || q.contains(QStringLiteral("리포트"))) intent.kind = NLIntent::REPORT;
    else if (q.contains("kpi", Qt::CaseInsensitive)) intent.kind = NLIntent::KPI;
    else intent.kind = NLIntent::KPI;

    intent.type  = detectType(q);
    intent.level = detectLevel(q);

    if (!parseExplicitRange(q, &intent.from, &intent.to))
        parseRelativeRange(q, &intent.from, &intent.to);

    intent.bucketSec = parseBucketSec(q);
    return intent;
}

} // namespace


static QDateTime defaultFrom() { return QDateTime::currentDateTime().addDays(-1); }
static QDateTime defaultTo()   { return QDateTime::currentDateTime(); }

static QString trType(const QString& typ){
    auto& L = LanguageManager::inst();
    if (typ=="TEMP") return L.t("TEMP");
    if (typ=="VIB")  return L.t("VIB");
    if (typ=="INTR") return L.t("INTR");
    return typ;
}

BaseAIDialog::BaseAIDialog(QWidget* parent) : QDialog(parent) {
    setModal(false);
    setWindowModality(Qt::NonModal);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(980, 640);
    lay = new QVBoxLayout(this);
    setLayout(lay);
}

void BaseAIDialog::attachPdfToolbar(QWidget* target)
{
    auto& L = LanguageManager::inst();
    render_ = target;
    auto *row = new QHBoxLayout();
    auto *btnPdf = new QPushButton(L.t("Export PDF"), this);
    auto *btnPrint = new QPushButton(L.t("Print"), this);
    row->addStretch(1);
    row->addWidget(btnPdf);
    row->addWidget(btnPrint);
    lay->addLayout(row, 0);
    connect(btnPdf,  &QPushButton::clicked, this, &BaseAIDialog::exportPdf);
    connect(btnPrint,&QPushButton::clicked, this, &BaseAIDialog::printDoc);
}

void BaseAIDialog::exportPdf()
{
    auto& L = LanguageManager::inst();
    const QString path = QFileDialog::getSaveFileName(this, L.t("Export PDF"), QString(), "PDF (*.pdf)");
    if (path.isEmpty() || !render_) return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageMargins(QMarginsF(12,12,12,12));

    render_->render(&printer);
}

void BaseAIDialog::printDoc()
{
    if (!render_) return;
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) return;
    render_->render(&printer);
}

KPIDialog::KPIDialog(DBBridge* db, const QDateTime& from, const QDateTime& to, QWidget* parent)
    : BaseAIDialog(parent)
{
    auto& L = LanguageManager::inst();
    setWindowTitle("AI • KPI");
    buildChart(db, from, to);
}

void KPIDialog::buildChart(DBBridge* db, const QDateTime& from, const QDateTime& to)
{
    auto& L = LanguageManager::inst();
    QMap<QString,KPIStats> kpis; db->kpiAll(from, to, &kpis);

    auto *setAvg = new QBarSet(L.t("Avg"));
    auto *setMin = new QBarSet(L.t("Min"));
    auto *setMax = new QBarSet(L.t("Max"));

    QStringList cats; const QStringList types = {"TEMP","VIB","INTR"};
    for (const auto& t : types) {
        cats << trType(t);
        const KPIStats st = kpis.value(t);
        *setAvg << (st.count? st.avg : 0);
        *setMin << (st.count? st.mn  : 0);
        *setMax << (st.count? st.mx  : 0);
    }

    auto *series = new QBarSeries(); series->append(setAvg); series->append(setMin); series->append(setMax);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(QString("KPI (%1 ~ %2)")
                        .arg(from.toString("MM-dd HH:mm"))
                        .arg(to.toString("MM-dd HH:mm")));
    chart->setAnimationOptions(QChart::SeriesAnimations);

    auto *axisX = new QBarCategoryAxis(); axisX->append(cats); chart->addAxis(axisX, Qt::AlignBottom); series->attachAxis(axisX);
    auto *axisY = new QValueAxis(); axisY->setTitleText(L.t("Value")); chart->addAxis(axisY, Qt::AlignLeft); series->attachAxis(axisY);

    auto *view = new QChartView(chart); view->setRenderHint(QPainter::Antialiasing); lay->addWidget(view, 1); attachPdfToolbar(view);
}

KPISummaryDialog::KPISummaryDialog(DBBridge* db, const QDateTime& from, const QDateTime& to, QWidget* parent)
    : BaseAIDialog(parent)
{
    auto& L = LanguageManager::inst();
    setWindowTitle("AI • KPI Summary");
    auto* text = new QPlainTextEdit(this); text->setReadOnly(true);

    QMap<QString,KPIStats> kpis; db->kpiAll(from, to, &kpis);
    QString s; s += L.t("Period:") + " " + from.toString("yyyy-MM-dd HH:mm") + " ~ " + to.toString("yyyy-MM-dd HH:mm") + "\n\n";
    for (auto it = kpis.begin(); it != kpis.end(); ++it) {
        const QString t = trType(it.key());
        const KPIStats& st = it.value();
        s += QString("[%1]\n - %2: %3\n - %4: %5, %6: %7, %8: %9\n - %10: LOW %11, MED %12, HIGH %13\n\n")
                 .arg(t)
                 .arg(L.t("Count")).arg(st.count)
                 .arg(L.t("Avg")).arg(QString::number(st.avg,'f',2))
                 .arg(L.t("Min")).arg(QString::number(st.mn ,'f',2))
                 .arg(L.t("Max")).arg(QString::number(st.mx ,'f',2))
                 .arg(L.t("Levels")).arg(st.low).arg(st.med).arg(st.high);
    }
    text->setPlainText(s);

    lay->addWidget(text, 1); attachPdfToolbar(text);
}

static QVector<QPointF> holtLinear(const QVector<QPointF>& s, double alpha, double beta,
                                   int steps, qint64 stepMs)
{
    QVector<QPointF> out; if (s.size() < 2) return out; double L = s[0].y(); double T = s[1].y() - s[0].y();
    for (int i=1;i<s.size();++i){ const double y = s[i].y(); const double prevL = L; L = alpha * y + (1.0 - alpha) * (L + T); T = beta  * (L - prevL) + (1.0 - beta) * T; }
    const qreal lastX = s.back().x(); for (int k=1;k<=steps;++k){ const double yhat = L + k * T; out.append(QPointF(lastX + k * double(stepMs), yhat)); }
    return out;
}

TrendDialog::TrendDialog(DBBridge* db, const QDateTime& from, const QDateTime& to, int bucketSec, QWidget* parent)
    : BaseAIDialog(parent)
{
    auto& L = LanguageManager::inst();
    setWindowTitle("Analyst • Trend (+Forecast)");

    const QStringList types = {"TEMP","VIB","INTR"};
    auto *chart = new QChart();

    QMap<QString, TypeThreshold> thrByType; for (const auto& t : types) { TypeThreshold th; thrByType[t] = (db->getTypeThreshold(t, &th) ? th : TypeThreshold{t,50,80,""}); }
    qint64 stepMs = qint64(bucketSec) * 1000;

    for (const auto& t : types) {
        auto pts = db->trendSeries(t, from, to, bucketSec); if (pts.isEmpty()) continue;
        auto *ls = new QLineSeries(); ls->setName(trType(t)); for (const auto& p : pts) ls->append(p); chart->addSeries(ls);
        auto fc = holtLinear(pts, 0.35, 0.20, 10, stepMs);
        if (!fc.isEmpty()) {
            auto *lf = new QLineSeries(); lf->setName(trType(t) + " • Forecast"); QPen pen(Qt::DashLine); pen.setWidthF(1.5); lf->setPen(pen); for (const auto& p : fc) lf->append(p); chart->addSeries(lf);
            const auto thr = thrByType.value(t); double maxF = thr.warn; for (const auto& p : fc) maxF = std::max(maxF, double(p.y())); const double denom = std::max(1e-6, thr.high - thr.warn); const double risk = std::clamp((maxF - thr.warn) / denom, 0.0, 1.0); chart->setTitle(QString("Trend (%1s) — %2 risk≈%3%") .arg(bucketSec).arg(trType(t)).arg(int(std::round(risk*100))));
        }
    }

    auto *axX = new QDateTimeAxis(); axX->setTitleText(L.t("Time")); axX->setFormat("MM-dd HH:mm"); axX->setTickCount(8);
#if (QT_VERSION >= QT_VERSION_CHECK(6,2,0))
    axX->setLabelsAngle(-45);
#endif
    auto *axY = new QValueAxis(); axY->setTitleText(L.t("Avg Value"));

    chart->addAxis(axX, Qt::AlignBottom); chart->addAxis(axY, Qt::AlignLeft); for (auto s : chart->series()) { s->attachAxis(axX); s->attachAxis(axY); }
    chart->setAnimationOptions(QChart::SeriesAnimations);

    auto *view = new QChartView(chart); view->setRenderHint(QPainter::Antialiasing); lay->addWidget(view, 1); attachPdfToolbar(view);
}

XAIDialog::XAIDialog(DBBridge* db, const QDateTime& from, const QDateTime& to, QWidget* parent)
    : BaseAIDialog(parent)
{
    auto& L = LanguageManager::inst();
    setWindowTitle("Analyst • XAI");
    auto* text = new QPlainTextEdit(this); text->setReadOnly(true); lay->addWidget(text, 1); attachPdfToolbar(text);

    auto highs = db->topAnomalies(from, to, 10); int tH=0, vH=0, iH=0; for (const auto& d : highs) { if (d.type=="TEMP") ++tH; else if (d.type=="VIB") ++vH; else if (d.type=="INTR") ++iH; }
    QString out; out += QString("%1 %2 ~ %3\n\n").arg(L.t("Period:"), from.toString("yyyy-MM-dd HH:mm"), to.toString("yyyy-MM-dd HH:mm"));
    out += L.t("XAI Highlights") + ":\n";
    if (!highs.isEmpty()) {
        out += QString(" • %1 → TEMP:%2, VIB:%3, INTR:%4\n")
                   .arg(L.t("Top 10 HIGH distribution")).arg(tH).arg(vH).arg(iH);
        out += " • " + L.t("Checks recommended: TEMP↑(cooling/vent), VIB↑(bearing/alignment), INTR↑(access control)") + "\n";
    } else {
        out += " • " + L.t("No significant HIGH in range.") + "\n";
    }
    text->setPlainText(out);
}

ReportDialog::ReportDialog(DBBridge* db, const QDateTime& from, const QDateTime& to, QWidget* parent)
    : BaseAIDialog(parent), db_(db), from_(from), to_(to)
{
    auto& L = LanguageManager::inst();
    setWindowTitle("Analyst • Report");

    txt = new QPlainTextEdit(this); txt->setReadOnly(false);
    auto *row = new QHBoxLayout(); auto *btnGen = new QPushButton("↻ Auto Generate", this); row->addStretch(1); row->addWidget(btnGen); lay->addLayout(row, 0);
    lay->addWidget(txt, 1); attachPdfToolbar(txt);

    connect(btnGen, &QPushButton::clicked, this, &ReportDialog::autoGenerate);
    autoGenerate();
}

RuleSimDialog::RuleSimDialog(DBBridge* db, const QDateTime& from, const QDateTime& to, QWidget* parent)
    : BaseAIDialog(parent)
{
    auto& L = LanguageManager::inst();
    setWindowTitle("Analyst • Rule Simulator");

    const QStringList types = {"TEMP","VIB","INTR"}; auto *chart = new QChart();
    auto *curHigh = new QBarSet(L.t("Current HIGH")); auto *newHigh = new QBarSet(L.t("Simulated HIGH"));

    QStringList cats; for (const auto& t: types) {
        KPIStats cur{}; if (!db->kpiForType(t, from, to, &cur)) continue; double warn = 50, high = 80; TypeThreshold thr; if (db->getTypeThreshold(t, &thr)) { warn = thr.warn; high = thr.high; }
        KPIStats sim{}; db->simulateCounts(t, from, to, warn*1.05, high*1.10, &sim);
        cats << trType(t); *curHigh << cur.high; *newHigh << sim.high;
    }
    auto *series = new QBarSeries(); series->append(curHigh); series->append(newHigh);
    chart->addSeries(series);
    auto *axX = new QBarCategoryAxis(); axX->append(cats); chart->addAxis(axX, Qt::AlignBottom); series->attachAxis(axX);
    auto *axY = new QValueAxis(); axY->setTitleText("HIGH"); chart->addAxis(axY, Qt::AlignLeft); series->attachAxis(axY);

    auto *view = new QChartView(chart); view->setRenderHint(QPainter::Antialiasing); lay->addWidget(view, 1); attachPdfToolbar(view);
}

RCADialog::RCADialog(DBBridge* db, const QDateTime& from, const QDateTime& to, const QString& typeFilter, QWidget* parent)
    : BaseAIDialog(parent)
{
    auto& L = LanguageManager::inst();
    setWindowTitle("Analyst • RCA (Co-occurrence)");
    auto* txt = new QPlainTextEdit(this); txt->setReadOnly(true); lay->addWidget(txt, 1); attachPdfToolbar(txt);

    auto pairs = db->rcaCooccurrence(from, to, typeFilter, 5, 15);
    QString s; s += QString("%1 %2 ~ %3\n\n").arg(L.t("Period:"), from.toString("yyyy-MM-dd HH:mm"), to.toString("yyyy-MM-dd HH:mm"));
    s += L.t("RCA (Co-occurrence)") + ":\n";
    if (pairs.isEmpty()) s += " - " + L.t("No related audit logs") + "\n"; // reuse
    for (const auto& p : pairs) s += QString(" - %1 ↔ %2 : %3\n").arg(trType(p.a), trType(p.b)).arg(p.cnt);
    txt->setPlainText(s);
}

NLQDialog::NLQDialog(DBBridge* db, const QString& query, QWidget* parent)
    : BaseAIDialog(parent)
{
    auto& L = LanguageManager::inst();
    setWindowTitle("Analyst • NL Command");

    auto* txt = new QPlainTextEdit(this);
    txt->setReadOnly(true);
    lay->addWidget(txt, 1);
    attachPdfToolbar(txt);

    const NLIntent in = parseIntent(query);

    const QString header = QString("Query: %1\n%2 %3 ~ %4\n\n")
                               .arg(query)
                               .arg(L.t("Period:"))
                               .arg(in.from.toString("yyyy-MM-dd HH:mm"))
                               .arg(in.to.toString("yyyy-MM-dd HH:mm"));

    switch (in.kind) {
    case NLIntent::KPI:
        (new KPIDialog(db, in.from, in.to, this))->show();
        txt->setPlainText(header + L.t("Opened: KPI (Charts)"));
        break;

    case NLIntent::SUMMARY:
        (new KPISummaryDialog(db, in.from, in.to, this))->show();
        txt->setPlainText(header + L.t("Opened: KPI Summary"));
        break;

    case NLIntent::TREND: {
        auto *d = new TrendDialog(db, in.from, in.to, in.bucketSec, this);
        d->show();
        txt->setPlainText(header + QString("%1 (%2s)").arg(L.t("Opened: Trend + Forecast")).arg(in.bucketSec));
        break;
    }

    case NLIntent::RCA:
        (new RCADialog(db, in.from, in.to, in.type, this))->show();
        txt->setPlainText(header + L.t("Opened: RCA (Co-occurrence)") + (in.type.isEmpty()? "" : " • " + trType(in.type)));
        break;

    case NLIntent::RULESIM:
        (new RuleSimDialog(db, in.from, in.to, this))->show();
        txt->setPlainText(header + L.t("Opened: Rule Simulator"));
        break;

    case NLIntent::HEALTH:
        (new HealthDiagDialog(db, in.from, in.to, in.type, this))->show();
        txt->setPlainText(header + L.t("Opened: Sensor Health") + (in.type.isEmpty()? "" : " • " + trType(in.type)));
        break;

    case NLIntent::REPORT:
        (new ReportDialog(db, in.from, in.to, this))->show();
        txt->setPlainText(header + L.t("Opened: Report Editor"));
        break;

    default: {
        // fallback: 기존 단순 KPI 요약
        QString type, level; QDateTime f, t;
        parseSimpleQuery(query, &type, &level, &f, &t);
        KPIStats k{}; db->kpiForType(type.isEmpty()? "TEMP" : type, f, t, &k);
        QString s = header;
        s += QString("[%1] %2=%3 / LOW=%4, MED=%5, HIGH=%6\n")
                 .arg(type.isEmpty()? "(?)" : trType(type))
                 .arg(L.t("Count")).arg(k.count)
                 .arg(k.low).arg(k.med).arg(k.high);
        s += "\n" + L.t("Tip: try '진동 추세 10분', '지난주 TEMP 요약', 'INTR RCA', '센서 상태', '리포트 생성'.");
        txt->setPlainText(s);
        break;
    }
    }
}



static bool parseSimpleQuery(const QString& q, QString* type, QString* level, QDateTime* from, QDateTime* to) {
    QString s = q.toUpper().trimmed(); *type=""; *level=""; const QDateTime now = QDateTime::currentDateTime();
    if (s.contains("TEMP")) *type="TEMP"; else if (s.contains("VIB")) *type="VIB"; else if (s.contains("INTR")) *type="INTR";
    if (s.contains("HIGH")) *level="HIGH"; else if (s.contains("MED")) *level="MEDIUM"; else if (s.contains("LOW")) *level="LOW";
    // KR
    if (s.contains(QString::fromUtf8("지난주").toUpper())) { *from = now.addDays(-7); *to = now; }
    else if (s.contains(QString::fromUtf8("어제").toUpper())) { *from = now.addDays(-1); *to = now; }
    else if (s.contains(QString::fromUtf8("지난 3일").toUpper())) { *from = now.addDays(-3); *to = now; }
    // EN
    else if (s.contains("LAST WEEK")) { *from = now.addDays(-7); *to = now; }
    else if (s.contains("YESTERDAY")) { *from = now.addDays(-1); *to = now; }
    else if (s.contains("PAST 3 DAYS") || s.contains("LAST 3 DAYS")) { *from = now.addDays(-3); *to = now; }
    else { *from = now.addDays(-7); *to = now; }
    return true;
}


HealthDiagDialog::HealthDiagDialog(DBBridge* db, const QDateTime& from, const QDateTime& to, const QString& typeFilter, QWidget* parent)
    : BaseAIDialog(parent)
{
    auto& L = LanguageManager::inst();
    setWindowTitle("Analyst • Sensor Health");
    auto* txt = new QPlainTextEdit(this); txt->setReadOnly(true); lay->addWidget(txt, 1); attachPdfToolbar(txt);

    auto rows = db->sensorHealth(from, to, typeFilter, 200);
    QString s; s += QString("%1 %2 ~ %3\n\n").arg(L.t("Period:"), from.toString("yyyy-MM-dd HH:mm"), to.toString("yyyy-MM-dd HH:mm"));
    s += "flatRatio=unique/total, driftSlope, spikeScore(sd).\n\n";
    for (const auto& r : rows) {
        s += QString(" - [%1/%2] flatRatio=%3, driftSlope=%4, spikeScore=%5\n")
                 .arg(r.sensorId, trType(r.type))
                 .arg(r.flatRatio,  0, 'f', 3)
                 .arg(r.driftSlope, 0, 'f', 3)
                 .arg(r.spikeScore, 0, 'f', 3);
    }
    txt->setPlainText(s);
}

static QString pct(double v, int d=1) { return QString::number(v, 'f', d) + "%"; }

void ReportDialog::autoGenerate() { buildAutoReport(); }

void ReportDialog::buildAutoReport()
{
    auto& L = LanguageManager::inst();
    if (!db_) { txt->setPlainText(L.t("Cannot open file.")); return; }

    QString out; out += QString("%1 %2 ~ %3\n\n").arg(L.t("Period:"), from_.toString("yyyy-MM-dd HH:mm"), to_.toString("yyyy-MM-dd HH:mm"));

    // 1) KPI
    QMap<QString,KPIStats> kpis; db_->kpiAll(from_, to_, &kpis);
    out += "[" + L.t("KPI Summary") + "]\n";
    const QStringList types = {"TEMP","VIB","INTR"};
    for (const auto& t : types) {
        const auto st = kpis.value(t);
        out += QString(" - %1: %2=%3 | %4=%5, %6=%7, %8=%9 | LOW=%10, MED=%11, HIGH=%12\n")
                   .arg(trType(t))
                   .arg(L.t("Count")).arg(st.count)
                   .arg(L.t("Avg")).arg(QString::number(st.avg,'f',2))
                   .arg(L.t("Min")).arg(QString::number(st.mn ,'f',2))
                   .arg(L.t("Max")).arg(QString::number(st.mx ,'f',2))
                   .arg(st.low).arg(st.med).arg(st.high);
    }
    out += "\n";

    // 2) Trend slope (5m)
    out += "[" + L.t("Trend") + "]\n";
    auto slope = [&](const QVector<QPointF>& v)->double{
        if (v.size() < 2) return 0.0; long double n=0,sx=0,sy=0,sxx=0,sxy=0; for (const auto& p: v){ n++; sx+=p.x(); sy+=p.y(); sxx+=p.x()*p.x(); sxy+=p.x()*p.y(); }
        const long double denom = n*sxx - sx*sx; if (denom==0) return 0.0; const long double b = (n*sxy - sx*sy) / denom; return double(b * 3600000.0);
    };
    for (const auto& t : types) {
        auto pts = db_->trendSeries(t, from_, to_, 300);
        out += QString(" - %1: %2 %3 /h (%4 %5)\n")
                   .arg(trType(t))
                   .arg(L.t("Slope"))
                   .arg(QString::number(slope(pts),'f',2))
                   .arg(pts.size())
                   .arg(L.t("points"));
    }
    out += "\n";

    // 3) XAI
    out += "[" + L.t("XAI Highlights") + "]\n";
    const auto highs = db_->topAnomalies(from_, to_, 10);
    if (highs.isEmpty()) {
        out += " - " + L.t("No significant HIGH in range.") + "\n\n";
    } else {
        int tH=0, vH=0, iH=0; for (const auto& d : highs) { if (d.type=="TEMP") ++tH; else if (d.type=="VIB") ++vH; else if (d.type=="INTR") ++iH; }
        out += QString(" - %1 → TEMP:%2, VIB:%3, INTR:%4\n").arg(L.t("Top 10 HIGH distribution")).arg(tH).arg(vH).arg(iH);
        out += " - " + L.t("Checks recommended: TEMP↑(cooling/vent), VIB↑(bearing/alignment), INTR↑(access control)") + "\n\n";
    }

    // 4) Tuning history (audit)
    out += "[" + L.t("Tuning History") + "]\n";
    const QDateTime afrom = from_.addDays(-30);
    auto logs = db_->auditLogs(afrom, to_, QString(), "saveTypeThresholds", false, QString(), 20);
    auto logs2= db_->auditLogs(afrom, to_, QString(), "saveSensorThresholds", false, QString(), 20);
    if (logs.isEmpty() && logs2.isEmpty()) {
        out += " - " + L.t("No related audit logs") + "\n";
    } else {
        auto dump = [&](const QList<DBBridge::AuditLog>& Ls){
            for (const auto& a : Ls) {
                out += QString(" - [%1] %2: %3 (%4)\n")
                .arg(QDateTime::fromMSecsSinceEpoch(a.ts).toString("MM-dd HH:mm"))
                    .arg(a.actor, a.action, a.details);
            }
        };
        dump(logs); dump(logs2);
    }
    out += "\n";

    // 5) HIGH mitigation
    out += "[" + L.t("HIGH mitigation (hypothetical)") + "]\n";
    for (const auto& t : types) {
        TypeThreshold thr{t,50,80,""}; db_->getTypeThreshold(t, &thr);
        KPIStats cur{}; db_->simulateCounts(t, from_, to_, thr.warn, thr.high, &cur);
        KPIStats sim{}; db_->simulateCounts(t, from_, to_, thr.warn*1.05, thr.high*1.05, &sim);
        const double drop = (cur.high>0) ? (double(cur.high - sim.high)/cur.high*100.0) : 0.0;
        out += QString(" - %1: HIGH %2 → %3 (%4)\n").arg(trType(t)).arg(cur.high).arg(sim.high).arg(pct(drop));
    }

    txt->setPlainText(out);
}


TuningImpactDialog::TuningImpactDialog(DBBridge* db,
                                       const QDateTime& from,
                                       const QDateTime& to,
                                       QWidget* parent)
    : BaseAIDialog(parent)
{
    auto& L = LanguageManager::inst();
    setWindowTitle(QStringLiteral(u"Analyst • Tuning Impact"));

    const QStringList types = { QStringLiteral("TEMP"),
                               QStringLiteral("VIB"),
                               QStringLiteral("INTR") };

    auto *curHigh = new QBarSet(L.t("Current HIGH"));
    auto *simHigh = new QBarSet(L.t("Simulated HIGH"));

    QStringList cats;
    for (const auto& t : types) {
        TypeThreshold thr{t, 50, 80, ""};
        db->getTypeThreshold(t, &thr);

        KPIStats cur{}; db->simulateCounts(t, from, to, thr.warn, thr.high, &cur);
        KPIStats sim{}; db->simulateCounts(t, from, to, thr.warn * 1.05, thr.high * 1.05, &sim);

        cats << trType(t);
        *curHigh << cur.high;
        *simHigh << sim.high;
    }

    auto *series = new QBarSeries();
    series->append(curHigh);
    series->append(simHigh);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setTitle(QStringLiteral(u"Tuning Impact — HIGH 감소 효과 시뮬레이션"));

    auto *axX = new QBarCategoryAxis(); axX->append(cats);
    chart->addAxis(axX, Qt::AlignBottom); series->attachAxis(axX);

    auto *axY = new QValueAxis(); axY->setTitleText(L.t("Count"));
    chart->addAxis(axY, Qt::AlignLeft); series->attachAxis(axY);

    auto *view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);
    lay->addWidget(view, 1);
    attachPdfToolbar(view);
}
