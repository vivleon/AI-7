#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>   
#include "member.h"
#include "db.h"
#include "global.h"
#include "handler.h" 
#include "account.h"
#include <mysql/mysql.h>
#include "password_utils.h"
#include "chat_util.h"


#define ESC_MSG_MAX    (BUF_SIZE - 1)
#define BUF_SIZE 2048



// 비밀번호/전화번호/이메일 수정
void handle_modify_info(const char *id, const char *args) {
    char new_pw[100], new_phone[32], new_email[100], hash[65];

    if (sscanf(args, "%s %s %s", new_pw, new_phone, new_email) != 3) {
        printf("[오류] 입력 파라미터 부족\n");
        return;
    }

    // 패스워드 강도 및 입력값 검증
    // 전화번호/이메일도 유효성 검사 
    if (!is_valid_password(new_pw)) {
        printf("[오류] 비밀번호 정책 위반: 8자 이상, 대/소문자/숫자/특수문자 각각 1개 이상 포함 필수\n");
        return;
    }
    if (!is_valid_phone(new_phone)) {
        printf("[오류] 전화번호 형식 오류(숫자, 10~13자리)\n");
        return;
    }
    if (!is_valid_email(new_email)) {
        printf("[오류] 이메일 형식 오류(@와 . 포함, 최소 5자)\n");
        return;
    }

    // 패스워드 해시화 (SHA-256 등)
    hash_password_sha256(new_pw, hash);

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        printf("[오류] DB 준비 실패\n");
        return;
    }
    const char *sql = "UPDATE members SET pw=?, ph=?, email=? WHERE id=?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        printf("[오류] 쿼리 준비 실패: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return;
    }
    MYSQL_BIND bind[4] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char *)hash;     bind[0].buffer_length = strlen(hash);
    bind[1].buffer_type = MYSQL_TYPE_STRING; bind[1].buffer = (char *)new_phone; bind[1].buffer_length = strlen(new_phone);
    bind[2].buffer_type = MYSQL_TYPE_STRING; bind[2].buffer = (char *)new_email; bind[2].buffer_length = strlen(new_email);
    bind[3].buffer_type = MYSQL_TYPE_STRING; bind[3].buffer = (char *)id;        bind[3].buffer_length = strlen(id);

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        printf("[오류] 바인딩 실패: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return;
    }
    if (mysql_stmt_execute(stmt) != 0) {
        printf("[오류] 실행 실패: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return;
    }
    mysql_stmt_close(stmt);
    printf("[수정 완료] %s 정보 수정됨\n", id);
}

// 회원 탈퇴 처리 (disabled=1)
void handle_delete_account(const char *id, int sock) {
    write(sock, "정말 탈퇴하시겠습니까? (Y/N): \n", strlen("정말 탈퇴하시겠습니까? (Y/N): \n"));
    char yn[4];
    if (read_line(sock, yn, sizeof(yn)) > 0 && (yn[0]=='Y'||yn[0]=='y')) {
        if (db_delete(id)) {
            write(sock, "[탈퇴 완료] 계정이 비활성화 되었습니다.\n", strlen("[탈퇴 완료] 계정이 비활성화 되었습니다.\n"));
            // active_users에서 삭제
            char sql[128];
            snprintf(sql, sizeof(sql), "DELETE FROM active_users WHERE user_id='%s'", id);
            mysql_query(conn, sql);
            // members[] 배열에서도 제거
            pthread_mutex_lock(&members_mutex);
            for (int i = 0; i < member_count; i++) {
                if (strcmp(members[i].id, id) == 0) {
                    for (int j = i; j < member_count-1; j++)
                        members[j] = members[j+1];
                    member_count--;
                    break;
                }
            }
            pthread_mutex_unlock(&members_mutex);
        } else {
            write(sock, "[오류] 탈퇴 처리 실패\n", strlen("[오류] 탈퇴 처리 실패\n"));
        }
    } else {
        write(sock, "[탈퇴 취소]\n", strlen("[탈퇴 취소]\n"));
    }
}

