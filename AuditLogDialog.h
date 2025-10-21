#ifndef AUDITLOGDIALOG_H
#define AUDITLOGDIALOG_H

#include <QDialog>
#include <QDateTime>

#include "Role.h"

class DBBridge;
class QLineEdit;
class QDateTimeEdit;
class QTableWidget;
class QPushButton;

class AuditLogDialog : public QDialog {
    Q_OBJECT
public:
    explicit AuditLogDialog(DBBridge* db,
                            const QString& currentUser,
                            Role currentRole,
                            QWidget* parent = nullptr);
private slots:
    void refresh();
    void exportCsv();

private:
    DBBridge*    db {};
    QString      currentUser;
    Role         currentRole {Role::Viewer};
    bool         onlyOwn {true};
    bool         canExport {false};

    QDateTimeEdit* dtFrom {};
    QDateTimeEdit* dtTo   {};
    QLineEdit*     edActor {};
    QLineEdit*     edAction {};
    QTableWidget*  table {};
    QPushButton*   btnExport {};
};

#endif // AUDITLOGDIALOG_H
