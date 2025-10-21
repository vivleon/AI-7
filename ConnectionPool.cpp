#include "ConnectionPool.h"

ConnectionPool& ConnectionPool::instance() {
    static ConnectionPool inst;
    return inst;
}

ConnectionPool::ConnectionPool(QObject* parent) : QObject(parent) {}

void ConnectionPool::configure(const Config& c) {
    QMutexLocker lk(&m_);
    cfg_ = c;
}

QString ConnectionPool::acquire(int waitMs) {
    QMutexLocker lk(&m_);
    const auto deadline = QDeadlineTimer(waitMs);
    while (idle_.isEmpty() && created_ >= cfg_.maxPool) {
        if (!cond_.wait(&m_, deadline.remainingTime())) break;
    }
    QString name;
    if (!idle_.isEmpty()) {
        name = idle_.dequeue();
    } else if (created_ < cfg_.maxPool) {
        name = QString("pool-%1").arg(++created_);
    }
    if (!name.isEmpty()) used_.insert(name);
    return name;
}

void ConnectionPool::release(const QString& name) {
    if (name.isEmpty()) return;
    QMutexLocker lk(&m_);
    if (used_.remove(name)) {
        idle_.enqueue(name);
        cond_.wakeOne();
    }
}

QSqlDatabase ConnectionPool::openDb(const QString& name) {
    QSqlDatabase db = QSqlDatabase::contains(name)
        ? QSqlDatabase::database(name)
        : QSqlDatabase::addDatabase(cfg_.driver, name);

    db.setHostName(cfg_.host);
    db.setPort(cfg_.port);
    db.setDatabaseName(cfg_.db);
    db.setUserName(cfg_.user);
    db.setPassword(cfg_.pw);

    QString opts;
    if (cfg_.useSSL) opts += "CLIENT_SSL=1;";
    // 안정성 옵션(사용 가능한 경우에만 적용됨)
    opts += "MYSQL_OPT_RECONNECT=1;MYSQL_OPT_CONNECT_TIMEOUT=5;MYSQL_OPT_READ_TIMEOUT=8;MYSQL_OPT_WRITE_TIMEOUT=8;";
    if (!cfg_.connectOptions.trimmed().isEmpty()) {
        opts += cfg_.connectOptions.trimmed();
        if (!opts.endsWith(';')) opts += ';';
    }
    db.setConnectOptions(opts);

    for (int a=0; a<cfg_.openRetries; ++a) {
        if (db.isOpen()) return db;
        if (db.open()) return db;
        QThread::msleep(cfg_.openBackoffMs * (1<<a));
    }
    return db;
}

ConnectionPool::Scoped::Scoped() {
    ConnectionPool& p = ConnectionPool::instance();
    name_ = p.acquire();
    if (!name_.isEmpty()) {
        db_ = p.openDb(name_);
    }
}

ConnectionPool::Scoped::~Scoped() {
    if (!name_.isEmpty())
        ConnectionPool::instance().release(name_);
}

