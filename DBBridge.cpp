#include "DBBridge.h"
#include "ConnectionPool.h"
#include "CryptoUtils.h"

#include <QThread>
#include <algorithm>
#include <cmath>
#include <QSet>
#include <QStringList>

static inline qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

DBBridge::DBBridge(QObject *parent) : QObject(parent) {}
DBBridge::~DBBridge() {}

bool DBBridge::connectDB(const QString& host, int port, const QString& db, const QString& user, const QString& password, bool useSSL)
{
    ConnectionPool::Config cfg;
    cfg.host = host; cfg.port = port; cfg.db = db; cfg.user = user; cfg.pw = password; cfg.useSSL = useSSL;
    ConnectionPool::instance().configure(cfg);

    ConnectionPool::Scoped s;
    auto dbc = s.db();
    return dbc.isValid() && dbc.isOpen();
}

bool DBBridge::connectWithCandidates(const QList<QPair<QString,int>>& hosts, const QString& dbName, const QString& user, const QString& pw, bool useSSL, QString* err)
{
    for (const auto& hp : hosts) {
        ConnectionPool::Config cfg;
        cfg.host = hp.first; cfg.port = hp.second; cfg.db = dbName; cfg.user = user; cfg.pw = pw; cfg.useSSL = useSSL;
        ConnectionPool::instance().configure(cfg);
        ConnectionPool::Scoped s;
        auto dbc = s.db();
        if (dbc.isValid() && dbc.isOpen()) return true;
        if (err) *err = dbc.lastError().text();
    }
    if (err && err->isEmpty()) *err = "No reachable DB host.";
    return false;
}

// ── 재시도 ───────────────────────────────────────────────────────────────────
bool DBBridge::isTransient(const QSqlError& e)
{
    const QString tx = e.text().toLower();
    if (tx.contains("server has gone away") ||
        tx.contains("lost connection") ||
        tx.contains("lock wait timeout") ||
        tx.contains("deadlock") ||
        tx.contains("timeout"))
        return true;

    const QString code = e.nativeErrorCode(); // 2006, 2013, 1205, 1213 등
    static const QSet<QString> transientCodes = {"2006","2013","1205","1213","1040","1159","1160","1161"};
    return transientCodes.contains(code);
}

bool DBBridge::execWithRetry(QSqlQuery& q, QSqlDatabase& db, int maxRetry) const
{
    for (int a = 0; a <= maxRetry; ++a) {
        if (q.exec()) return true;

        const QSqlError e = q.lastError();
        if (a == maxRetry || !isTransient(e)) return false;

        // 지수 백오프 후 재연결 시도
        QThread::msleep(150u * (1u << a));
        if (db.isOpen()) db.close();
        if (!db.open())  return false;

        // 주의: DB 재연결 시 prepare가 무효화될 수 있음.
        // 이 함수는 호출자가 매 시도마다 q.prepare(...)를 다시 호출해주는 패턴으로 쓰거나,
        // 실패 시 바깥에서 q를 새로 만들고 재-prepare 해주는 방식이 가장 안전합니다.
        // (현재 코드는 대부분 시도 직전 prepare 하므로 OK)
    }
    return false;
}
// ── Sensor Data ──────────────────────────────────────────────────────────────
void DBBridge::insertSensorData(const SensorData& d)
{
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.prepare("INSERT INTO sensor_data(sensor_id, type, value, level, timestamp, latitude, longitude) VALUES (?,?,?,?,?,?,?)");
    q.addBindValue(d.sensorId); q.addBindValue(d.type); q.addBindValue(d.value); q.addBindValue(d.level);
    q.addBindValue(d.timestamp); q.addBindValue(d.latitude); q.addBindValue(d.longitude);
    if (!execWithRetry(q, db)) qWarning() << "[DB] Insert 실패:" << q.lastError().text();
}

QList<SensorData> DBBridge::fetchRecentData(int limit)
{
    QList<SensorData> list; ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return list;
    QSqlQuery q(db);
    q.prepare("SELECT sensor_id, type, value, level, timestamp, latitude, longitude "
                        "FROM sensor_data ORDER BY id DESC LIMIT ?");
    q.addBindValue(limit);
    if (!execWithRetry(q, db)) return list;
    while (q.next()) {
        SensorData d;
        d.sensorId  = q.value(0).toString();
        d.type      = q.value(1).toString();
        d.value     = q.value(2).toDouble();
        d.level     = q.value(3).toString();
        d.timestamp = q.value(4).toLongLong();
        d.latitude  = q.value(5).toDouble();
        d.longitude = q.value(6).toDouble();
        list.append(d);
    }
    return list;
}

