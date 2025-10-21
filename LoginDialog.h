#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QPixmap>


QT_BEGIN_NAMESPACE
class QResizeEvent;
namespace Ui { class LoginDialog; }
QT_END_NAMESPACE

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    QString username() const;
    QString password() const;

public slots:
    void accept() override;  // ✅ 선언 추가 (정의와 일치)

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void rescaleBanner();
    void addLanguageButtons();
    void applyLanguageTexts();
    Ui::LoginDialog *ui;
    QWidget* langDock {nullptr};
    QPushButton *btnKo {nullptr}, *btnEn {nullptr};

    QPixmap bannerSrc_;
};

#endif // LOGINDIALOG_H
