#pragma once
#ifndef SESSIONSERVICE_H
#define SESSIONSERVICE_H


#include <QtCore>
#include <QtSql>


struct SessionRow {
    qint64 id{};
    QString username;
    QString clientId;
    QString token;
    qint64 createdAt{}; // ms epoch
    qint64 lastSeen{}; // ms epoch
    bool active{false};
};


class SessionService : public QObject {
    Q_OBJECT
public:
    explicit SessionService(QObject* parent=nullptr);


    // Returns true if there is an active (non‑stale) session for username
    bool hasActiveSession(const QString& username,
                          int staleMs,
                          qint64* outId,
                          QString* outClientId);


    // Begin a new session after auth success
    bool beginSession(const QString& username, const QString& clientId,
                      qint64* outId, QString* outToken, QString* errOut);


    // Heartbeat (30s recommended)
    bool heartbeat(qint64 sessionId, const QString& token);


    // End session (logout / normal exit)
    bool endSession(qint64 sessionId, const QString& token);


    // Admin popup: list of active (and non‑stale) sessions
    QVector<SessionRow> listActive(int staleMs);


    // Record login attempt
    void logLogin(const QString& username, bool success, const QString& reason,
                  const QString& clientId, const QString& token=QString());
};


#endif // SESSIONSERVICE_H
