#ifndef CURRENTSESSIONSDIALOG_H
#define CURRENTSESSIONSDIALOG_H

#include <QDialog>

class QTableWidget;
class QPushButton;

class CurrentSessionsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CurrentSessionsDialog(QWidget* parent=nullptr);
public slots:
    void refresh();

private:
    QTableWidget* tbl {nullptr};
    QPushButton*  btnRefresh {nullptr};
};

#endif // CURRENTSESSIONSDIALOG_H
