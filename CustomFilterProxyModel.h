// CustomFilterProxyModel.h
#ifndef CUSTOMFILTERPROXYMODEL_H
#define CUSTOMFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QDateTime>
#include <optional>
#include <QString>

class CustomFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit CustomFilterProxyModel(QObject* parent = nullptr);

    Q_SLOT void setTypeFilter(const QString& v);
    Q_SLOT void setLevelFilter(const QString& v);
    Q_SLOT void setIdSubstring(const QString& v);
    Q_SLOT void setValueMin(std::optional<double> v);
    Q_SLOT void setValueMax(std::optional<double> v);
    Q_SLOT void setFromTo(const QDateTime& from, const QDateTime& to, bool enabled);
    Q_SLOT void setLocationFilter(const QString& loc);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

private:
    QString typeFilter_  = "All";
    QString levelFilter_ = "All";
    QString idSubstr_;
    std::optional<double> vmin_;
    std::optional<double> vmax_;
    bool      timeEnabled_ = false;
    QDateTime fromDt_;
    QDateTime toDt_;
    QString locationFilter_ = "All";

    bool endIsNow_ = false;
};

#endif // CUSTOMFILTERPROXYMODEL_H