// 1. 전체 회원 조회
void handle_view_all_members(int sock) {
    const char *sql = 
        "SELECT id, fname, lname, ph, email, disabled FROM members";
    if (mysql_query(conn, sql) != 0) {
        const char *err = "[SQL 오류] 전체 조회 실패\n";
        write(sock, err, strlen(err));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) {
        const char *err = "[SQL 오류] 결과 저장 실패\n";
        write(sock, err, strlen(err));
        return;
    }

    // 1) 헤더
    write(sock, "[전체 회원 목록]\n", strlen("[전체 회원 목록]\n"));
    char header[128];
    snprintf(header, sizeof(header),
        "%-20s | %-10s | %-10s | %-15s | %-30s | %-8s\n",
        "ID", "이름", "성", "전화번호", "이메일", "상태");
    write(sock, header, strlen(header));
    write(sock,
          "----------------------+------------+------------+-----------------+--------------------------------+----------\n",
          strlen(
    "----------------------+------------+------------+-----------------+--------------------------------+----------\n"));

    // 2) 데이터 행
    MYSQL_ROW row;
    int cnt = 0;
    while ((row = mysql_fetch_row(res))) {
        // disabled 컬럼: '0'이면 활성, else 비활성
        const char *status = (strcmp(row[5], "0") == 0) ? "활성" : "비활성";
        char line[256];
        snprintf(line, sizeof(line),
            "%-20s | %-10s | %-10s | %-15s | %-30s | %-8s\n",
            row[0], row[1], row[2], row[3], row[4], status);
        write(sock, line, strlen(line));
        cnt++;
    }
    mysql_free_result(res);

    // 3) 총합
    char footer[64];
    snprintf(footer, sizeof(footer), "총 회원 수: %d\n", cnt);
    write(sock, footer, strlen(footer));
}

// 2. 비밀번호 변경
void handle_change_admin_pw(const char* id, int sock) {
    char new_pw[100], hash[65];

    const char *msg = "[PW 변경] 새 비밀번호 입력: ";
    write(sock, msg, strlen(msg));
    usleep(300000);

    int len = read(sock, new_pw, sizeof(new_pw) - 1);
    if (len <= 0) {
        write(sock, "[오류] 입력 실패\n", strlen("[오류] 입력 실패\n"));
        return;
    }
    new_pw[strcspn(new_pw, "\r\n")] = 0;

    if (strlen(new_pw) == 0) {
        write(sock, "[오류] 빈 비밀번호 입력\n", strlen("[오류] 빈 비밀번호 입력\n"));
        return;
    }

    hash_password_sha256(new_pw, hash);

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        write(sock, "[오류] PW 변경 실패\n", strlen("[오류] PW 변경 실패\n"));
        return;
    }
    const char *sql = "UPDATE members SET pw=? WHERE id=?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        write(sock, "[오류] PW 변경 실패\n", strlen("[오류] PW 변경 실패\n"));
        return;
    }
    MYSQL_BIND bind[2] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = hash;     bind[0].buffer_length = strlen(hash);
    bind[1].buffer_type = MYSQL_TYPE_STRING; bind[1].buffer = (char*)id; bind[1].buffer_length = strlen(id);

    mysql_stmt_bind_param(stmt, bind);
    if (mysql_stmt_execute(stmt) == 0)
        write(sock, "[PW 변경 성공]\n", strlen("[PW 변경 성공]\n"));
    else
        write(sock, "[오류] PW 변경 실패\n", strlen("[오류] PW 변경 실패\n"));
    mysql_stmt_close(stmt);
}

// 3. 회원 복구 (disabled → 0)
void handle_restore_member(int sock) {
    char target_id[100];
    write(sock, "복구할 회원 ID: ", 21);
    int len = read(sock, target_id, sizeof(target_id)-1);
    if (len <= 0) return;
    target_id[strcspn(target_id, "\r\n")] = 0;

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) { write(sock, "[오류] 복구 실패\n", 22); return; }
    const char *sql = "UPDATE members SET disabled=0 WHERE id=?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt); write(sock, "[오류] 복구 실패\n", 22); return;
    }
    MYSQL_BIND bind[1] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = target_id; bind[0].buffer_length = strlen(target_id);

    mysql_stmt_bind_param(stmt, bind);
    if (mysql_stmt_execute(stmt) == 0)
        write(sock, "[복구 성공]\n", 16);
    else
        write(sock, "[오류] 복구 실패\n", 22);
    mysql_stmt_close(stmt);
}

