// [db.c]
#include <stdio.h>
#include <string.h>
#include <mysql/mysql.h>
#include "db.h"
#include "member.h"
#include <stdlib.h>
#include <openssl/sha.h>
#include "password_utils.h"


MYSQL *conn;


// DB 연결
int db_connect() {
    conn = mysql_init(NULL);


    // 환경변수로 인증서 경로 지정 (하드코딩도 가능)
    const char *ssl_dir = getenv("MYSQL_SSL_DIR");
    char ca[256], cert[256], key[256];
    snprintf(ca, sizeof(ca), "%s/ca-cert.pem", ssl_dir ? ssl_dir : ".");
    snprintf(cert, sizeof(cert), "%s/client-cert.pem", ssl_dir ? ssl_dir : ".");
    snprintf(key, sizeof(key), "%s/client-key.pem", ssl_dir ? ssl_dir : ".");

    // [★] SSL 옵션 적용 (mysql_real_connect 전에 반드시 호출)
    mysql_ssl_set(conn, key, cert, ca, NULL, NULL);


    const char *host = getenv("CHATDB_HOST");
    const char *port = getenv("CHATDB_PORT");
    const char *user = getenv("CHATDB_USER");
    const char *pass = getenv("CHATDB_PASS");
    const char *name = getenv("CHATDB_NAME");
    unsigned int port_num = (port ? atoi(port) : 3306);

    if (!mysql_real_connect(conn,
            host  ? host  : "localhost",
            user  ? user  : "chatapp_user",
            pass  ? pass  : "qwer12341234!",
            name  ? name  : "chatdb",
            port_num, NULL, 0))
    {
        fprintf(stderr, "[DB] 연결 실패: %s\n", mysql_error(conn));
        return 0;
    }
    // UTF8mb4 설정
    if (mysql_set_character_set(conn, "utf8mb4") != 0) {
        fprintf(stderr, "[DB] 문자셋 설정 실패: %s\n", mysql_error(conn));
        // continue anyway
    }
    return 1;
}

// ID 중복체크 (SELECT id FROM members WHERE id=?)
int db_id_exists(const char *id) {
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;
    const char *sql = "SELECT id FROM members WHERE id=?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }
    MYSQL_BIND bind[1] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char *)id; bind[0].buffer_length = strlen(id);
    mysql_stmt_bind_param(stmt, bind);

    int exists = 0;
    if (mysql_stmt_execute(stmt) == 0) {
        MYSQL_RES *res = mysql_stmt_result_metadata(stmt);
        if (res) {
            MYSQL_BIND out_bind[1] = {0};
            char out_id[20] = {0};
            out_bind[0].buffer_type = MYSQL_TYPE_STRING;
            out_bind[0].buffer = out_id;
            out_bind[0].buffer_length = sizeof(out_id);
            mysql_stmt_bind_result(stmt, out_bind);
            if (mysql_stmt_fetch(stmt) == 0) exists = 1;
            mysql_free_result(res);
        }
    } else {
        fprintf(stderr, "[DB] db_id_exists execute failed: %s\n", mysql_stmt_error(stmt));
    }
    mysql_stmt_close(stmt);
    return exists;
}

