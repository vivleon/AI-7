// [db.h]
#ifndef DB_H
#define DB_H

#include <mysql/mysql.h>
#include "member.h"   
#include <stdbool.h>

extern MYSQL *conn;  // 추가: 전역 DB 연결 객체

int db_connect();
int db_get_member_info(const char *id, Member *m);
int db_register_full(const char *id, const char *pw, const char *fname, const char *lname, const char *ph, const char *email);
int db_register(const char *id, const char *pw);
int db_login(const char *id, const char *pw);
int db_delete(const char *id);
// db.h 에 선언 추가
int db_id_exists(const char *id);

void db_close();
// 새로운 로그 함수
int db_log_chat(const char *room,
                const char *sender,
                const char *message,
                const char *ts);

int db_log_note(const char *sender,
                const char *target,
                const char *message,
                const char *ts);

int db_create_room(int room_id,
                   const char *name,
                   const char *owner,
                   int is_public,
                   const char *password,
                   const char *created_at);

// 관리자 전용 helper
bool db_admin_change_pw(const char *target_id, const char *new_pw);
bool db_admin_change_ph(const char *target_id, const char *new_phone);
bool db_admin_change_email(const char *target_id, const char *new_email);
bool db_set_disabled(const char *target_id, int disabled);
bool db_purge_user(const char *target_id);
bool db_is_disabled(const char *target_id);

#endif