#ifndef AIDIALOGS_H
#define AIDIALOGS_H

#include <QDialog>
#include <QDateTime>

class QVBoxLayout;
class QPlainTextEdit;
class QWidget;
class DBBridge;

class BaseAIDialog : public QDialog {
    Q_OBJECT
public:
    explicit BaseAIDialog(QWidget* parent = nullptr);
    virtual ~BaseAIDialog() = default;

protected slots:
    void exportPdf();
    void printDoc();

protected:
    void attachPdfToolbar(QWidget* targetForRender);
    QWidget* renderTarget() const { return render_; }

protected:
    QVBoxLayout* lay {nullptr};

private:
    QWidget* render_ {nullptr};
};

class KPIDialog : public BaseAIDialog {
    Q_OBJECT
public:
    explicit KPIDialog(DBBridge* db,
                       const QDateTime& from,
                       const QDateTime& to,
                       QWidget* parent = nullptr);
private:
    void buildChart(DBBridge* db, const QDateTime& from, const QDateTime& to);
};

class KPISummaryDialog : public BaseAIDialog {
    Q_OBJECT
public:
    explicit KPISummaryDialog(DBBridge* db,
                              const QDateTime& from,
                              const QDateTime& to,
                              QWidget* parent = nullptr);
};

class TrendDialog : public BaseAIDialog {
    Q_OBJECT
public:
    explicit TrendDialog(DBBridge* db,
                         const QDateTime& from,
                         const QDateTime& to,
                         int bucketSec,
                         QWidget* parent = nullptr);
};

class XAIDialog : public BaseAIDialog {
    Q_OBJECT
public:
    explicit XAIDialog(DBBridge* db,
                       const QDateTime& from,
                       const QDateTime& to,
                       QWidget* parent = nullptr);
};

class ReportDialog : public BaseAIDialog {
    Q_OBJECT
public:
    explicit ReportDialog(DBBridge* db,
                          const QDateTime& from,
                          const QDateTime& to,
                          QWidget* parent = nullptr);
private slots:
    void autoGenerate();
private:
    void buildAutoReport();
    QPlainTextEdit* txt {nullptr};
    DBBridge* db_ {nullptr};
    QDateTime from_, to_;
};

class RuleSimDialog : public BaseAIDialog {
    Q_OBJECT
public:
    explicit RuleSimDialog(DBBridge* db,
                           const QDateTime& from,
                           const QDateTime& to,
                           QWidget* parent=nullptr);
};

class RCADialog : public BaseAIDialog {
    Q_OBJECT
public:
    explicit RCADialog(DBBridge* db,
                       const QDateTime& from,
                       const QDateTime& to,
                       const QString& typeFilter,
                       QWidget* parent=nullptr);
};

class NLQDialog : public BaseAIDialog {
    Q_OBJECT
public:
    explicit NLQDialog(DBBridge* db, const QString& query, QWidget* parent=nullptr);
};

class HealthDiagDialog : public BaseAIDialog {
    Q_OBJECT
public:
    explicit HealthDiagDialog(DBBridge* db,
                              const QDateTime& from,
                              const QDateTime& to,
                              const QString& typeFilter,
                              QWidget* parent=nullptr);
};

class TuningImpactDialog : public BaseAIDialog {
    Q_OBJECT
public:
    explicit TuningImpactDialog(DBBridge* db,
                                const QDateTime& from,
                                const QDateTime& to,
                                QWidget* parent=nullptr);
};

#endif // AIDIALOGS_H