// ── Users/Roles ──────────────────────────────────────────────────────────────
QList<UserInfo> DBBridge::listUsersEx(QString* errOut)
{
    QList<UserInfo> out; ConnectionPool::Scoped s; auto db = s.db();
    if (!db.isOpen()) { if (errOut) *errOut = "DB 연결 안됨"; return out; }
    QSqlQuery q(db);
    if (!q.exec("SELECT username, role, active FROM users ORDER BY username")) { if (errOut) *errOut = q.lastError().text(); return out; }
    while (q.next()) { UserInfo u; u.username=q.value(0).toString(); u.role=roleFromInt(q.value(1).toInt()); u.active=q.value(2).toInt()==1; out.append(u); }
    return out;
}

bool DBBridge::setUserActive(const QString& actorUsername, const QString& targetUsername, bool active, QString* errOut)
{
    const Role actor = getUserRole(actorUsername);
    if (actor != Role::SuperAdmin) { if (errOut) *errOut = "권한 없음: SuperAdmin만 활성/비활성 변경 가능"; return false; }
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) { if (errOut) *errOut = "DB 연결 안됨"; return false; }

    QSqlQuery q(db); q.prepare("UPDATE users SET active=? WHERE username=?");
    q.addBindValue(active ? 1 : 0); q.addBindValue(targetUsername);
    if (!execWithRetry(q, db)) { if (errOut) *errOut = q.lastError().text(); return false; }
    const bool ok = q.numRowsAffected() > 0;
    if (ok) audit(actorUsername, active ? "activateUser" : "deactivateUser", targetUsername);
    return ok;
}

bool DBBridge::setUserRole(const QString& actorUsername, const QString& targetUsername, Role newRole, QString* errOut)
{
    const Role actor = getUserRole(actorUsername);
    if (actor != Role::SuperAdmin) { if (errOut) *errOut = "권한 없음: SuperAdmin만 역할 변경 가능"; return false; }
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) { if (errOut) *errOut = "DB 연결 안됨"; return false; }

    QSqlQuery q(db); q.prepare("UPDATE users SET role=? WHERE username=?");
    q.addBindValue(static_cast<int>(newRole)); q.addBindValue(targetUsername);
    if (!execWithRetry(q, db)) { if (errOut) *errOut = q.lastError().text(); return false; }
    const bool ok = q.numRowsAffected() > 0;
    if (ok) audit(actorUsername, "setUserRole", targetUsername, QString("newRole=%1").arg(static_cast<int>(newRole)));
    return ok;
}

bool DBBridge::createUser(const QString& actorUsername, const QString& username, const QString& rawPassword, Role initialRole, QString* errOut)
{
    const Role actor = getUserRole(actorUsername);
    if (actor < Role::Admin) { if (errOut) *errOut = "권한 없음: Admin 이상만 사용자 생성 가능"; return false; }
    if (actor == Role::Admin && initialRole == Role::SuperAdmin) { if (errOut) *errOut = "Admin은 SuperAdmin 생성 불가"; return false; }
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) { if (errOut) *errOut = "DB 연결 안됨"; return false; }

    const bool preferArgon2 = true;
    StoredPw pw = CryptoUtils::hashPassword(rawPassword, preferArgon2);

    QSqlQuery q(db);
    if (pw.algo == PwAlgo::PBKDF2) {
        q.prepare("INSERT INTO users(username, pw_algo, pw_hash, pw_salt, pw_iter, role, active, pw_version) "
                  "VALUES(?, 'PBKDF2', ?, ?, ?, ?, 1, 2)");
        q.addBindValue(username);
        q.addBindValue(pw.hash);
        q.addBindValue(pw.salt);
        q.addBindValue(pw.iterations);
        q.addBindValue(static_cast<int>(initialRole));
    } else {
        q.prepare("INSERT INTO users(username, pw_algo, pw_hash, role, active, pw_version) "
                  "VALUES(?, 'ARGON2', ?, ?, 1, 2)");
        q.addBindValue(username);
        q.addBindValue(pw.hash); // encoded
        q.addBindValue(static_cast<int>(initialRole));
    }
    if (!execWithRetry(q, db)) { if (errOut) *errOut = q.lastError().text(); return false; }
    audit(actorUsername, "createUser", username, QString("role=%1").arg(static_cast<int>(initialRole)));
    return true;
}

