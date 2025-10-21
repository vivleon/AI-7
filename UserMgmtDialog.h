#ifndef UserMgmtDialog_H
#define UserMgmtDialog_H

#pragma once
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QVector>
#include "Role.h"

class DBBridge;

class UserMgmtDialog : public QDialog {
    Q_OBJECT
public:
    explicit UserMgmtDialog(DBBridge* db, const QString& actor, QWidget* parent=nullptr);

protected:
    void closeEvent(QCloseEvent* e) override;

private slots:
    void onResetPw();
    void onSave();
    void onClose();
    void onCellChanged(int row, int col);
    void onAddUser();

private:
    struct Row { QString username; Role role; bool active; };
    void load();
    void setRow(int row, const Row& r);
    Row   getRow(int row) const;
    bool  applyChanges(QString* errOut);
    bool  confirmLoseChanges(const QString& what);
    void  markDirty(bool d);

    DBBridge* db_;
    QString actor_;
    QTableWidget* tbl_;
    QPushButton *btnReset_, *btnSave_, *btnClose_;
    QVector<Row> original_;   // 로딩 당시 스냅샷
    QPushButton* btnAdd_{};
    bool dirty_ = false;
};


#endif // UserMgmtDialog_H
