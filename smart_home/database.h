#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QSqlDatabase>
#include <QVariant>
#include <QVector>
#include <optional>

class Database
{
public:
    static Database& instance();

    bool connect(const QString& host,
                 const QString& dbName,
                 const QString& user,
                 const QString& password,
                 int port = 3306);

    void disconnect();

    // SELECT 쿼리 메서드들
    std::optional<QPair<double, double>> getLatestHomeEnv(const QString& homeId);
    std::optional<QPair<QString, QString>> getLatestFireStatus(const QString& homeId);
    std::optional<int> getLatestSoilMoisture(const QString& homeId);
    std::optional<QString> getLatestPetToilet(const QString& homeId);
    std::optional<QString> getLatestDoorStatus(const QString& homeId);

    std::optional<QVector<QVector<QString>>> getSearchListHome(const QString& firstDate, const QString& lastDate);
    std::optional<QVector<QVector<QString>>> getSearchListFire(const QString& firstDate, const QString& lastDate);
    std::optional<QVector<QVector<QString>>> getSearchListGas(const QString& firstDate, const QString& lastDate);
    std::optional<QVector<QVector<QString>>> getSearchListPlant(const QString& firstDate, const QString& lastDate);
    std::optional<QVector<QVector<QString>>> getSearchListPet(const QString& firstDate, const QString& lastDate);

private:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    QSqlDatabase m_db;
};

#endif // DATABASE_H
