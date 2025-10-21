#include "AuditLogDialog.h"
#include "DBBridge.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

static void styleCalendar(QDateTimeEdit* dt)
{
    if (!dt) return;
    dt->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    dt->setCalendarPopup(true);
}

AuditLogDialog::AuditLogDialog(DBBridge* db_,
                               const QString& currentUser_,
                               Role currentRole_,
                               QWidget* parent)
    : QDialog(parent),
    db(db_),
    currentUser(currentUser_),
    currentRole(currentRole_)
{
    setWindowTitle("🧾 Audit Logs");
    resize(900, 560);

    onlyOwn   = (currentRole < Role::Admin);  // Admin 이상: 전체 조회 가능
    canExport = (currentRole >= Role::Admin); // Admin 이상: CSV 내보내기 허용

    auto *root = new QVBoxLayout(this);

    // 필터 바
    auto *flt = new QHBoxLayout();
    dtFrom = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-7), this);
    dtTo   = new QDateTimeEdit(QDateTime::currentDateTime().addDays(+1), this);
    styleCalendar(dtFrom); styleCalendar(dtTo);

    edActor  = new QLineEdit(this);
    edAction = new QLineEdit(this);
    edActor->setPlaceholderText("actor (username)");
    edAction->setPlaceholderText("action (contains)");

    // 권한에 따라 actor 필드 동작
    if (onlyOwn) {
        edActor->setText(currentUser);
        edActor->setEnabled(false);
    }

    auto *btnSearch = new QPushButton("Search", this);
    btnExport = new QPushButton("Export CSV", this);
    btnExport->setEnabled(canExport);

    flt->addWidget(new QLabel("From:", this));
    flt->addWidget(dtFrom);
    flt->addWidget(new QLabel("To:", this));
    flt->addWidget(dtTo);
    flt->addWidget(new QLabel("Actor:", this));
    flt->addWidget(edActor);
    flt->addWidget(new QLabel("Action:", this));
    flt->addWidget(edAction);
    flt->addStretch(1);
    flt->addWidget(btnSearch);
    flt->addWidget(btnExport);

    root->addLayout(flt);

    // 표
    table = new QTableWidget(this);
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels({"ID","Time","Actor","Action","Target","Details"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(table, 1);

    connect(btnSearch, &QPushButton::clicked, this, &AuditLogDialog::refresh);
    connect(btnExport, &QPushButton::clicked, this, &AuditLogDialog::exportCsv);

    refresh();
}

void AuditLogDialog::refresh()
{
    if (!db) return;

    const auto list = db->auditLogs(dtFrom->dateTime(), dtTo->dateTime(),
                                    edActor->text().trimmed(),
                                    edAction->text().trimmed(),
                                    onlyOwn, currentUser, 2000);

    table->setRowCount(list.size());
    for (int i=0;i<list.size();++i) {
        const auto& a = list[i];
        table->setItem(i,0,new QTableWidgetItem(QString::number(a.id)));
        table->setItem(i,1,new QTableWidgetItem(QDateTime::fromMSecsSinceEpoch(a.ts).toString("yyyy-MM-dd HH:mm:ss")));
        table->setItem(i,2,new QTableWidgetItem(a.actor));
        table->setItem(i,3,new QTableWidgetItem(a.action));
        table->setItem(i,4,new QTableWidgetItem(a.target));
        table->setItem(i,5,new QTableWidgetItem(a.details));
    }
    table->resizeColumnsToContents();
}

void AuditLogDialog::exportCsv()
{
    if (!canExport) {
        QMessageBox::warning(this,"권한 없음","Admin 이상만 내보내기가 가능합니다.");
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), QString(), tr("CSV Files (*.csv)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export"), tr("Cannot open file."));
        return;
    }

    QTextStream out(&f);
    out << "ID,Time,Actor,Action,Target,Details\n";
    for (int r = 0; r < table->rowCount(); ++r) {
        QStringList cols;
        for (int c = 0; c < table->columnCount(); ++c) {
            QString cell = table->item(r,c) ? table->item(r,c)->text() : QString();
            cell.replace('"', "\"\"");
            if (cell.contains(',') || cell.contains('"'))
                cell = "\"" + cell + "\"";
            cols << cell;
        }
        out << cols.join(",") << "\n";
    }
    f.close();
    QMessageBox::information(this, "완료", "CSV로 내보냈습니다.");
}
