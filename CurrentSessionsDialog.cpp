#include "CurrentSessionsDialog.h"
#include "SessionService.h"
#include "LanguageManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QDateTime>

CurrentSessionsDialog::CurrentSessionsDialog(QWidget* parent)
    : QDialog(parent)
{
    auto& L = LanguageManager::inst();
    setWindowTitle(L.t("👥 Active Sessions"));
    resize(760, 440);

    auto *root = new QVBoxLayout(this);
    tbl = new QTableWidget(this);
    tbl->setColumnCount(5);
    tbl->setHorizontalHeaderLabels({L.t("User"),L.t("Client"),L.t("Created"),L.t("Last Seen"),L.t("ID")});
    tbl->horizontalHeader()->setStretchLastSection(true);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(tbl, 1);

    auto *row = new QHBoxLayout();
    row->addStretch(1);
    btnRefresh = new QPushButton(L.t("Refresh"), this);
    row->addWidget(btnRefresh);
    root->addLayout(row);

    connect(btnRefresh, &QPushButton::clicked, this, &CurrentSessionsDialog::refresh);
    refresh();
}

void CurrentSessionsDialog::refresh()
{
    SessionService s; auto& L = LanguageManager::inst();
    const auto list = s.listActive(120000);
    tbl->setRowCount(list.size()); int r = 0;
    for (const auto& it : list) {
        tbl->setItem(r, 0, new QTableWidgetItem(it.username));
        tbl->setItem(r, 1, new QTableWidgetItem(it.clientId));
        tbl->setItem(r, 2, new QTableWidgetItem(QDateTime::fromMSecsSinceEpoch(it.createdAt).toString("yyyy-MM-dd HH:mm:ss")));
        tbl->setItem(r, 3, new QTableWidgetItem(QDateTime::fromMSecsSinceEpoch(it.lastSeen).toString("yyyy-MM-dd HH:mm:ss")));
        tbl->setItem(r, 4, new QTableWidgetItem(QString::number(it.id)));
        ++r;
    }
    tbl->resizeColumnsToContents();
}