bool DBBridge::resetUserPassword(const QString& actorUsername, const QString& targetUsername, const QString& newRawPassword, QString* errOut)
{
    const Role actor = getUserRole(actorUsername);
    if (actor != Role::SuperAdmin) { if (errOut) *errOut = "권한 없음: SuperAdmin만 암호 초기화"; return false; }
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) { if (errOut) *errOut = "DB 연결 안됨"; return false; }

    const bool preferArgon2 = true;
    StoredPw pw = CryptoUtils::hashPassword(newRawPassword, preferArgon2);

    QSqlQuery q(db);
    if (pw.algo == PwAlgo::PBKDF2) {
        q.prepare("UPDATE users SET pw_algo='PBKDF2', pw_hash=?, pw_salt=?, pw_iter=?, pw_version=2, password_hash=NULL WHERE username=?");
        q.addBindValue(pw.hash); q.addBindValue(pw.salt); q.addBindValue(pw.iterations); q.addBindValue(targetUsername);
    } else {
        q.prepare("UPDATE users SET pw_algo='ARGON2', pw_hash=?, pw_salt=NULL, pw_iter=NULL, pw_version=2, password_hash=NULL WHERE username=?");
        q.addBindValue(pw.hash); q.addBindValue(targetUsername);
    }
    if (!execWithRetry(q, db)) { if (errOut) *errOut = q.lastError().text(); return false; }
    const bool ok = q.numRowsAffected() > 0;
    if (ok) audit(actorUsername, "resetUserPassword", targetUsername);
    return ok;
}

// ── 인증 ─────────────────────────────────────────────────────────────────────
bool DBBridge::fetchUserRow(const QString& username,
                            int* id, int* role, bool* active,
                            QByteArray* pwHashNew, QByteArray* pwSalt,
                            QString* pwAlgo, int* pwIter,
                            QString* oldHexSha)
{
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare("SELECT id, role, active, pw_algo, pw_hash, pw_salt, pw_iter, password_hash "
              "FROM users WHERE username=? LIMIT 1");
    q.addBindValue(username);
    if (!execWithRetry(q, db) || !q.next()) return false;

    if (id) *id = q.value(0).toInt();
    if (role) *role = q.value(1).toInt();
    if (active) *active = (q.value(2).toInt()==1);
    if (pwAlgo) *pwAlgo = q.value(3).toString();
    if (pwHashNew) *pwHashNew = q.value(4).toByteArray();
    if (pwSalt) *pwSalt = q.value(5).toByteArray();
    if (pwIter) *pwIter = q.value(6).isNull() ? 0 : q.value(6).toInt();
    if (oldHexSha) *oldHexSha = q.value(7).toString();
    return true;
}

bool DBBridge::upgradePasswordToNewScheme(int userId, const QString& newRawPassword, QString* errOut)
{
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) { if (errOut) *errOut="DB 연결 안됨"; return false; }
    StoredPw pw = CryptoUtils::hashPassword(newRawPassword, /*preferArgon2*/true);

    QSqlQuery q(db);
    if (pw.algo == PwAlgo::PBKDF2) {
        q.prepare("UPDATE users SET pw_algo='PBKDF2', pw_hash=?, pw_salt=?, pw_iter=?, pw_version=2, password_hash=NULL WHERE id=?");
        q.addBindValue(pw.hash); q.addBindValue(pw.salt); q.addBindValue(pw.iterations); q.addBindValue(userId);
    } else {
        q.prepare("UPDATE users SET pw_algo='ARGON2', pw_hash=?, pw_salt=NULL, pw_iter=NULL, pw_version=2, password_hash=NULL WHERE id=?");
        q.addBindValue(pw.hash); q.addBindValue(userId);
    }
    if (!execWithRetry(q, db)) { if (errOut) *errOut = q.lastError().text(); return false; }
    return q.numRowsAffected() > 0;
}

bool DBBridge::verifyUser(const QString& username, const QString& password, QString& roleOut)
{
    const AuthResult r = authenticate(username, password);
    if (r.ok) { roleOut = roleToString(r.role); return true; }
    return false;
}