// 4. 회원 완전 삭제
void handle_delete_member(int sock) {
    char target_id[100];
    write(sock, "삭제할 회원 ID: ", 21);
    int len = read(sock, target_id, sizeof(target_id)-1);
    if (len <= 0) return;
    target_id[strcspn(target_id, "\r\n")] = 0;

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) { write(sock, "[오류] 삭제 실패\n", 22); return; }
    const char *sql = "DELETE FROM members WHERE id=?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt); write(sock, "[오류] 삭제 실패\n", 22); return;
    }
    MYSQL_BIND bind[1] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = target_id; bind[0].buffer_length = strlen(target_id);

    mysql_stmt_bind_param(stmt, bind);
    if (mysql_stmt_execute(stmt) == 0)
        write(sock, "[삭제 성공]\n", 16);
    else
        write(sock, "[오류] 삭제 실패\n", 22);
    mysql_stmt_close(stmt);
}


// 관리자 메뉴 (handle_admin_menu)
void handle_admin_menu(const char* admin_id, int sock) {
    char buf[BUF_SIZE];


    // 관리자 권한 확인
    Member *admin = NULL;
    pthread_mutex_lock(&members_mutex);
    for (int i = 0; i < member_count; i++) {
        if (strcmp(members[i].id, admin_id) == 0) {
            admin = &members[i];
            break;
        }
    }
    pthread_mutex_unlock(&members_mutex);

    if (!admin || admin->is_admin == 0) {
        write(sock, "[오류] 관리자 권한 없음\n", strlen("[오류] 관리자 권한 없음\n"));
        return;  
    }  

    // 1) 메뉴 문자열을 한 번만 선언·출력
    const char *menu =
        "\n=== 관리자 메뉴 ===\n"
        "1. 전체 회원 조회\n"
        "2. 내 비밀번호 변경\n"
        "3. 회원 복구 (disabled=1 → 0)\n"
        "4. 회원 완전 삭제\n"
        "5. 사용자 정보 수정\n"
        "6. 종료\n";
    write(sock, menu, strlen(menu));

    // 2) 프롬프트-입력-처리 루프
    while (1) {
        write(sock, "> ", 2);
        if (read_line(sock, buf, sizeof(buf)) <= 0) return;

        // 빈 줄 또는 하트비트 명령은 무시
        if (buf[0]=='\0' || strncmp(buf, "/ping", 5)==0)
            continue;

        int cmd = atoi(buf);
        switch (cmd) {
        case 1:
            handle_view_all_members(sock);
            break;

        case 2: {  // 내 비밀번호 변경
            write(sock, "[PW 변경] 새 비밀번호: \n", strlen("[PW 변경] 새 비밀번호: \n"));
            if (read_line(sock, buf, sizeof(buf)) <= 0) return;
            buf[strcspn(buf, "\r\n")] = 0;

            if (!is_valid_password(buf)) {
                write(sock, "[오류] 비밀번호 정책 위반\n", strlen("[오류] 비밀번호 정책 위반\n"));
            }
            else if (db_admin_change_pw(admin_id, buf)) {
                write(sock, "[PW 변경] 완료\n", strlen("[PW 변경] 완료\n"));
            }
            else {
                write(sock, "[오류] DB 오류\n", strlen("[오류] DB 오류\n"));
            }
            break;
        }

        case 3: {  // 회원 복구
            write(sock, "복구할 사용자 ID: \n", strlen("복구할 사용자 ID: \n"));
            if (read_line(sock, buf, sizeof(buf)) <= 0) return;
            buf[strcspn(buf, "\r\n")] = 0;

            if (!db_id_exists(buf) || !db_is_disabled(buf)) {
                write(sock, "[오류] 복구 대상이 아닙니다.\n", strlen("[오류] 복구 대상이 아닙니다.\n"));
            }
            else {
                write(sock, "정말 복구하시겠습니까? (Y/N): \n", strlen("정말 복구하시겠습니까? (Y/N): \n"));
                char yn[4];
                if (read_line(sock, yn, sizeof(yn)) > 0 &&
                    (yn[0]=='Y'||yn[0]=='y')) {
                    // ← 호출 함수명 수정
                    if (db_set_disabled(buf, 0)) {
                        write(sock, "[복구 완료]\n", strlen("[복구 완료]\n"));
                    } else {
                        write(sock, "[오류] DB 오류\n", strlen("[오류] DB 오류\n"));
                    }
                } else {
                    write(sock, "[복구 취소]\n", strlen("[복구 취소]\n"));
                }
            }
            break;
        }

        case 4: {  // 회원 완전 삭제
            write(sock, "삭제할 사용자 ID: \n", strlen("삭제할 사용자 ID: \n"));
            if (read_line(sock, buf, sizeof(buf)) <= 0) return;
            buf[strcspn(buf, "\r\n")] = 0;

            if (!db_id_exists(buf)) {
                write(sock, "[오류] 존재하지 않는 ID\n", strlen("[오류] 존재하지 않는 ID\n"));
            }
            else {
                write(sock, "정말 삭제하시겠습니까? (Y/N): \n", strlen("정말 삭제하시겠습니까? (Y/N): \n"));
                char yn[4];
                if (read_line(sock, yn, sizeof(yn)) > 0 && (yn[0]=='Y'||yn[0]=='y')) {
                    // ← 호출 함수명 수정
                    if (db_purge_user(buf)) {
                        write(sock, "[삭제 완료]\n", strlen("[삭제 완료]\n"));
                    } else {
                        write(sock, "[오류] DB 오류\n", strlen("[오류] DB 오류\n"));
                    }
                } else {
                    write(sock, "[삭제 취소]\n", strlen("[삭제 취소]\n"));
                }
            }
            break;
        }

        case 5: {  // 사용자 정보 수정
            // (1) 대상 ID 입력
            char target_id[20];
            write(sock, "수정할 사용자 ID: \n", strlen("수정할 사용자 ID: \n"));
            if (read_line(sock, target_id, sizeof(target_id)) <= 0) return;
            target_id[strcspn(target_id,"\r\n")] = 0;

            if (!db_id_exists(target_id)) {
                write(sock, "[오류] 존재하지 않는 ID\n", strlen("[오류] 존재하지 않는 ID\n"));
                break;
            }

            // (2) 필드 선택
            write(sock,
                "\n=== 정보 수정 ===\n"
                "1. 비밀번호\n"
                "2. 전화번호\n"
                "3. 이메일\n"
                "4. 취소\n"
                "> ",
                strlen(
                "\n=== 정보 수정 ===\n"
                "1. 비밀번호\n"
                "2. 전화번호\n"
                "3. 이메일\n"
                "4. 취소\n"
                "> "));

            if (read_line(sock, buf, sizeof(buf)) <= 0) return;
            buf[strcspn(buf, "\r\n")] = 0;
            int fld = atoi(buf);
            const char *prompt;
            int (*validator)(const char*);
            switch (fld) {
            case 1:
                prompt = "[PW 수정] 새 비밀번호: \n";
                validator = is_valid_password;
                break;
            case 2:
                prompt = "[전화번호 수정] 새 전화번호: \n";
                validator = is_valid_phone;
                break;
            case 3:
                prompt = "[이메일 수정] 새 이메일: \n";
                validator = is_valid_email;
                break;
            default:
                write(sock, "[취소]\n", strlen("[취소]\n"));
                continue;
            }

            // (3) 새 값 입력 및 검증
            char new_value[100];
            if (!prompt_and_read_valid(sock, prompt, new_value, sizeof(new_value), validator, "[오류] 형식 오류\n")) {
                break;
            }
            new_value[strcspn(new_value,"\r\n")] = 0;

            // 이제 실제 수정 호출
            bool ok = false;
            if (fld == 1) {
                ok = db_admin_change_pw(target_id, new_value);
            } else if (fld == 2) {
                ok = db_admin_change_ph(target_id, new_value);
            } else if (fld == 3) {
                ok = db_admin_change_email(target_id, new_value);
            }
            fprintf(stderr, "[DEBUG] admin_modify: field=%d, id=%s, value=%s, ok=%d\n",
        fld, target_id, new_value, ok);
            const char *resp = ok ? "[수정 완료]\n> " : "[오류] DB 오류\n> ";
            write(sock, resp, strlen(resp));
            break;
        }
        case 6:
            write(sock, "[관리자 메뉴 종료]\n", strlen("[관리자 메뉴 종료]\n"));
            return;
        default:
            write(sock, "[오류] 잘못된 입력\n", strlen("[오류] 잘못된 입력\n"));
        }
        // 처리 후에도 메뉴는 다시 그리지 않고 '> ' 프롬프트로 돌아갑니다.
    }
}

