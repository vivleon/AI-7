#include <QApplication>
#include <QMessageBox>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <memory>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QGuiApplication>

#include "DBBridge.h"
#include "LoginDialog.h"
#include "MainDashboard.h"
#include "SessionService.h"


static QString clientId()
{
    QString host = QHostInfo::localHostName(); QString mac;
    const auto ifs = QNetworkInterface::allInterfaces();
    for (const auto& it : ifs) {
        if (it.flags().testFlag(QNetworkInterface::IsUp)
            && !it.hardwareAddress().isEmpty()
            && it.hardwareAddress() != "00:00:00:00:00:00") { mac = it.hardwareAddress(); break; }
    }
    return host + "|" + mac;
}

// host:port,host2:port2
static QList<QPair<QString,int>> parseHosts(const QString& csv)
{
    QList<QPair<QString,int>> out; const auto parts = csv.split(',', Qt::SkipEmptyParts);
    for (const auto& part : parts) { const auto s = part.trimmed(); if (s.isEmpty()) continue; const int colon = s.lastIndexOf(':'); if (colon > 0) out.append({ s.left(colon), s.mid(colon+1).toInt() }); else out.append({ s, 3306 }); }
    return out;
}

static QString resolveIniPath()
{
    const QString env = qEnvironmentVariable("VIGILEDGE_DB_INI"); if (!env.isEmpty() && QFileInfo::exists(env)) return env;
    const QString appDir = QCoreApplication::applicationDirPath(); const QString cand1 = appDir + "/db.ini"; if (QFileInfo::exists(cand1)) return cand1;
    QDir d(appDir); for (int up=1; up<=5; ++up) { QDir t = d; for (int i=0; i<up; ++i) t.cdUp(); const QString cand = t.absoluteFilePath("db.ini"); if (QFileInfo::exists(cand)) return cand; }
    const QString confDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!confDir.isEmpty()) { const QString cand = QDir(confDir).absoluteFilePath("db.ini"); if (QFileInfo::exists(cand)) return cand; }
    return QString();
}

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);

    // DB 설정 로드
    const QString iniPath = resolveIniPath(); std::unique_ptr<QSettings> inip;
    if (!iniPath.isEmpty()) { qInfo().noquote() << "[CFG] Using ini file:" << iniPath; inip = std::make_unique<QSettings>(iniPath, QSettings::IniFormat); }
    else { qInfo() << "[CFG] No db.ini; using UserScope settings (VigilEdge/VigilEdge)."; inip = std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope, "VigilEdge", "VigilEdge"); }
    QSettings& ini = *inip;

    auto envOr = [](const char* key, const QString& def)->QString { const QByteArray v = qgetenv(key); return v.isEmpty() ? def : QString::fromUtf8(v); };

    ini.beginGroup("database"); QString hostsCsv = ini.value("hosts", "127.0.0.1:3306").toString(); QString dbName   = ini.value("name", "vigiledge").toString(); QString dbUser   = ini.value("user", "vigiledge_app").toString(); QString dbPass   = ini.value("password", "").toString(); ini.endGroup();
    ini.beginGroup("options"); bool useSSL = ini.value("ssl", 0).toInt() != 0; ini.endGroup();

    hostsCsv = envOr("VIGILEDGE_DB_HOSTS", hostsCsv); dbName = envOr("VIGILEDGE_DB_NAME", dbName); dbUser = envOr("VIGILEDGE_DB_USER", dbUser); dbPass = envOr("VIGILEDGE_DB_PASS", dbPass); useSSL = envOr("VIGILEDGE_DB_SSL", useSSL ? "1" : "0").toInt() != 0;

    if (qEnvironmentVariableIsSet("VIGILEDGE_LOCAL_ONLY")) { hostsCsv = "127.0.0.1:3306"; qInfo() << "[CFG] Local-only mode enabled"; }
    if (hostsCsv.trimmed().isEmpty()) hostsCsv = "127.0.0.1:3306";

    if (dbPass.isEmpty()) {
        QMessageBox::critical(nullptr, "Database Config Error",
                              "DB 비밀번호가 비어있습니다. db.ini 또는 환경변수(VIGILEDGE_DB_PASS)를 확인하세요.");
        return 1;
    }

    // DB 연결
    DBBridge db; QString err; auto hosts = parseHosts(hostsCsv);
    if (!db.connectWithCandidates(hosts, dbName, dbUser, dbPass, useSSL, &err)) {
        const QString msg = QString("DB 접속 실패\n- hosts: %1\n- db: %2\n- user: %3\n- ssl: %4\n\n에러: %5")
                                .arg(hostsCsv, dbName, dbUser, useSSL?"on":"off", err);
        qCritical().noquote() << "[DB] connect failed:" << msg;
        QMessageBox::critical(nullptr, "Database Error", msg);
        return 1;
    }

    SessionService sess; const QString cid = clientId();

RELOGIN:
    while (true) {
        LoginDialog dlg;
        if (dlg.exec() != QDialog::Accepted) {
            // 로그인 창에서 취소 → 프로그램 종료 (#2-1, #9)
            return 0;
        }
        const QString u = dlg.username().trimmed(); const QString p = dlg.password();

        const AuthResult ar = db.authenticate(u, p);
        if (!ar.ok) { sess.logLogin(u, false, ar.error, cid); QMessageBox::warning(nullptr, "Login", ar.error); continue; }

        // 중복 로그인 방지 (stale 120s 이내)
        qint64 existing=-1;
        if (sess.hasActiveSession(u, /*staleMs*/40000, &existing, nullptr)) {
            sess.logLogin(u, false, "duplicate_active_session", cid);
            QMessageBox::warning(nullptr, "Login", "다른 곳에서 이미 로그인 중입니다.");
            continue;
        }

        // 세션 시작
        qint64 sid=-1; QString token; QString sErr;
        if (!sess.beginSession(u, cid, &sid, &token, &sErr)) {
            sess.logLogin(u, false, "begin_session_failed:"+sErr, cid);
            QMessageBox::warning(nullptr, "Login", "세션 시작 실패: "+sErr);
            continue;
        }
        sess.logLogin(u, true, "", cid, token);

        // 메인
        MainDashboard w(&db, u, ar.role);
        w.setSession(sid, token);   // 하트비트 시작
        w.show();
        const int rc = app.exec();

        // 정상 종료 시 세션 종료
        sess.endSession(sid, token);

        if (rc == 777) { // 로그아웃 → 재로그인 루프
            goto RELOGIN;
        }
        return rc;
    }
}