AuthResult DBBridge::authenticate(const QString& username, const QString& password)
{
    AuthResult r;
    int uid=0, rint=0, iters=0; bool act=false;
    QByteArray pwHashNew, pwSalt; QString algo, oldHex;
    if (!fetchUserRow(username, &uid, &rint, &act, &pwHashNew, &pwSalt, &algo, &iters, &oldHex)) {
        r.error = "아이디/비밀번호 불일치"; return r;
    }
    if (!act) { r.error = "비활성화된 계정입니다."; return r; }

    bool ok=false;
    if (!algo.isEmpty() && !pwHashNew.isEmpty()) {
        StoredPw st;
        if (algo.compare("PBKDF2", Qt::CaseInsensitive)==0) {
            st.algo = PwAlgo::PBKDF2; st.hash=pwHashNew; st.salt=pwSalt; st.iterations=iters>0?iters:120000;
        } else {
            st.algo = PwAlgo::ARGON2ID; st.hash=pwHashNew;
        }
        ok = CryptoUtils::verifyPassword(password, st);
    } else if (!oldHex.isEmpty()) {
        // 구 스킴(SHA256 hex) → 성공 시 즉시 업그레이드
        const QByteArray local = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex();
        ok = (QString::fromLatin1(local).toLower() == oldHex.toLower());
        if (ok) { QString err; upgradePasswordToNewScheme(uid, password, &err); }
    }

    if (!ok) { r.error = "아이디/비밀번호 불일치"; return r; }
    r.ok = true; r.userId = uid; r.username = username; r.role = roleFromInt(rint);
    return r;
}

Role DBBridge::getUserRole(const QString& username)
{
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return Role::Viewer;
    QSqlQuery q(db); q.prepare("SELECT role FROM users WHERE username=? AND active=1");
    q.addBindValue(username);
    if (!execWithRetry(q, db) || !q.next()) return Role::Viewer;
    return roleFromInt(q.value(0).toInt());
}

QString DBBridge::roleToString(Role r) const {
    switch (r) {
    case Role::Viewer: return "Viewer";
    case Role::Operator: return "Operator";
    case Role::Analyst: return "Analyst";
    case Role::Admin: return "Admin";
    case Role::SuperAdmin: return "SuperAdmin";
    }
    return "Viewer";
}

// ── KPI/기타 쿼리 ────────────────────────────────────────────────────────────
bool DBBridge::kpiForType(const QString& type, const QDateTime& from, const QDateTime& to, KPIStats* out)
{
    if (!out) return false;
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*), AVG(value), MIN(value), MAX(value), "
              "SUM(level='LOW'), SUM(level='MEDIUM'), SUM(level='HIGH') "
              "FROM sensor_data WHERE type=? AND timestamp BETWEEN ? AND ?");
    q.addBindValue(type);
    q.addBindValue(from.toMSecsSinceEpoch());
    q.addBindValue(to.toMSecsSinceEpoch());
    if (!execWithRetry(q, db) || !q.next()) return false;
    out->count=q.value(0).toInt(); out->avg=q.value(1).toDouble(); out->mn=q.value(2).toDouble(); out->mx=q.value(3).toDouble();
    out->low=q.value(4).toInt(); out->med=q.value(5).toInt(); out->high=q.value(6).toInt();
    return true;
}

bool DBBridge::kpiAll(const QDateTime& from, const QDateTime& to, QMap<QString,KPIStats>* out)
{
    if (!out) return false; out->clear(); KPIStats st;
    if (kpiForType("TEMP", from, to, &st)) (*out)["TEMP"] = st;
    if (kpiForType("VIB",  from, to, &st)) (*out)["VIB"]  = st;
    if (kpiForType("INTR", from, to, &st)) (*out)["INTR"] = st;
    return !out->isEmpty();
}

QVector<QPointF> DBBridge::trendSeries(const QString& type, const QDateTime& from, const QDateTime& to, int bucketSec)
{
    QVector<QPointF> pts; ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return pts;
    const qint64 bucketMs = qint64(bucketSec) * 1000;
    QSqlQuery q(db);
    q.prepare("SELECT (timestamp DIV ?) * ? AS bucket, AVG(value) "
              "FROM sensor_data WHERE type=? AND timestamp BETWEEN ? AND ? "
              "GROUP BY bucket ORDER BY bucket");
    q.addBindValue(bucketMs); q.addBindValue(bucketMs); q.addBindValue(type);
    q.addBindValue(from.toMSecsSinceEpoch()); q.addBindValue(to.toMSecsSinceEpoch());
    if (!execWithRetry(q, db)) return pts;
    while (q.next()) pts.append(QPointF(double(q.value(0).toLongLong()), q.value(1).toDouble()));
    return pts;
}