// 사용자 정보 수정 핸들러
void handle_admin_modify_user(const char *admin_id, int sock, const char *args) {
    char target_id[20] = {0}, field[20] = {0}, new_value[100] = {0};
    int n = sscanf(args, "%19s %19s %99[^\n]", target_id, field, new_value);
    if (n != 3) {
        write(sock, "[오류] 사용법: /admin_modify <사용자ID> <필드(email|ph|pw)> <새값>\n", 55);
        return;
    }

    pthread_mutex_lock(&members_mutex);

    Member *admin = NULL;
    for (int i = 0; i < member_count; i++) {
        if (strcmp(members[i].id, admin_id) == 0) {
            admin = &members[i];
            break;
        }
    }
    if (!admin || !is_user_admin(admin_id)) {
        pthread_mutex_unlock(&members_mutex);
        write(sock, "[오류] 관리자 권한이 필요합니다.\n", 26);
        return;
    }

    Member *target = NULL;
    for (int i = 0; i < member_count; i++) {
        if (strcmp(members[i].id, target_id) == 0) {
            target = &members[i];
            break;
        }
    }
    if (!target) {
        pthread_mutex_unlock(&members_mutex);
        write(sock, "[오류] 대상 사용자를 찾을 수 없습니다.\n", 28);
        return;
    }

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        pthread_mutex_unlock(&members_mutex);
        write(sock, "[오류] DB 준비 실패\n", 18);
        return;
    }

    const char *sql = NULL;
    char hashed_pw[65] = {0};
    char *update_value = new_value;

    if (strcmp(field, "email") == 0) {
        if (!is_valid_email(new_value)) {
            pthread_mutex_unlock(&members_mutex);
            write(sock, "[오류] 이메일 형식 오류: @와 . 포함, 최소 5자 이상\n", 38);
            mysql_stmt_close(stmt);
            return;
        }
        strncpy(target->email, new_value, sizeof(target->email)-1);
        sql = "UPDATE members SET email=? WHERE id=?";
    }
    else if (strcmp(field, "ph") == 0 || strcmp(field, "phone") == 0) {
        if (!is_valid_phone(new_value)) {
            pthread_mutex_unlock(&members_mutex);
            write(sock, "[오류] 전화번호 형식 오류: 숫자만, 10~13자리\n", 33);
            mysql_stmt_close(stmt);
            return;
        }
        strncpy(target->ph, new_value, sizeof(target->ph)-1);
        sql = "UPDATE members SET ph=? WHERE id=?";
    }
    else if (strcmp(field, "pw") == 0 || strcmp(field, "password") == 0) {
        if (!is_valid_password(new_value)) {
            pthread_mutex_unlock(&members_mutex);
            write(sock, "[오류] 비밀번호 정책 위반: 8자 이상, 대/소/숫자/특수문자 필수\n", 47);
            mysql_stmt_close(stmt);
            return;
        }
        hash_password_sha256(new_value, hashed_pw);
        strncpy(target->pw, hashed_pw, sizeof(target->pw)-1);
        update_value = hashed_pw;
        sql = "UPDATE members SET pw=? WHERE id=?";
    }
    else {
        pthread_mutex_unlock(&members_mutex);
        write(sock, "[오류] 변경 가능한 필드는 email, ph(전화번호), pw(비밀번호)입니다.\n", 44);
        mysql_stmt_close(stmt);
        return;
    }

    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        pthread_mutex_unlock(&members_mutex);
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[DB 오류] 쿼리 준비 실패: %s\n", mysql_stmt_error(stmt));
        write(sock, err_msg, strlen(err_msg));
        mysql_stmt_close(stmt);
        return;
    }

    MYSQL_BIND bind[2] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = update_value;
    bind[0].buffer_length = strlen(update_value);
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = target_id;
    bind[1].buffer_length = strlen(target_id);

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        pthread_mutex_unlock(&members_mutex);
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[DB 오류] 바인딩 실패: %s\n", mysql_stmt_error(stmt));
        write(sock, err_msg, strlen(err_msg));
        mysql_stmt_close(stmt);
        return;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        pthread_mutex_unlock(&members_mutex);
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[DB 오류] 실행 실패: %s\n", mysql_stmt_error(stmt));
        write(sock, err_msg, strlen(err_msg));
        mysql_stmt_close(stmt);
        return;
    }

    mysql_stmt_close(stmt);
    pthread_mutex_unlock(&members_mutex);

    char success_msg[128];
    snprintf(success_msg, sizeof(success_msg), "[성공] %s의 %s이(가) 변경되었습니다.\n", target_id, field);
    write(sock, success_msg, strlen(success_msg));

    log_server(admin_id, "관리자 권한으로 사용자 정보 변경");
}






