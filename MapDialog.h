#ifndef MAPDIALOG_H
#define MAPDIALOG_H

#include <QDialog>
class QQuickWidget;
class QQuickItem;

class MapDialog : public QDialog {
    Q_OBJECT
public:
    explicit MapDialog(QWidget* parent = nullptr);
    ~MapDialog();

    // ✅ 기존 시그니처 유지
    Q_INVOKABLE void setLocation(double lat, double lng);
    Q_INVOKABLE void setInfoText(const QString& text);

private:
    QQuickWidget* quick {nullptr};
    QQuickItem*   root  {nullptr}; // QML root object
    QWidget*      container {nullptr};

    void initQml();
};

#endif // MAPDIALOG_H