QList<SensorData> DBBridge::topAnomalies(const QDateTime& from, const QDateTime& to, int limit)
{
    QList<SensorData> out; ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return out;
    QSqlQuery q(db);
    q.prepare(QString("SELECT sensor_id, type, value, level, timestamp, latitude, longitude "
                      "FROM sensor_data WHERE level='HIGH' AND timestamp BETWEEN ? AND ? "
                      "ORDER BY value DESC LIMIT %1").arg(limit));
    q.addBindValue(from.toMSecsSinceEpoch()); q.addBindValue(to.toMSecsSinceEpoch());
    if (!execWithRetry(q, db)) return out;
    while (q.next()) {
        SensorData d; d.sensorId=q.value(0).toString(); d.type=q.value(1).toString(); d.value=q.value(2).toDouble();
        d.level=q.value(3).toString(); d.timestamp=q.value(4).toLongLong();
        d.latitude=q.value(5).toDouble(); d.longitude=q.value(6).toDouble(); out.append(d);
    }
    return out;
}

QVector<TypeThreshold> DBBridge::listTypeThresholds() {
    QVector<TypeThreshold> out; ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return out;
    QSqlQuery q(db);
    if (!q.exec("SELECT type,warn,high,unit FROM thresholds_type ORDER BY FIELD(type,'TEMP','VIB','INTR')")) return out;
    while (q.next()){ TypeThreshold t; t.type=q.value(0).toString(); t.warn=q.value(1).toDouble(); t.high=q.value(2).toDouble(); t.unit=q.value(3).toString(); out.push_back(t); }
    return out;
}

QVector<SensorThreshold> DBBridge::listSensorThresholds(const QString& typeFilter, const QString& idContains) {
    QVector<SensorThreshold> out; ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return out;
    QString sql = "SELECT sensor_id,type,warn,high,unit FROM thresholds_sensor";
    QString where;
    if (!typeFilter.isEmpty()) where += (where.isEmpty()? " WHERE " : " AND ") + QString("type=?");
    if (!idContains.isEmpty()) where += (where.isEmpty()? " WHERE " : " AND ") + QString("sensor_id LIKE ?");
    sql += where + " ORDER BY sensor_id";
    QSqlQuery q(db); q.prepare(sql);
    int b=0;
    if (!typeFilter.isEmpty()) q.bindValue(b++, typeFilter);
    if (!idContains.isEmpty()) q.bindValue(b++, "%" + idContains + "%");
    if (!execWithRetry(q, db)) return out;
    while (q.next()){
        SensorThreshold t; t.sensorId=q.value(0).toString(); t.type=q.value(1).toString(); t.warn=q.value(2).toDouble();
        t.high=q.value(3).toDouble(); t.unit=q.value(4).toString(); out.push_back(t);
    }
    return out;
}

bool DBBridge::saveTypeThresholds(const QString& actor, const QVector<TypeThreshold>& rows, QString* errOut) {
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) { if(errOut)*errOut="DB not open"; return false; }
    if (!db.transaction()) { if(errOut)*errOut="BEGIN failed"; return false; }
    QSqlQuery q(db);
    q.prepare(R"(INSERT INTO thresholds_type(type,warn,high,unit,updated_by)
                 VALUES(?,?,?,?,?)
                 ON DUPLICATE KEY UPDATE warn=VALUES(warn), high=VALUES(high),
                 unit=VALUES(unit), updated_by=VALUES(updated_by))");
    for (const auto& r: rows){
        q.bindValue(0,r.type); q.bindValue(1,r.warn); q.bindValue(2,r.high);
        q.bindValue(3,r.unit); q.bindValue(4,actor);
        if (!execWithRetry(q, db)) { if(errOut)*errOut=q.lastError().text(); db.rollback(); return false; }
    }
    if (!db.commit()) { if(errOut)*errOut="COMMIT failed"; return false; }
    audit(actor, "saveTypeThresholds", "", QString("count=%1").arg(rows.size()));
    return true;
}

bool DBBridge::saveSensorThresholds(const QString& actor, const QVector<SensorThreshold>& rows, QString* errOut) {
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) { if(errOut)*errOut="DB not open"; return false; }
    if (!db.transaction()) { if(errOut)*errOut="BEGIN failed"; return false; }
    QSqlQuery q(db);
    q.prepare(R"(INSERT INTO thresholds_sensor(sensor_id,type,warn,high,unit,updated_by)
                 VALUES(?,?,?,?,?,?)
                 ON DUPLICATE KEY UPDATE type=VALUES(type), warn=VALUES(warn), high=VALUES(high),
                 unit=VALUES(unit), updated_by=VALUES(updated_by))");
    for (const auto& r: rows){
        q.bindValue(0,r.sensorId); q.bindValue(1,r.type); q.bindValue(2,r.warn);
        q.bindValue(3,r.high);     q.bindValue(4,r.unit); q.bindValue(5,actor);
        if (!execWithRetry(q, db)) { if(errOut)*errOut=q.lastError().text(); db.rollback(); return false; }
    }
    if (!db.commit()) { if(errOut)*errOut="COMMIT failed"; return false; }
    audit(actor, "saveSensorThresholds", "", QString("count=%1").arg(rows.size()));
    return true;
}

