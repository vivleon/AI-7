#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#pragma once
#include <QtCore>
#include <QtSql>

class ConnectionPool : public QObject {
    Q_OBJECT
public:
    static ConnectionPool& instance();

    struct Config {
        QString driver {"QMYSQL"};
        QString host;
        int     port {3306};
        QString db;
        QString user;
        QString pw;
        bool    useSSL {false};
        int     maxPool {8};
        int     openRetries {3};
        int     openBackoffMs {200}; // 200, 400, 800...
        QString connectOptions; // 추가 옵션
    };

    void configure(const Config& c);

    class Scoped {
    public:
        Scoped();
        ~Scoped();
        QSqlDatabase db() const { return db_; }
        bool isValid() const { return db_.isValid(); }
    private:
        QString name_;
        QSqlDatabase db_;
    };

private:
    explicit ConnectionPool(QObject* parent=nullptr);
    Q_DISABLE_COPY(ConnectionPool)

    QString acquire(int waitMs = 1500);
    void release(const QString& name);
    QSqlDatabase openDb(const QString& name);

    Config cfg_;
    QMutex m_;
    QWaitCondition cond_;
    QSet<QString> used_;
    QQueue<QString> idle_;
    int created_ {0};
};

#endif // CONNECTIONPOOL_H