// 도착 쪽지 개수 조회 및 알림
void notify_offline_note_count(const char *id, int sock) {
    char query[256];
    snprintf(query, sizeof(query),
             "SELECT COUNT(*) FROM note_logs WHERE target_id='%s'", id);
    if (mysql_query(conn, query) != 0) return;

    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return;

    MYSQL_ROW row = mysql_fetch_row(res);
    int cnt = (row && row[0]) ? atoi(row[0]) : 0;
    mysql_free_result(res);

    if (cnt > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf),
                 "[알림] 도착한 쪽지: %d개(확인: /inbox) \n", cnt);
        write(sock, buf, strlen(buf));
    }
}

// 받은 쪽지 조회 및 삭제 (포맷 수정)
void handle_view_notes(const char *id, int sock) {
    MYSQL_RES *res;
    MYSQL_ROW row;
    char query[512], line[8192];

    // 1) 받은 쪽지 조회
    snprintf(query, sizeof(query),
             "SELECT sender_id, message, ts "
             "FROM note_logs "
             "WHERE target_id='%s' "
             "ORDER BY ts", id);
    if (mysql_query(conn, query) != 0) {
        write(sock, "[오류] 쪽지 조회 실패\n", strlen("[오류] 쪽지 조회 실패\n"));
        return;
    }
    res = mysql_store_result(conn);
    if (!res) {
        write(sock, "[오류] 쪽지 결과 저장 실패\n", strlen("[오류] 쪽지 결과 저장 실패\n"));
        return;
    }

    // 2) 헤더 전송
    write(sock,
          "[받은 쪽지 목록]\n"
          "보낸이 | 내용 | 시간\n",
          strlen("[받은 쪽지 목록]\n보낸이 | 내용 | 시간\n"));

    // 3) 각 쪽지 출력 (한 줄에 전부 표시)
    while ((row = mysql_fetch_row(res)) != NULL) {
        // row[0]=sender_id, row[1]=message, row[2]=ts
        // message에 포함된 개행 제거
        char msg_clean[ESC_MSG_MAX+1];
        strncpy(msg_clean, row[1], ESC_MSG_MAX);
        msg_clean[strcspn(msg_clean, "\r\n")] = '\0';
        snprintf(line, sizeof(line),
                 "%.200s | %.200s | %.20s\n",
                 row[0], msg_clean, row[2]);
        write(sock, line, strlen(line));
    }
    mysql_free_result(res);

    // 4) 조회된 쪽지 모두 삭제
    snprintf(query, sizeof(query),
             "DELETE FROM note_logs WHERE target_id='%s'", id);
    if (mysql_query(conn, query) != 0) {
        write(sock, "[오류] 쪽지 삭제 실패\n", strlen("[오류] 쪽지 삭제 실패\n"));
    } else {
        write(sock, "[알림] 조회된 쪽지를 모두 삭제했습니다.\n",
              strlen("[알림] 조회된 쪽지를 모두 삭제했습니다.\n"));
    }
}