bool DBBridge::getTypeThreshold(const QString& type, TypeThreshold* out) {
    if (!out) return false; ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return false;
    QSqlQuery q(db); q.prepare("SELECT warn,high,unit FROM thresholds_type WHERE type=?");
    q.addBindValue(type);
    if (!execWithRetry(q, db) || !q.next()) return false;
    out->type=type; out->warn=q.value(0).toDouble(); out->high=q.value(1).toDouble(); out->unit=q.value(2).toString();
    return true;
}

bool DBBridge::getSensorOverride(const QString& sensorId, SensorThreshold* out) {
    if (!out) return false; ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return false;
    QSqlQuery q(db); q.prepare("SELECT type,warn,high,unit FROM thresholds_sensor WHERE sensor_id=?");
    q.addBindValue(sensorId);
    if (!execWithRetry(q, db) || !q.next()) return false;
    out->sensorId=sensorId; out->type=q.value(0).toString();
    out->warn=q.value(1).toDouble(); out->high=q.value(2).toDouble(); out->unit=q.value(3).toString();
    return true;
}

QStringList DBBridge::listKnownSensors()
{
    QStringList out; ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return out;
    QSqlQuery q(db);
    if (!q.exec("SELECT DISTINCT sensor_id FROM sensor_data ORDER BY sensor_id ASC")) return out;
    while (q.next()) out << q.value(0).toString();
    return out;
}

QStringList DBBridge::listKnownSensors(const QString& typeFilter, const QString& idContains) {
    QStringList out; ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return out;
    QString sql="SELECT DISTINCT sensor_id FROM sensor_data";
    QString where;
    if (!typeFilter.isEmpty()) where += (where.isEmpty()? " WHERE " : " AND ") + QString("type=?");
    if (!idContains.isEmpty()) where += (where.isEmpty()? " WHERE " : " AND ") + QString("sensor_id LIKE ?");
    sql += where + " ORDER BY sensor_id";
    QSqlQuery q(db); q.prepare(sql);
    int b=0;
    if (!typeFilter.isEmpty()) q.bindValue(b++, typeFilter);
    if (!idContains.isEmpty()) q.bindValue(b++, "%"+idContains+"%");
    if (!execWithRetry(q, db)) return out;
    while (q.next()) out << q.value(0).toString();
    return out;
}

QVector<double> DBBridge::sampleValues(const QString& type, const QString& sensorId, const QDateTime& from, const QDateTime& to, int maxN)
{
    QVector<double> out; ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return out;
    QString sql = "SELECT value FROM sensor_data WHERE timestamp BETWEEN ? AND ?";
    if (!type.isEmpty())     sql += " AND type=?";
    if (!sensorId.isEmpty()) sql += " AND sensor_id=?";
    sql += " ORDER BY id DESC LIMIT ?";

    QSqlQuery q(db); q.prepare(sql);
    int b = 0;
    q.bindValue(b++, from.toMSecsSinceEpoch());
    q.bindValue(b++, to.toMSecsSinceEpoch());
    if (!type.isEmpty())     q.bindValue(b++, type);
    if (!sensorId.isEmpty()) q.bindValue(b++, sensorId);
    q.bindValue(b++, maxN);

    if (!execWithRetry(q, db)) return out;
    while (q.next()) out.append(q.value(0).toDouble());
    return out;
}

bool DBBridge::audit(const QString& actor, const QString& action, const QString& target, const QString& details)
{
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare("INSERT INTO audit_log(actor,action,target,details,ts) VALUES(?,?,?,?,?)");
    q.addBindValue(actor); q.addBindValue(action); q.addBindValue(target); q.addBindValue(details); q.addBindValue(nowMs());
    return execWithRetry(q, db);
}

