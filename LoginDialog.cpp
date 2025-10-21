#include "LoginDialog.h"
#include "ui_LoginDialog.h"
#include "LanguageManager.h"  // ✅ 다국어 매니저
#include <QSettings>          // ✅ 설정 저장/로드
#include <QMessageBox>
#include <QResizeEvent>
#include <QPixmap>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QHBoxLayout>


LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::LoginDialog)
{
    ui->setupUi(this);

    // ✅ 원본을 한 번만 로드해서 보관
    bannerSrc_ = QPixmap(":/img/vigiledge_banner.png");

    // 1) 로고 픽스맵 주입 (qrc 기준 경로)
    ui->bannerLabel->setAlignment(Qt::AlignCenter);
    ui->bannerLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->bannerLabel->setMinimumHeight(120);
    ui->bannerLabel->setScaledContents(false); // 비율 유지

    addLanguageButtons();      // ✅ 추가
    applyLanguageTexts();      // ✅ 다국어 텍스트 반영


    // 2) 버튼 시그널
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &LoginDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &LoginDialog::reject);

    rescaleBanner();
}
LoginDialog::~LoginDialog()
{
    delete ui;
}

QString LoginDialog::username() const { return ui->lineEditUsername->text(); }
QString LoginDialog::password() const { return ui->lineEditPassword->text(); }


// ✅ 항상 bannerSrc_에서만 스케일 (라벨의 pixmap()을 재사용하지 않음)
void LoginDialog::rescaleBanner()
{
    auto *lab = ui->bannerLabel;
    if (!lab || bannerSrc_.isNull()) return;

    const int w = lab->width();
    if (w <= 0) return;

    const int h = int(w * (bannerSrc_.height() / double(bannerSrc_.width())));
    lab->setFixedHeight(qMax(h, 120));

    // ✅ HiDPI 선명도: 디바이스 픽셀 비율 고려하여 스케일
    const qreal dpr = this->devicePixelRatioF();
    QPixmap scaled = bannerSrc_.scaled(int(w * dpr), int(h * dpr),
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);

    lab->setPixmap(scaled);
}


void LoginDialog::addLanguageButtons()
{
    // ... 기존 제거
    auto *btnKo = new QPushButton(QStringLiteral("🇰🇷"), this);
    auto *btnEn = new QPushButton(QStringLiteral("🇺🇸"), this);
    btnKo->setFixedHeight(24); btnEn->setFixedHeight(24);

    connect(btnKo, &QPushButton::clicked, this, [this]{
        QSettings s("VigilEdge","VigilEdge");
        s.setValue("ui/lang","ko");
        LanguageManager::inst().set(LanguageManager::Lang::Korean);
        applyLanguageTexts();
    });
    connect(btnEn, &QPushButton::clicked, this, [this]{
        QSettings s("VigilEdge","VigilEdge");
        s.setValue("ui/lang","en");
        LanguageManager::inst().set(LanguageManager::Lang::English);
        applyLanguageTexts();
    });

    // ✅ 하단 좌측에 붙이고, OK/Cancel은 우측
    if (auto *v = qobject_cast<QVBoxLayout*>(this->layout())) {
        v->removeWidget(ui->buttonBox);
        auto *row = new QHBoxLayout();
        auto *left = new QWidget(this);
        auto *hl = new QHBoxLayout(left);
        hl->setContentsMargins(0,0,0,0);
        hl->setSpacing(6);
        hl->addWidget(btnKo);
        hl->addWidget(btnEn);
        row->addWidget(left, 0, Qt::AlignLeft);
        row->addStretch();
        row->addWidget(ui->buttonBox, 0, Qt::AlignRight);
        v->addLayout(row);
    }
}


// resizeEvent() 안에 좌표 보정 추가 (맨 아래 줄에 이 두 줄만 추가해도 됩니다)
void LoginDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    rescaleBanner();
    if (langDock) langDock->move(10, height() - langDock->height() - 10); // ← 좌측하단 고정
}

void LoginDialog::applyLanguageTexts()
{
    auto &L = LanguageManager::inst();

    setWindowTitle(L.t("Login"));

    if (ui->lineEditUsername)
        ui->lineEditUsername->setPlaceholderText(L.t("User ID"));
    if (ui->lineEditPassword)
        ui->lineEditPassword->setPlaceholderText(L.t("Password"));

    if (auto *ok = ui->buttonBox->button(QDialogButtonBox::Ok))
        ok->setText(L.t("OK"));
    if (auto *cc = ui->buttonBox->button(QDialogButtonBox::Cancel))
        cc->setText(L.t("Cancel"));
}


void LoginDialog::accept()
{
    const QString u = ui->lineEditUsername->text().trimmed();
    const QString p = ui->lineEditPassword->text();

    if (u.isEmpty() || p.isEmpty()) {
        auto &L = LanguageManager::inst();
        QMessageBox::warning(this, L.t("Login"), L.t("Please enter user ID and password."));
        return; // ✅ 경고 후 닫히지 않음 (요구사항 #9)
    }
    QDialog::accept();
}