// 회원가입 (전체정보)
int db_register_full(const char *id, const char *pw, const char *fname,
                     const char *lname, const char *phone, const char *email) {
    char hash[65];
    hash_password_sha256(pw, hash);

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;
    const char *sql = "INSERT INTO members (id, pw, fname, lname, ph, email) VALUES (?, ?, ?, ?, ?, ?)";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        fprintf(stderr, "[DB] db_register_full prepare failed: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }
    MYSQL_BIND bind[6] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char *)id;    bind[0].buffer_length = strlen(id);
    bind[1].buffer_type = MYSQL_TYPE_STRING; bind[1].buffer = (char *)hash;  bind[1].buffer_length = strlen(hash);
    bind[2].buffer_type = MYSQL_TYPE_STRING; bind[2].buffer = (char *)fname; bind[2].buffer_length = strlen(fname);
    bind[3].buffer_type = MYSQL_TYPE_STRING; bind[3].buffer = (char *)lname; bind[3].buffer_length = strlen(lname);
    bind[4].buffer_type = MYSQL_TYPE_STRING; bind[4].buffer = (char *)phone; bind[4].buffer_length = strlen(phone);
    bind[5].buffer_type = MYSQL_TYPE_STRING; bind[5].buffer = (char *)email; bind[5].buffer_length = strlen(email);

    mysql_stmt_bind_param(stmt, bind);
    if (mysql_stmt_execute(stmt) != 0) {
        fprintf(stderr,
                "[DB] db_register_full execute failed: stmt_errno=%u, stmt_err='%s'\n",
                mysql_stmt_errno(stmt), mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }
    mysql_stmt_close(stmt);
    return 1;
}

// 회원가입 (ID/PW만, 비회원가입 경로)
int db_register(const char *id, const char *pw) {
    char hash[65];
    hash_password_sha256(pw, hash);

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;
    const char *sql = "INSERT INTO members (id, pw, fname, lname, ph, email, disabled) VALUES (?, ?, '', '', '', '', 0)";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }
    MYSQL_BIND bind[2] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char *)id;   bind[0].buffer_length = strlen(id);
    bind[1].buffer_type = MYSQL_TYPE_STRING; bind[1].buffer = (char *)hash; bind[1].buffer_length = strlen(hash);

    mysql_stmt_bind_param(stmt, bind);
    int result = (mysql_stmt_execute(stmt) == 0);
    mysql_stmt_close(stmt);
    return result;
}

// 로그인
int db_login(const char *id, const char *pw) {
    char hash[65];
    hash_password_sha256(pw, hash);

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;
    const char *sql = "SELECT id FROM members WHERE id=? AND pw=? AND disabled=0";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }
    MYSQL_BIND bind[2] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char *)id;   bind[0].buffer_length = strlen(id);
    bind[1].buffer_type = MYSQL_TYPE_STRING; bind[1].buffer = (char *)hash; bind[1].buffer_length = strlen(hash);

    mysql_stmt_bind_param(stmt, bind);
    int found = 0;
    if (mysql_stmt_execute(stmt) == 0) {
        MYSQL_RES *res = mysql_stmt_result_metadata(stmt);
        if (res) {
            MYSQL_BIND out_bind[1] = {0};
            char out_id[20] = {0};
            out_bind[0].buffer_type = MYSQL_TYPE_STRING;
            out_bind[0].buffer = out_id;
            out_bind[0].buffer_length = sizeof(out_id);
            mysql_stmt_bind_result(stmt, out_bind);
            if (mysql_stmt_fetch(stmt) == 0) found = 1;
            mysql_free_result(res);
        }
    }
    mysql_stmt_close(stmt);
    return found;
}

// 계정 삭제 (disabled=1)
int db_delete(const char *id) {
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;
    const char *sql = "UPDATE members SET disabled=1 WHERE id=?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }
    MYSQL_BIND bind[1] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char *)id; bind[0].buffer_length = strlen(id);

    mysql_stmt_bind_param(stmt, bind);
    int result = (mysql_stmt_execute(stmt) == 0);
    mysql_stmt_close(stmt);
    return result;
}

