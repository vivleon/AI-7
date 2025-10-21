#include "UserMgmtDialog.h"
#include "DBBridge.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QCloseEvent>

static QString roleToStr(Role r){
    switch(r){
    case Role::Viewer: return "Viewer";
    case Role::Operator: return "Operator";
    case Role::Analyst: return "Analyst";
    case Role::Admin: return "Admin";
    case Role::SuperAdmin: return "SuperAdmin";
    }
    return "Viewer";
}
static Role strToRole(const QString& s){
    if (s=="Operator") return Role::Operator;
    if (s=="Analyst")  return Role::Analyst;
    if (s=="Admin")    return Role::Admin;
    if (s=="SuperAdmin") return Role::SuperAdmin;
    return Role::Viewer;
}

UserMgmtDialog::UserMgmtDialog(DBBridge* db, const QString& actor, QWidget* parent)
    : QDialog(parent), db_(db), actor_(actor)
{
    setWindowTitle("User Management");
    setModal(true);
    setMinimumSize(720, 420);

    tbl_ = new QTableWidget(this);
    tbl_->setColumnCount(3);
    tbl_->setHorizontalHeaderLabels({"Username","Role","Active"});
    tbl_->setAlternatingRowColors(true);
    tbl_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tbl_->setSelectionMode(QAbstractItemView::SingleSelection);
    tbl_->horizontalHeader()->setStretchLastSection(false);
    tbl_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);           // Username
    tbl_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);  // Role
    tbl_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);  // Active
    tbl_->verticalHeader()->setVisible(false);
    tbl_->setEditTriggers(QAbstractItemView::NoEditTriggers); // 위젯(콤보/체크박스)로만 수정
    connect(tbl_, &QTableWidget::cellChanged, this, &UserMgmtDialog::onCellChanged);

    btnAdd_  = new QPushButton("Add User…", this);    // ⬅️ 추가
    btnReset_ = new QPushButton("Reset Password…", this);
    btnSave_  = new QPushButton("Save", this);
    btnClose_ = new QPushButton("Close", this);

    auto *btns = new QHBoxLayout();
    btns->addStretch();
    btns->addWidget(btnAdd_);      // ⬅️ 추가 (Reset 앞에 배치)
    btns->addWidget(btnReset_);
    btns->addWidget(btnSave_);
    btns->addWidget(btnClose_);


    auto *root = new QVBoxLayout(this);
    root->addWidget(tbl_);
    root->addLayout(btns);
    setLayout(root);

    connect(btnAdd_,  &QPushButton::clicked, this, &UserMgmtDialog::onAddUser);  // ⬅️ 추가
    connect(btnReset_, &QPushButton::clicked, this, &UserMgmtDialog::onResetPw);
    connect(btnSave_,  &QPushButton::clicked, this, &UserMgmtDialog::onSave);
    connect(btnClose_, &QPushButton::clicked, this, &UserMgmtDialog::onClose);

    load();
    markDirty(false);
}

void UserMgmtDialog::load()
{
    tbl_->blockSignals(true);
    tbl_->setRowCount(0);
    original_.clear();

    QString err;
    const auto list = db_ ? db_->listUsersEx(&err) : decltype(db_->listUsersEx(nullptr)){};
    if (!err.isEmpty()) QMessageBox::warning(this, "Error", err);

    tbl_->setRowCount(list.size());
    for (int i=0;i<list.size();++i){
        Row r{ list[i].username, list[i].role, list[i].active };
        original_.push_back(r);
        setRow(i, r);
    }
    tbl_->blockSignals(false);
}

void UserMgmtDialog::setRow(int row, const Row& r)
{
    // Username (read-only)
    auto *u = new QTableWidgetItem(r.username);
    u->setFlags(u->flags() & ~Qt::ItemIsEditable);
    tbl_->setItem(row, 0, u);

    // Role (combobox)
    auto *combo = new QComboBox(tbl_);
    combo->addItems({"Viewer","Operator","Analyst","Admin","SuperAdmin"});
    combo->setCurrentText(roleToStr(r.role));
    combo->setMinimumWidth(120);
    QObject::connect(combo, &QComboBox::currentTextChanged, this, [this,row](const QString&){
        markDirty(true);
        // 시각적 변경을 위해 cellChanged 유발 대신 바로 색/상태만 조정
    });
    tbl_->setCellWidget(row, 1, combo);

    // Active (checkbox + "ON" 초록 표시)
    QWidget* w = new QWidget(tbl_);
    auto *lay = new QHBoxLayout(w); lay->setContentsMargins(6,2,6,2);
    auto *cb  = new QCheckBox("ON", w);
    cb->setChecked(r.active);
    cb->setStyleSheet("QCheckBox { font-weight:600; color:#0a7d24; }");
    QObject::connect(cb, &QCheckBox::toggled, this, [this](bool){ markDirty(true); });
    lay->addWidget(cb, 0, Qt::AlignLeft);
    w->setLayout(lay);
    tbl_->setCellWidget(row, 2, w);
}