bool DBBridge::insertThresholdTuning(const QString& actor, const QString& scope, const QString& name, const QString& method, double warn, double high, int sample, const QString& details)
{
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare(R"(INSERT INTO threshold_tuning
                 (actor, scope, name, method, warn, high, sample, details, ts)
                 VALUES(?,?,?,?,?,?,?,?,?))");
    q.addBindValue(actor); q.addBindValue(scope); q.addBindValue(name); q.addBindValue(method);
    q.addBindValue(warn);  q.addBindValue(high);  q.addBindValue(sample); q.addBindValue(details);
    q.addBindValue(nowMs());
    return execWithRetry(q, db);
}

QList<DBBridge::AuditLog> DBBridge::auditLogs(const QDateTime& from, const QDateTime& to, const QString& actorFilter, const QString& actionFilter, bool onlyOwn, const QString& currentUser, int limit)
{
    QList<AuditLog> out; ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return out;

    QString sql = "SELECT id,actor,action,target,details,ts FROM audit_log WHERE ts BETWEEN ? AND ?";
    QStringList where;
    if (onlyOwn) where << "actor = ?";
    else if (!actorFilter.trimmed().isEmpty()) where << "actor LIKE ?";
    if (!actionFilter.trimmed().isEmpty()) where << "action LIKE ?";

    if (!where.isEmpty()) sql += " AND " + where.join(" AND ");
    sql += " ORDER BY ts DESC LIMIT ?";

    QSqlQuery q(db); q.prepare(sql);
    int b=0;
    q.bindValue(b++, from.toMSecsSinceEpoch()); q.bindValue(b++, to.toMSecsSinceEpoch());
    if (onlyOwn) q.bindValue(b++, currentUser);
    else if (!actorFilter.trimmed().isEmpty()) q.bindValue(b++, "%" + actorFilter.trimmed() + "%");
    if (!actionFilter.trimmed().isEmpty()) q.bindValue(b++, "%" + actionFilter.trimmed() + "%");
    q.bindValue(b++, limit);

    if (!execWithRetry(q, db)) { qWarning() << "[DB] auditLogs failed:" << q.lastError().text(); return out; }
    while (q.next()) { AuditLog a; a.id=q.value(0).toInt(); a.actor=q.value(1).toString(); a.action=q.value(2).toString();
        a.target=q.value(3).toString(); a.details=q.value(4).toString(); a.ts=q.value(5).toLongLong(); out.append(a); }
    return out;
}


bool DBBridge::simulateCounts(const QString& type,
                              const QDateTime& from, const QDateTime& to,
                              double warn, double high, KPIStats* out)
{
    if (!out) return false;
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return false;

    QSqlQuery q(db);
    q.prepare(R"(SELECT
                    COUNT(*),
                    AVG(value),
                    MIN(value),
                    MAX(value),
                    SUM(value < ?),
                    SUM(value >= ? AND value < ?),
                    SUM(value >= ?)
                 FROM sensor_data
                 WHERE type = ? AND timestamp BETWEEN ? AND ?)");
    int b=0;
    q.bindValue(b++, warn);
    q.bindValue(b++, warn);
    q.bindValue(b++, high);
    q.bindValue(b++, high);
    q.bindValue(b++, type);
    q.bindValue(b++, from.toMSecsSinceEpoch());
    q.bindValue(b++, to.toMSecsSinceEpoch());

    if (!execWithRetry(q, db) || !q.next()) return false;

    out->count = q.value(0).toInt();
    out->avg   = q.value(1).toDouble();
    out->mn    = q.value(2).toDouble();
    out->mx    = q.value(3).toDouble();
    out->low   = q.value(4).toInt();
    out->med   = q.value(5).toInt();
    out->high  = q.value(6).toInt();
    return true;
}


