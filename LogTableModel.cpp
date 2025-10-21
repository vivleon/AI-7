#include "LogTableModel.h"
#include "LanguageManager.h"
#include <QDateTime>
#include <limits>
#include <QString>


namespace {
QString nearestPlace(double lat, double lng)
{
    struct P { const char* name; double lat; double lng; };
    static const P places[] = {
        {"경복궁", 37.579617,126.977041}, {"창덕궁", 37.579414,126.991102},
        {"창경궁", 37.582604,126.994546}, {"덕수궁", 37.565804,126.975146},
        {"경희궁", 37.571785,126.959206}, {"경복궁-서문", 37.578645,126.976829},
        {"경복궁-북측", 37.579965,126.978345},{"창덕궁-서측", 37.581112,126.992230},
        {"덕수궁-동측", 37.564990,126.976000},{"경희궁-남측", 37.572500,126.960800}
    };
    double best = std::numeric_limits<double>::infinity();
    const char* bestName = "Unknown";
    for (const auto& p : places) {
        const double d2 = (lat - p.lat)*(lat - p.lat) + (lng - p.lng)*(lng - p.lng);
        if (d2 < best) { best = d2; bestName = p.name; }
    }
    return QString::fromUtf8(bestName);
}
} // namespace


LogTableModel::LogTableModel(QObject* parent) : QAbstractTableModel(parent) {}

int LogTableModel::rowCount(const QModelIndex&) const { return rows.size(); }
int LogTableModel::columnCount(const QModelIndex&) const { return COL__COUNT; }

QVariant LogTableModel::headerData(int section, Qt::Orientation o, int role) const {
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    auto& L = LanguageManager::inst();
    switch (section) {
        case COL_ID:        return L.t("SensorId");
        case COL_LOCATION:  return L.t("Location");       // ✅
        case COL_TYPE:      return L.t("Type");
        case COL_VALUE:     return L.t("Value");
        case COL_LEVEL:     return L.t("Level");
        case COL_TIME:      return L.t("Time");
        case COL_DATETIME:  return L.t("DateTime");
        case COL_LAT:       return "Lat";
        case COL_LNG:       return "Lng";
        default:            return {};
    }
}

static QString trType(const QString& typ) {
    const auto& L = LanguageManager::inst();
    if (typ == "TEMP") return L.t("TEMP");
    if (typ == "VIB")  return L.t("VIB");
    if (typ == "INTR") return L.t("INTR");
    return typ;
}
static QString trLevel(const QString& lv) {
    const auto& L = LanguageManager::inst();
    if (lv == "LOW") return L.t("LOW");
    if (lv == "MEDIUM") return L.t("MEDIUM");
    if (lv == "HIGH") return L.t("HIGH");
    return lv;
}

QVariant LogTableModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid()) return {};
    if (role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::UserRole) return {};
    const auto& d = rows.at(idx.row());
    switch (idx.column()) {
    case COL_ID:       return d.sensorId;
    case COL_LOCATION: return nearestPlace(d.latitude, d.longitude); // ✅ 새 컬럼
    case COL_TYPE:     return d.type;
    case COL_VALUE:    return d.value;
    case COL_LEVEL:    return d.level;
    case COL_TIME:     return QDateTime::fromMSecsSinceEpoch(d.timestamp).toString("HH:mm:ss");
    case COL_DATETIME: return QDateTime::fromMSecsSinceEpoch(d.timestamp).toString("yyyy-MM-dd HH:mm:ss");
    case COL_LAT:      return d.latitude;
    case COL_LNG:      return d.longitude;
    }
    return {};
}

void LogTableModel::append(const SensorData& d) {
    const int r = rows.size();
    beginInsertRows({}, r, r);
    rows.push_back(d);
    endInsertRows();
}