// 1) 오프라인 쪽지 전용 핸들러
void handle_offline_note(const char *sender, const char *args) {
    char target[20], message[ESC_MSG_MAX + 1];
    const char *p = strchr(args, ' ');
    if (!p) return;

    size_t id_len = p - args;
    if (id_len > sizeof(target)-1) id_len = sizeof(target)-1;
    strncpy(target, args, id_len);
    target[id_len] = '\0';

    strncpy(message, p + 1, ESC_MSG_MAX);
    message[ESC_MSG_MAX] = '\0';

    // 대상 유효성 검사
    if (!db_id_exists(target)) {
        for (int i = 0; i < member_count; i++) {
            if (strcmp(members[i].id, sender) == 0) {
                write(members[i].sockfd,
                      "[오류] 상대 유저가 존재하지 않습니다.\n",
                      strlen("[오류] 상대 유저가 존재하지 않습니다.\n"));
                return;
            }
        }
    }

    // 날짜·시간 생성 (thread‑safe)
    time_t t = time(NULL);
    struct tm tm_buf;
    if (localtime_r(&t, &tm_buf) == NULL) memset(&tm_buf, 0, sizeof(tm_buf));
    char ts[20];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    // DB에 INSERT (오프라인 쪽지)
    if (!db_log_note(sender, target, message, ts)) {
        fprintf(stderr, "[DB_NOTE ERROR] note_logs insert failed\n");
    }

    // 발신자 안내
    for (int i = 0; i < member_count; i++) {
        if (strcmp(members[i].id, sender) == 0) {
            write(members[i].sockfd,
                  "[알림] 쪽지를 남겼습니다.\n",
                  strlen("[알림]쪽지를 남겼습니다.\n"));
            break;
        }
    }
}