// 회원정보 조회
int db_get_member_info(const char *id, Member *m) {
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;
    const char *sql = "SELECT id, pw, fname, lname, ph, email, disabled, is_admin FROM members WHERE id=?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }
    MYSQL_BIND bind[1] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char *)id; bind[0].buffer_length = strlen(id);
    mysql_stmt_bind_param(stmt, bind);

    int found = 0;
    if (mysql_stmt_execute(stmt) == 0) {
        MYSQL_RES *res = mysql_stmt_result_metadata(stmt);
        if (res) {
            MYSQL_BIND out_bind[8] = {0};
            char out_id[20] = {0}, out_pw[65] = {0}, out_fname[50] = {0}, out_lname[50] = {0};
            char out_ph[20] = {0}, out_email[50] = {0};
            int out_disabled = 0;
            unsigned char out_is_admin = 0;
            
            out_bind[0].buffer_type = MYSQL_TYPE_STRING; out_bind[0].buffer = out_id;     out_bind[0].buffer_length = sizeof(out_id);
            out_bind[1].buffer_type = MYSQL_TYPE_STRING; out_bind[1].buffer = out_pw;     out_bind[1].buffer_length = sizeof(out_pw);
            out_bind[2].buffer_type = MYSQL_TYPE_STRING; out_bind[2].buffer = out_fname;  out_bind[2].buffer_length = sizeof(out_fname);
            out_bind[3].buffer_type = MYSQL_TYPE_STRING; out_bind[3].buffer = out_lname;  out_bind[3].buffer_length = sizeof(out_lname);
            out_bind[4].buffer_type = MYSQL_TYPE_STRING; out_bind[4].buffer = out_ph;     out_bind[4].buffer_length = sizeof(out_ph);
            out_bind[5].buffer_type = MYSQL_TYPE_STRING; out_bind[5].buffer = out_email;  out_bind[5].buffer_length = sizeof(out_email);
            out_bind[6].buffer_type = MYSQL_TYPE_LONG;   out_bind[6].buffer = &out_disabled; out_bind[6].is_unsigned = 0;
            out_bind[7].buffer_type = MYSQL_TYPE_TINY;   out_bind[7].buffer = &out_is_admin;  out_bind[7].is_unsigned = 1;

            mysql_stmt_bind_result(stmt, out_bind);

            if (mysql_stmt_fetch(stmt) == 0) {
                strncpy(m->id, out_id, sizeof(m->id));
                strncpy(m->pw, out_pw, sizeof(m->pw));
                strncpy(m->fname, out_fname, sizeof(m->fname));
                strncpy(m->lname, out_lname, sizeof(m->lname));
                strncpy(m->ph, out_ph, sizeof(m->ph));
                strncpy(m->email, out_email, sizeof(m->email));
                m->disabled = out_disabled;
                m->is_admin = (int)out_is_admin; // 새로 추가된 is_admin 저장
                found = (m->disabled == 0);
            }
            mysql_free_result(res);
        }
    }
    mysql_stmt_close(stmt);
    return found;
}

// 1) 채팅 로그 기록
int db_log_chat(const char *room,
                const char *sender,
                const char *message,
                const char *ts)
{
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char *sql =
      "INSERT INTO chat_logs (room_name, sender_id, message, ts) "
      "VALUES (?, ?, ?, ?)";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        fprintf(stderr,
                "[DB_LOG] prepare chat_logs failed: %s\n",
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));

    // room_name
    bind[0].buffer_type   = MYSQL_TYPE_STRING;
    bind[0].buffer        = (char *)room;
    bind[0].buffer_length = strlen(room);
    // sender_id
    bind[1].buffer_type   = MYSQL_TYPE_STRING;
    bind[1].buffer        = (char *)sender;
    bind[1].buffer_length = strlen(sender);
    // message
    bind[2].buffer_type   = MYSQL_TYPE_STRING;
    bind[2].buffer        = (char *)message;
    bind[2].buffer_length = strlen(message);
    // ts
    bind[3].buffer_type   = MYSQL_TYPE_STRING;
    bind[3].buffer        = (char *)ts;
    bind[3].buffer_length = strlen(ts);

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        fprintf(stderr,
                "[DB_LOG] bind chat_logs failed: %s\n",
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }
    if (mysql_stmt_execute(stmt) != 0) {
        fprintf(stderr,
                "[DB_LOG ERROR] chat_logs insert failed: stmt_errno=%u, stmt_err='%s'\n",
                mysql_stmt_errno(stmt),
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }
    mysql_stmt_close(stmt);
    return 1;
}

