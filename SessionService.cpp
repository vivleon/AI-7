#include "SessionService.h"
#include "ConnectionPool.h"

#include <QtCore>
#include <QtSql>
#include <QUuid>

static inline qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

static QString makeToken() {
    QString t = QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.remove('-');
    return t;
}

SessionService::SessionService(QObject* parent) : QObject(parent) {}

bool SessionService::hasActiveSession(const QString& username,
                                      int staleMs,
                                      qint64* outId,
                                      QString* outClientId)
{
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return false;

    QSqlQuery q(db);
    q.prepare("SELECT id, client_id FROM sessions "
              "WHERE username=? AND active=1 AND last_seen >= ? "
              "ORDER BY last_seen DESC LIMIT 1");
    q.addBindValue(username);
    q.addBindValue(nowMs() - qint64(staleMs));
    if (!q.exec()) return false;
    if (!q.next()) return false;

    if (outId) *outId = q.value(0).toLongLong();
    if (outClientId) *outClientId = q.value(1).toString();
    return true;
}

bool SessionService::beginSession(const QString& username, const QString& clientId,
                                  qint64* outId, QString* outToken, QString* errOut)
{
    ConnectionPool::Scoped s; auto db = s.db();
    if (!db.isOpen()) { if (errOut) *errOut = "DB not open"; return false; }

    const QString token = makeToken();
    const qint64 now = nowMs();

    QSqlQuery q(db);
    q.prepare("INSERT INTO sessions(username, client_id, token, created_at, last_seen, active) "
              "VALUES(?,?,?,?,?,1)");
    q.addBindValue(username);
    q.addBindValue(clientId);
    q.addBindValue(token);
    q.addBindValue(now);
    q.addBindValue(now);

    if (!q.exec()) { if (errOut) *errOut = q.lastError().text(); return false; }

    const qint64 id = q.lastInsertId().toLongLong();
    if (outId) *outId = id;
    if (outToken) *outToken = token;
    return true;
}

bool SessionService::heartbeat(qint64 sessionId, const QString& token)
{
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return false;

    QSqlQuery q(db);
    q.prepare("UPDATE sessions SET last_seen=?, active=1 "
              "WHERE id=? AND token=?");
    q.addBindValue(nowMs());
    q.addBindValue(sessionId);
    q.addBindValue(token);
    return q.exec();
}

bool SessionService::endSession(qint64 sessionId, const QString& token)
{
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return false;

    QSqlQuery q(db);
    q.prepare("UPDATE sessions SET active=0, last_seen=? WHERE id=? AND token=?");
    q.addBindValue(nowMs());
    q.addBindValue(sessionId);
    q.addBindValue(token);
    return q.exec();
}

QVector<SessionRow> SessionService::listActive(int staleMs)
{
    QVector<SessionRow> out;
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return out;

    QSqlQuery q(db);
    q.prepare("SELECT id, username, client_id, token, created_at, last_seen, active "
              "FROM sessions WHERE active=1 AND last_seen>=? "
              "ORDER BY last_seen DESC LIMIT 500");
    q.addBindValue(nowMs() - qint64(staleMs));
    if (!q.exec()) return out;

    while (q.next()) {
        SessionRow r;
        r.id = q.value(0).toLongLong();
        r.username = q.value(1).toString();
        r.clientId = q.value(2).toString();
        r.token = q.value(3).toString();
        r.createdAt = q.value(4).toLongLong();
        r.lastSeen = q.value(5).toLongLong();
        r.active = q.value(6).toInt() != 0;
        out.push_back(r);
    }
    return out;
}

void SessionService::logLogin(const QString& username, bool success, const QString& reason,
                              const QString& clientId, const QString& token)
{
    ConnectionPool::Scoped s; auto db = s.db(); if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare("INSERT INTO login_log(username, success, reason, client_id, token, ts) "
              "VALUES(?,?,?,?,?,?)");
    q.addBindValue(username);
    q.addBindValue(success ? 1 : 0);
    q.addBindValue(reason);
    q.addBindValue(clientId);
    q.addBindValue(token);
    q.addBindValue(nowMs());
    q.exec(); // 실패해도 앱 플로우에 영향 없음
}
