#pragma once
#ifndef DBBRIDGE_H
#define DBBRIDGE_H

#pragma once
#include <QtCore>
#include <QtSql>
#include <QSqlError>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVector>
#include <QPointF>
#include <QSet>

#include "Role.h"
#include "Thresholds.h"
#include "SensorData.h"

struct AuthResult {
    bool ok{false};
    int userId{0};
    QString username;
    Role role{Role::Viewer};
    QString error;
};

struct KPIStats { int count{}; double avg{}, mn{}, mx{}; int low{}, med{}, high{}; };
struct UserInfo { QString username; Role role{Role::Viewer}; bool active{true}; };

class DBBridge : public QObject {
    Q_OBJECT
public:
    explicit DBBridge(QObject* parent=nullptr);
    ~DBBridge();

    // 풀 구성(최초 연결 점검)
    bool connectDB(const QString& host, int port, const QString& db, const QString& user, const QString& password, bool useSSL);
    bool connectWithCandidates(const QList<QPair<QString,int>>& hosts, const QString& dbName, const QString& user, const QString& pw, bool useSSL, QString* err);

    // 데이터
    void insertSensorData(const SensorData& d);
    QList<SensorData> fetchRecentData(int limit);

    // 사용자/권한
    QList<UserInfo> listUsersEx(QString* errOut=nullptr);
    bool setUserActive(const QString& actorUsername, const QString& targetUsername, bool active, QString* errOut=nullptr);
    bool setUserRole(const QString& actorUsername, const QString& targetUsername, Role newRole, QString* errOut=nullptr);
    bool createUser(const QString& actorUsername, const QString& username, const QString& rawPassword, Role initialRole, QString* errOut=nullptr);
    bool resetUserPassword(const QString& actorUsername, const QString& targetUsername, const QString& newRawPassword, QString* errOut=nullptr);

    bool verifyUser(const QString& username, const QString& password, QString& roleOut);
    AuthResult authenticate(const QString& username, const QString& password);
    Role getUserRole(const QString& username);

    // KPI/쿼리
    bool kpiForType(const QString& type, const QDateTime& from, const QDateTime& to, KPIStats* out);
    bool kpiAll(const QDateTime& from, const QDateTime& to, QMap<QString,KPIStats>* out);
    QVector<QPointF> trendSeries(const QString& type, const QDateTime& from, const QDateTime& to, int bucketSec);
    QList<SensorData> topAnomalies(const QDateTime& from, const QDateTime& to, int limit);

    QVector<TypeThreshold> listTypeThresholds();
    QVector<SensorThreshold> listSensorThresholds(const QString& typeFilter, const QString& idContains);
    bool saveTypeThresholds(const QString& actor, const QVector<TypeThreshold>& rows, QString* errOut=nullptr);
    bool saveSensorThresholds(const QString& actor, const QVector<SensorThreshold>& rows, QString* errOut=nullptr);
    bool getTypeThreshold(const QString& type, TypeThreshold* out);
    bool getSensorOverride(const QString& sensorId, SensorThreshold* out);

    QStringList listKnownSensors();
    QStringList listKnownSensors(const QString& typeFilter, const QString& idContains);
    QVector<double> sampleValues(const QString& type, const QString& sensorId, const QDateTime& from, const QDateTime& to, int maxN);

    bool audit(const QString& actor, const QString& action, const QString& target=QString(), const QString& details=QString());
    bool insertThresholdTuning(const QString& actor, const QString& scope, const QString& name, const QString& method, double warn, double high, int sample, const QString& details=QString());

    struct AuditLog { int id{}; QString actor, action, target, details; qint64 ts{}; };
    QList<AuditLog> auditLogs(const QDateTime& from, const QDateTime& to, const QString& actorFilter, const QString& actionFilter, bool onlyOwn, const QString& currentUser, int limit);

    // === NEW: co-occurrence 결과 페어 ===
    struct CoPair {
        QString a;
        QString b;
        int cnt = 0;
    };

    // === NEW: 센서 헬스 진단 행 ===
    struct SensorHealthRow {
        QString sensorId;
        QString type;
        double flatRatio = 0.0;   // unique(values)/N
        double driftSlope = 0.0;  // 회귀 기울기 (value vs time)
        double spikeScore = 0.0;  // 표준편차
    };

    // === NEW: AIDialogs가 호출하는 API들 ===

    // warn/high 임계치로 LOW/MED/HIGH 카운트 시뮬레이션
    bool simulateCounts(const QString& type,
                        const QDateTime& from, const QDateTime& to,
                        double warn, double high, KPIStats* out);

    // 동일 시간버킷(분) 내 HIGH 동시발생 상위 페어
    QVector<CoPair> rcaCooccurrence(const QDateTime& from, const QDateTime& to,
                                    const QString& typeFilter, int bucketMin, int topN);

    // 센서 헬스 지표(flat/drift/spike)
    QVector<SensorHealthRow> sensorHealth(const QDateTime& from, const QDateTime& to,
                                          const QString& typeFilter, int maxRows);


private:
    bool execWithRetry(QSqlQuery& q, QSqlDatabase& db, int maxRetry = 3) const;
        QString roleToString(Role r) const;
    static bool isTransient(const QSqlError& e);

    // 사용자 로우/업그레이드
    bool fetchUserRow(const QString& username,
                      int* id, int* role, bool* active,
                      QByteArray* pwHashNew, QByteArray* pwSalt,
                      QString* pwAlgo, int* pwIter,
                      QString* oldHexSha = nullptr);

    bool upgradePasswordToNewScheme(int userId, const QString& newRawPassword, QString* errOut=nullptr);



};


#endif // DBBRIDGE_H