// 2) 귓속말 로그 기록
int db_log_note(const char *sender,
                const char *target,
                const char *message,
                const char *ts)
{
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char *sql =
      "INSERT INTO note_logs (sender_id, target_id, message, ts) "
      "VALUES (?, ?, ?, ?)";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        fprintf(stderr,
                "[DB_LOG] prepare note_logs failed: %s\n",
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));

    // sender_id
    bind[0].buffer_type   = MYSQL_TYPE_STRING;
    bind[0].buffer        = (char *)sender;
    bind[0].buffer_length = strlen(sender);
    // target_id
    bind[1].buffer_type   = MYSQL_TYPE_STRING;
    bind[1].buffer        = (char *)target;
    bind[1].buffer_length = strlen(target);
    // message
    bind[2].buffer_type   = MYSQL_TYPE_STRING;
    bind[2].buffer        = (char *)message;
    bind[2].buffer_length = strlen(message);
    // ts
    bind[3].buffer_type   = MYSQL_TYPE_STRING;
    bind[3].buffer        = (char *)ts;
    bind[3].buffer_length = strlen(ts);

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        fprintf(stderr,
                "[DB_LOG] bind note_logs failed: %s\n",
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }
    if (mysql_stmt_execute(stmt) != 0) {
        fprintf(stderr,
                "[DB_LOG ERROR] note_logs insert failed: stmt_errno=%u, stmt_err='%s'\n",
                mysql_stmt_errno(stmt),
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }
    mysql_stmt_close(stmt);
    return 1;
}

// 3) 방 생성 기록
int db_create_room(int room_id,
                   const char *name,
                   const char *owner,
                   int is_public,
                   const char *password,
                   const char *created_at)
{
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return 0;

    const char *sql =
      "INSERT INTO rooms "
      "(room_id, name, owner, is_public, password, created_at) "
      "VALUES (?, ?, ?, ?, ?, ?)";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND bind[6];
    memset(bind, 0, sizeof(bind));

    // room_id
    bind[0].buffer_type   = MYSQL_TYPE_LONG;
    bind[0].buffer        = (char *)&room_id;
    bind[0].is_unsigned   = 1;
    // name
    bind[1].buffer_type   = MYSQL_TYPE_STRING;
    bind[1].buffer        = (char *)name;
    bind[1].buffer_length = strlen(name);
    // owner
    bind[2].buffer_type   = MYSQL_TYPE_STRING;
    bind[2].buffer        = (char *)owner;
    bind[2].buffer_length = strlen(owner);
    // is_public
    bind[3].buffer_type   = MYSQL_TYPE_TINY;
    bind[3].buffer        = (char *)&is_public;
    // password
    bind[4].buffer_type   = MYSQL_TYPE_STRING;
    bind[4].buffer        = (char *)password;
    bind[4].buffer_length = strlen(password);
    // created_at
    bind[5].buffer_type   = MYSQL_TYPE_STRING;
    bind[5].buffer        = (char *)created_at;
    bind[5].buffer_length = strlen(created_at);

    if (mysql_stmt_bind_param(stmt, bind) ||
        mysql_stmt_execute(stmt))
    {
        mysql_stmt_close(stmt);
        return 0;
    }
    mysql_stmt_close(stmt);
    return 1;
}

void db_close() {
    if (conn) mysql_close(conn);
}

// 1) 관리자용 비밀번호 변경
bool db_admin_change_pw(const char *target_id, const char *new_pw) {
    char hash[65];
    hash_password_sha256(new_pw, hash);
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    const char *sql = "UPDATE members SET pw=? WHERE id=?";
    mysql_stmt_prepare(stmt, sql, strlen(sql));
    MYSQL_BIND b[2] = {{0}};
    b[0].buffer_type = MYSQL_TYPE_STRING; b[0].buffer = hash; b[0].buffer_length = strlen(hash);
    b[1].buffer_type = MYSQL_TYPE_STRING; b[1].buffer = (char*)target_id; b[1].buffer_length = strlen(target_id);
    mysql_stmt_bind_param(stmt, b);
    bool ok = (mysql_stmt_execute(stmt) == 0);
    mysql_stmt_close(stmt);
    return ok;
}


