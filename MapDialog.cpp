#include "MapDialog.h"
#include <QQuickWidget>
#include <QQmlContext>
#include <QQuickItem>
#include <QVBoxLayout>
#include <QMetaObject>
#include <QVariant>
#include <QDebug>
#include "ui_MapDialog.h"

class Ui_shim_MapDialog : public Ui_MapDialog {}; // uic 생성 헤더 사용

MapDialog::MapDialog(QWidget* parent)
    : QDialog(parent)
{
    auto ui = new Ui_shim_MapDialog();
    ui->setupUi(this);
    container = ui->container;

    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(800, 520);

    quick = new QQuickWidget(this);
    quick->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quick->setSource(QUrl("qrc:/qml/MapView.qml"));

    auto *lay = new QVBoxLayout(container);
    lay->setContentsMargins(0,0,0,0);
    lay->addWidget(quick);

    initQml();
}

MapDialog::~MapDialog(){}

void MapDialog::initQml()
{
    root = quick->rootObject();   // QQuickItem*
    if (!root) {
        qWarning() << "[MapDialog] QML root not ready";
    }
}

void MapDialog::setLocation(double lat, double lng)
{
    if (!root) initQml();
    if (!root) return;
    QObject* obj = root;  // ✅ QQuickItem* → QObject* 업캐스팅
    QMetaObject::invokeMethod(obj, "centerOnCoordinate",
                              Q_ARG(QVariant, lat), Q_ARG(QVariant, lng));
}

void MapDialog::setInfoText(const QString& text)
{
    if (!root) initQml();
    if (!root) return;
    QMetaObject::invokeMethod(root, "setInfo",
                              Q_ARG(QVariant, text));
}