QVector<DBBridge::CoPair>
DBBridge::rcaCooccurrence(const QDateTime& from, const QDateTime& to,
                          const QString& typeFilter, int bucketMin, int topN)
{
    QVector<CoPair> out;
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return out;

    const qint64 bucketMs = qint64(bucketMin) * 60 * 1000;

    QString sql =
        "SELECT DISTINCT (timestamp DIV ?) * ? AS bucket, type "
        "FROM sensor_data "
        "WHERE level='HIGH' AND timestamp BETWEEN ? AND ? ";
    if (!typeFilter.trimmed().isEmpty())
        sql += "AND type=? ";
    sql += "ORDER BY bucket";

    QSqlQuery q(db); q.prepare(sql);
    int b=0;
    q.bindValue(b++, bucketMs);
    q.bindValue(b++, bucketMs);
    q.bindValue(b++, from.toMSecsSinceEpoch());
    q.bindValue(b++, to.toMSecsSinceEpoch());
    if (!typeFilter.trimmed().isEmpty())
        q.bindValue(b++, typeFilter);

    if (!execWithRetry(q, db)) return out;

    // 버킷별 HIGH 타입 집합
    QMap<qint64, QStringList> byBucket;
    while (q.next()) {
        const qint64 buck = q.value(0).toLongLong();
        const QString typ = q.value(1).toString();
        byBucket[buck].append(typ);
    }

    // 페어 카운트
    QMap<QPair<QString,QString>, int> counts;
    for (auto it = byBucket.begin(); it != byBucket.end(); ++it) {
        auto types = it.value();
        types.removeDuplicates();
        std::sort(types.begin(), types.end());
        for (int i=0;i<types.size();++i)
            for (int j=i+1;j<types.size();++j)
                counts[qMakePair(types[i], types[j])] += 1;
    }

    QVector<CoPair> tmp; tmp.reserve(counts.size());
    for (auto it = counts.begin(); it != counts.end(); ++it) {
        CoPair p; p.a = it.key().first; p.b = it.key().second; p.cnt = it.value();
        tmp.append(p);
    }
    std::sort(tmp.begin(), tmp.end(), [](const CoPair& x, const CoPair& y){ return x.cnt > y.cnt; });
    if (topN > 0 && tmp.size() > topN) tmp.resize(topN);
    return tmp;
}


QVector<DBBridge::SensorHealthRow>
DBBridge::sensorHealth(const QDateTime& from, const QDateTime& to,
                       const QString& typeFilter, int maxRows)
{
    QVector<SensorHealthRow> out;
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return out;

    QString sql =
        "SELECT sensor_id, type, timestamp, value "
        "FROM sensor_data "
        "WHERE timestamp BETWEEN ? AND ? ";
    if (!typeFilter.trimmed().isEmpty())
        sql += "AND type=? ";
    sql += "ORDER BY sensor_id, type, timestamp";

    QSqlQuery q(db); q.prepare(sql);
    int b=0;
    q.bindValue(b++, from.toMSecsSinceEpoch());
    q.bindValue(b++, to.toMSecsSinceEpoch());
    if (!typeFilter.trimmed().isEmpty())
        q.bindValue(b++, typeFilter);

    if (!execWithRetry(q, db)) return out;

    struct Acc {
        int n=0; double sum=0, sum2=0;
        qint64 t0=0; double sumX=0, sumX2=0, sumXY=0;
        QSet<qint64> uniq; // value*1000 양자화로 유일값 근사
    };
    QMap<QPair<QString,QString>, Acc> acc;

    while (q.next()) {
        const QString sid = q.value(0).toString();
        const QString typ = q.value(1).toString();
        const qint64 ts   = q.value(2).toLongLong();
        const double v    = q.value(3).toDouble();

        auto key = qMakePair(sid, typ);
        auto &a = acc[key];
        if (a.n == 0) a.t0 = ts;
        const double x = double(ts - a.t0) / 1000.0; // seconds from first

        a.n += 1;
        a.sum  += v;
        a.sum2 += v*v;
        a.sumX += x;
        a.sumX2 += x*x;
        a.sumXY += x*v;

        const qint64 quant = qint64(std::llround(v * 1000.0));
        a.uniq.insert(quant);
    }

    QVector<SensorHealthRow> rows; rows.reserve(acc.size());
    for (auto it = acc.begin(); it != acc.end(); ++it) {
        const auto& k = it.key();
        const auto& a = it.value();
        SensorHealthRow r; r.sensorId = k.first; r.type = k.second;

        if (a.n > 0) {
            r.flatRatio = double(a.uniq.size()) / double(a.n);

            const double denom = (a.n * a.sumX2 - a.sumX * a.sumX);
            r.driftSlope = (denom != 0.0)
                               ? ((a.n * a.sumXY - a.sumX * a.sum) / denom)
                               : 0.0;

            const double variance = (a.n > 1)
                                        ? std::max(0.0, (a.sum2 - (a.sum*a.sum)/a.n) / (a.n - 1))
                                        : 0.0;
            r.spikeScore = std::sqrt(variance);
        }
        rows.append(r);
    }

    std::sort(rows.begin(), rows.end(),
              [](const SensorHealthRow& A, const SensorHealthRow& B){
                  return A.spikeScore > B.spikeScore;
              });
    if (maxRows > 0 && rows.size() > maxRows) rows.resize(maxRows);
    return rows;
}