// 2) disabled 값 설정
bool db_set_disabled(const char *target_id, int disabled) {
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    const char *sql = "UPDATE members SET disabled=? WHERE id=?";
    mysql_stmt_prepare(stmt, sql, strlen(sql));
    MYSQL_BIND b[2] = {{0}};
    b[0].buffer_type = MYSQL_TYPE_LONG; b[0].buffer = (char*)&disabled; b[0].is_unsigned = 0;
    b[1].buffer_type = MYSQL_TYPE_STRING; b[1].buffer = (char*)target_id; b[1].buffer_length = strlen(target_id);
    mysql_stmt_bind_param(stmt, b);
    bool ok = (mysql_stmt_execute(stmt) == 0);
    mysql_stmt_close(stmt);
    return ok;
}

// 3) 완전 삭제
bool db_purge_user(const char *target_id) {
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    const char *sql = "DELETE FROM members WHERE id=?";
    mysql_stmt_prepare(stmt, sql, strlen(sql));
    MYSQL_BIND b = {0};
    b.buffer_type = MYSQL_TYPE_STRING; b.buffer = (char*)target_id; b.buffer_length = strlen(target_id);
    mysql_stmt_bind_param(stmt, &b);
    bool ok = (mysql_stmt_execute(stmt) == 0);
    mysql_stmt_close(stmt);
    return ok;
}

// 4) disabled 조회
bool db_is_disabled(const char *target_id) {
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    const char *sql = "SELECT disabled FROM members WHERE id=?";
    mysql_stmt_prepare(stmt, sql, strlen(sql));
    MYSQL_BIND in = {0}, out = {0};
    int disabled = 0;
    in.buffer_type = MYSQL_TYPE_STRING; in.buffer = (char*)target_id; in.buffer_length = strlen(target_id);
    out.buffer_type = MYSQL_TYPE_LONG; out.buffer = &disabled; out.is_unsigned = 0;
    mysql_stmt_bind_param(stmt, &in);
    mysql_stmt_bind_result(stmt, &out);
    bool ok = (mysql_stmt_execute(stmt) == 0 && mysql_stmt_fetch(stmt) == 0);
    mysql_stmt_close(stmt);
    return ok ? disabled : false;
}

// 2) 관리자용 전화번호 변경
bool db_admin_change_ph(const char *target_id, const char *new_phone) {
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    const char *sql = "UPDATE members SET ph=? WHERE id=?";
    mysql_stmt_prepare(stmt, sql, strlen(sql));
    MYSQL_BIND b[2] = {{0}};
    b[0].buffer_type = MYSQL_TYPE_STRING; b[0].buffer = (char*)new_phone; b[0].buffer_length = strlen(new_phone);
    b[1].buffer_type = MYSQL_TYPE_STRING; b[1].buffer = (char*)target_id; b[1].buffer_length = strlen(target_id);
    mysql_stmt_bind_param(stmt, b);
    bool ok = (mysql_stmt_execute(stmt) == 0);
    mysql_stmt_close(stmt);
    return ok;
}
// 3) 관리자용 이메일 변경
bool db_admin_change_email(const char *target_id, const char *new_email) {
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    const char *sql = "UPDATE members SET email=? WHERE id=?";
    mysql_stmt_prepare(stmt, sql, strlen(sql));
    MYSQL_BIND b[2] = {{0}};
    b[0].buffer_type = MYSQL_TYPE_STRING; b[0].buffer = (char*)new_email; b[0].buffer_length = strlen(new_email);
    b[1].buffer_type = MYSQL_TYPE_STRING; b[1].buffer = (char*)target_id; b[1].buffer_length = strlen(target_id);
    mysql_stmt_bind_param(stmt, b);
    bool ok = (mysql_stmt_execute(stmt) == 0);
    mysql_stmt_close(stmt);
    return ok;
}