UserMgmtDialog::Row UserMgmtDialog::getRow(int row) const
{
    Row r;
    r.username = tbl_->item(row,0)->text();
    if (auto *combo = qobject_cast<QComboBox*>(tbl_->cellWidget(row,1)))
        r.role = strToRole(combo->currentText());
    if (auto *w = tbl_->cellWidget(row,2)){
        if (auto *cb = w->findChild<QCheckBox*>())
            r.active = cb->isChecked();
    }
    return r;
}

void UserMgmtDialog::onCellChanged(int, int){ markDirty(true); }

void UserMgmtDialog::markDirty(bool d)
{
    dirty_ = d;
    btnSave_->setEnabled(dirty_);
}

bool UserMgmtDialog::confirmLoseChanges(const QString& what)
{
    if (!dirty_) return true;
    const auto ret = QMessageBox::question(
        this, what,
        "변경 사항이 저장되지 않았습니다.\n계속 하시겠습니까?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return ret == QMessageBox::Yes;
}


void UserMgmtDialog::onAddUser()
{
    bool ok = false;

    const QString username = QInputDialog::getText(
        this, "Add User", "Username:", QLineEdit::Normal, QString(), &ok);
    if (!ok || username.trimmed().isEmpty()) return;

    const QString password = QInputDialog::getText(
        this, "Add User", "Temporary Password:", QLineEdit::Password, QString(), &ok);
    if (!ok || password.isEmpty()) return;

    const QStringList roles = {"Viewer","Operator","Analyst","Admin","SuperAdmin"};
    const QString roleStr = QInputDialog::getItem(
        this, "Add User", "Role:", roles, 0, false, &ok);
    if (!ok || roleStr.isEmpty()) return;

    const auto role = strToRole(roleStr);

    // 확인 한 번 더
    const auto ret = QMessageBox::question(
        this, "확인",
        QString("다음 정보로 사용자를 생성할까요?\n\n- Username: %1\n- Role: %2")
            .arg(username, roleStr),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ret != QMessageBox::Yes) return;

    QString err;
    if (!db_->createUser(actor_, username.trimmed(), password, role, &err)) {
        QMessageBox::warning(this, "실패", err.isEmpty()? "사용자 생성 실패" : err);
        return;
    }

    QMessageBox::information(this, "완료", "사용자가 생성되었습니다.");
    load();             // 목록 새로고침
    markDirty(false);   // 외부 변경이므로 저장 필요 없음
}



void UserMgmtDialog::onResetPw()
{
    const int row = tbl_->currentRow();
    if (row < 0) { QMessageBox::information(this,"안내","대상 사용자를 선택하세요."); return; }
    const QString target = tbl_->item(row,0)->text();

    bool ok=false;
    const QString pw = QInputDialog::getText(this,"비밀번호 초기화",
                                             QString("사용자 [%1]의 새 임시 비밀번호:").arg(target),
                                             QLineEdit::Password, QString(), &ok);
    if (!ok || pw.isEmpty()) return;

    // 한번 더 확인
    const auto ret = QMessageBox::question(this, "확인",
                                           QString("[%1]의 비밀번호를 초기화 하시겠습니까?").arg(target),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QString err;
    if (!db_->resetUserPassword(actor_, target, pw, &err)){
        QMessageBox::warning(this, "실패", err.isEmpty()? "비밀번호 초기화 실패" : err);
        return;
    }
    QMessageBox::information(this, "완료", "비밀번호가 초기화되었습니다.");
}

bool UserMgmtDialog::applyChanges(QString* errOut)
{
    int changed = 0;
    QString err;

    // 원본과 비교하여 Role/Active 변경만 적용
    for (int r=0; r<tbl_->rowCount(); ++r){
        const Row now = getRow(r);
        const Row old = original_.value(r);

        // Role
        if (now.role != old.role){
            if (!db_->setUserRole(actor_, now.username, now.role, &err)){
                if (errOut) *errOut = err;
                return false;
            }
            ++changed;
        }
        // Active
        if (now.active != old.active){
            if (!db_->setUserActive(actor_, now.username, now.active, &err)){
                if (errOut) *errOut = err;
                return false;
            }
            ++changed;
        }
    }
    if (changed==0) {
        if (errOut) *errOut = QString();
    }
    return true;
}

void UserMgmtDialog::onSave()
{
    const auto ret = QMessageBox::question(this, "저장 확인",
                                           "변경 사항을 저장하시겠습니까?",
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ret != QMessageBox::Yes) return;

    QString err;
    if (!applyChanges(&err)){
        QMessageBox::warning(this, "실패", err.isEmpty()? "저장 실패" : err);
        return;
    }
    QMessageBox::information(this, "완료", "저장되었습니다.");
    load();             // 최신 상태 재로딩
    markDirty(false);
}

void UserMgmtDialog::onClose()
{
    if (!confirmLoseChanges("닫기")) return;
    accept();
}

void UserMgmtDialog::closeEvent(QCloseEvent* e)
{
    if (!confirmLoseChanges("닫기")) { e->ignore(); return; }
    e->accept();
}