// 2) 실시간 귓속말 전용 핸들러
void handle_whisper(const char *sender, const char *args) {
    char target[20], message[ESC_MSG_MAX + 1];
    const char *p = strchr(args, ' ');
    if (!p) return;

    // 대상 ID 분리
    size_t id_len = p - args;
    if (id_len > sizeof(target)-1) id_len = sizeof(target)-1;
    strncpy(target, args, id_len);
    target[id_len] = '\0';

    // 메시지 분리 후 개행 제거
    strncpy(message, p + 1, ESC_MSG_MAX);
    message[ESC_MSG_MAX] = '\0';
    message[strcspn(message, "\r\n")] = '\0';  // ← 개행 삭제

    // 날짜·시간 생성 (thread‑safe)
    time_t t = time(NULL);
    struct tm tm_buf;
    localtime_r(&t, &tm_buf);
    char ts[20];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    // 온라인 대상에게 전송
    int found = 0;
    for (int i = 0; i < member_count; i++) {
        if (strcmp(members[i].id, target) == 0) {
            char buf[ESC_MSG_MAX + 64];
            // 메시지와 타임스탬프를 한 줄에
            snprintf(buf, sizeof(buf),
                     "[귓속말 from %s] %s (%s)\n",
                     sender, message, ts);
            write(members[i].sockfd, buf, strlen(buf));
            found = 1;
            break;
        }
    }

    // 발신자 안내
    for (int i = 0; i < member_count; i++) {
        if (strcmp(members[i].id, sender) == 0) {
            if (found) {
                write(members[i].sockfd,
                      "[알림] 귓속말 전송 완료.\n",
                      strlen("[알림] 귓속말 전송 완료.\n"));
            } else {
                write(members[i].sockfd,
                      "[오류] 상대 유저가 오프라인입니다.\n",
                      strlen("[오류] 상대 유저가 오프라인입니다.\n"));
            }
            break;
        }
    }
}

