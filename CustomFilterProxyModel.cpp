// CustomFilterProxyModel.cpp
#include "CustomFilterProxyModel.h"
#include <QAbstractItemModel>
#include <QVariant>


#include "LogTableModel.h"

#include <cmath> // std::abs

static inline bool isAll(const QString& s) {
    const QString t = s.trimmed();
    if (t.isEmpty()) return true;
    if (t.compare("All", Qt::CaseInsensitive) == 0) return true;
    if (t == QString::fromUtf8("전체")) return true;  // 다국어 허용
    return false;
}

CustomFilterProxyModel::CustomFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void CustomFilterProxyModel::setTypeFilter(const QString& v)      { typeFilter_ = v;  invalidateFilter(); }
void CustomFilterProxyModel::setLevelFilter(const QString& v)     { levelFilter_ = v; invalidateFilter(); }
void CustomFilterProxyModel::setIdSubstring(const QString& v)     { idSubstr_ = v;    invalidateFilter(); }
void CustomFilterProxyModel::setValueMin(std::optional<double> v) { vmin_ = v;        invalidateFilter(); }
void CustomFilterProxyModel::setValueMax(std::optional<double> v) { vmax_ = v;        invalidateFilter(); }
void CustomFilterProxyModel::setFromTo(const QDateTime& f,
                                       const QDateTime& t,
                                       bool en)
{
    fromDt_ = f;
    toDt_   = t;
    timeEnabled_ = en;

    // 상한이 '지금'으로 보이면(±2초) 실시간 상한으로 처리
    endIsNow_ = en && std::abs(t.msecsTo(QDateTime::currentDateTime())) <= 2000;

    invalidateFilter();
}
void CustomFilterProxyModel::setLocationFilter(const QString& loc)
{
    locationFilter_ = loc;
    invalidateFilter();
}

// 0: SensorId, 1: Type, 2: Value, 3: Level, 4: Time(HH:mm:ss), 5: DateTime(yyyy-MM-dd HH:mm:ss), 6: Lat, 7: Lng
bool CustomFilterProxyModel::filterAcceptsRow(int source_row,
                                              const QModelIndex& source_parent) const
{
    // 1) 모델이 LogTableModel이면 SensorData로 직접 판단 (컬럼 순서 무시)
    if (auto lm = qobject_cast<const LogTableModel*>(sourceModel())) {
        const SensorData& d = lm->at(source_row);

        // 타입/레벨
        if (!isAll(typeFilter_)  && d.type  != typeFilter_)  return false;
        if (!isAll(levelFilter_) && d.level != levelFilter_) return false;

        // ID 부분일치
        if (!idSubstr_.isEmpty() &&
            !d.sensorId.contains(idSubstr_, Qt::CaseInsensitive)) return false;

        // 값 범위
        if (vmin_ && d.value < *vmin_) return false;
        if (vmax_ && d.value > *vmax_) return false;

        // 시간 범위 (상한 포함)
        if (timeEnabled_) {
            const qint64 ts = d.timestamp;
            const qint64 fromMs = fromDt_.toMSecsSinceEpoch();
            const QDateTime to = endIsNow_
                                     ? QDateTime::currentDateTime().addSecs(1)
                                     : toDt_;
            const qint64 toMs = to.toMSecsSinceEpoch();
            if (ts < fromMs || ts > toMs) return false;
        }

        // 위치 필터 (모델에 위치 문자열 컬럼이 있는 경우)
        if (!isAll(locationFilter_)) {
            const QModelIndex idxLoc =
                lm->index(source_row, LogTableModel::COL_LOCATION, source_parent);
            const QString loc = lm->data(idxLoc, Qt::DisplayRole).toString();
            if (loc != locationFilter_) return false;
        }
        return true;
    }

    // 2) (예비) 일반 모델 fallback — 주석 기준 컬럼 사용 + 버그 수정
    const QAbstractItemModel* m = sourceModel();
    if (!m) return true;

    const auto idxId   = m->index(source_row, 0, source_parent); // SensorId
    const auto idxType = m->index(source_row, 1, source_parent); // Type
    const auto idxVal  = m->index(source_row, 2, source_parent); // ✅ Value = 2
    const auto idxLvl  = m->index(source_row, 3, source_parent); // ✅ Level = 3
    const auto idxDT   = m->index(source_row, 5, source_parent); // DateTime

    const QString sid  = m->data(idxId).toString();
    const QString typ  = m->data(idxType).toString();
    const QString lvl  = m->data(idxLvl).toString();
    const double  val  = m->data(idxVal).toDouble();

    if (!isAll(typeFilter_)  && typ != typeFilter_)  return false;
    if (!isAll(levelFilter_) && lvl != levelFilter_) return false;

    if (!idSubstr_.isEmpty() &&
        !sid.contains(idSubstr_, Qt::CaseInsensitive)) return false;

    if (vmin_ && val < *vmin_) return false;
    if (vmax_ && val > *vmax_) return false;

    if (timeEnabled_) {
        QString s = m->data(idxDT).toString();
        QDateTime dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm:ss");
        if (!dt.isValid()) dt = QDateTime::fromString(s, Qt::ISODate);
        if (!dt.isValid()) return false;

        const QDateTime to = endIsNow_
                                 ? QDateTime::currentDateTime().addSecs(1)
                                 : toDt_;
        if (dt < fromDt_ || dt > to) return false;
    }

    if (!isAll(locationFilter_)) {
        const int colLoc = LogTableModel::COL_LOCATION; // 상수 제공 시
        const QString loc = m->index(source_row, colLoc, source_parent).data().toString();
        if (loc != locationFilter_) return false;
    }
    return true;
}