void handle_modify_pw(const char *id, const char *new_pw, int sock) {
    if (!is_valid_password(new_pw)) {
        write(sock, "[오류] 비밀번호 정책 위반: 8자 이상, 대/소/숫자/특수문자 각각 1개 이상 포함 필수\n",
              strlen("[오류] 비밀번호 정책 위반: 8자 이상, 대/소/숫자/특수문자 각각 1개 이상 포함 필수\n"));
        return;
    }
    char hash[65];
    hash_password_sha256(new_pw, hash);
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        write(sock, "[오류] 비밀번호 변경 실패\n", strlen("[오류] 비밀번호 변경 실패\n"));
        return;
    }
    const char *sql = "UPDATE members SET pw=? WHERE id=?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        write(sock, "[오류] 비밀번호 변경 실패\n", strlen("[오류] 비밀번호 변경 실패\n"));
        return;
    }
    MYSQL_BIND bind[2] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char *)hash; bind[0].buffer_length = strlen(hash);
    bind[1].buffer_type = MYSQL_TYPE_STRING; bind[1].buffer = (char *)id;   bind[1].buffer_length = strlen(id);

    mysql_stmt_bind_param(stmt, bind);
    if (mysql_stmt_execute(stmt) == 0)
        write(sock, "[비밀번호 변경 완료]\n", strlen("[비밀번호 변경 완료]\n"));
    else
        write(sock, "[오류] 비밀번호 변경 실패\n", strlen("[오류] 비밀번호 변경 실패\n"));
    mysql_stmt_close(stmt);
}



void handle_modify_phone(const char *id, const char *new_phone, int sock) {
    // 1. 입력값 유효성 검사
    if (!is_valid_phone(new_phone)) {
        write(sock,
              "[오류] 전화번호 형식 오류: 숫자만 입력, 10~13자리 필수\n",
              strlen("[오류] 전화번호 형식 오류: 숫자만 입력, 10~13자리 필수\n"));
        return;
    }
    // 2. DB 변경 처리
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        write(sock, "[오류] 전화번호 변경 실패(DB 준비)\n", strlen("[오류] 전화번호 변경 실패(DB 준비)\n"));
        return;
    }
    const char *sql = "UPDATE members SET ph=? WHERE id=?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        write(sock, "[오류] 전화번호 변경 실패(쿼리 준비)\n", strlen("[오류] 전화번호 변경 실패(쿼리 준비)\n"));
        return;
    }
    MYSQL_BIND bind[2] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char *)new_phone;
    bind[0].buffer_length = strlen(new_phone);
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char *)id;
    bind[1].buffer_length = strlen(id);

    mysql_stmt_bind_param(stmt, bind);
    if (mysql_stmt_execute(stmt) == 0)
        write(sock, "[전화번호 변경 완료]\n", strlen("[전화번호 변경 완료]\n"));
    else
        write(sock, "[오류] 전화번호 변경 실패(DB 처리)\n", strlen("[오류] 전화번호 변경 실패(DB 처리)\n"));
    mysql_stmt_close(stmt);
}

void handle_modify_email(const char *id, const char *new_email, int sock) {
    // 1. 입력값 유효성 검사
    if (!is_valid_email(new_email)) {
        write(sock,
              "[오류] 이메일 형식 오류: @와 . 포함, 최소 5자 이상\n",
              strlen("[오류] 이메일 형식 오류: @와 . 포함, 최소 5자 이상\n"));
        return;
    }
    // 2. DB 변경 처리
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        write(sock, "[오류] 이메일 변경 실패(DB 준비)\n", strlen("[오류] 이메일 변경 실패(DB 준비)\n"));
        return;
    }
    const char *sql = "UPDATE members SET email=? WHERE id=?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        write(sock, "[오류] 이메일 변경 실패(쿼리 준비)\n", strlen("[오류] 이메일 변경 실패(쿼리 준비)\n"));
        return;
    }
    MYSQL_BIND bind[2] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char *)new_email;
    bind[0].buffer_length = strlen(new_email);
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char *)id;
    bind[1].buffer_length = strlen(id);

    mysql_stmt_bind_param(stmt, bind);
    if (mysql_stmt_execute(stmt) == 0)
        write(sock, "[이메일 변경 완료]\n", strlen("[이메일 변경 완료]\n"));
    else
        write(sock, "[오류] 이메일 변경 실패(DB 처리)\n", strlen("[오류] 이메일 변경 실패(DB 처리)\n"));
    mysql_stmt_close(stmt);
}
