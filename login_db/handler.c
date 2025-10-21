// handler.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mysql/mysql.h>
#include <pthread.h>
#include <time.h>
#include "password_utils.h"
#include "chat_util.h"
#include "room.h"
#include "handler.h"
#include "db.h"
#include "member.h"
#include "global.h"
#include "account.h"
#include <arpa/inet.h>
#include <sys/types.h>


// extern으로 서버 쪽 접속 수/뮤텍스 참조
extern int  connection_count;
extern pthread_mutex_t conn_count_mutex;

// client_info 재정의 (server.c와 동일하게)
typedef struct {
    int  sock;
    char addr_str[32];
} client_info;

// static char shared_new_id[20];
// static char shared_newpw[100];

extern Member members[MAX_CLIENTS];
extern int member_count;
extern MYSQL *conn;

// in‑memory 배열 보호용 mutex
extern pthread_mutex_t members_mutex;

int logout_requested = 0;

// 관리자 권한 확인 함수
int is_user_admin(const char* id) {
    int result = 0;
    pthread_mutex_lock(&members_mutex);
    for (int i = 0; i < member_count; i++) {
        if (strcmp(members[i].id, id) == 0) {
            fprintf(stderr, "[DEBUG] Checking admin status of '%s': is_admin=%d\n", id, members[i].is_admin);
            if (members[i].is_admin == 1) {
                result = 1;
                break;
            }
        }
    }
    pthread_mutex_unlock(&members_mutex);
    return result;
}


// 주요 입력 유효성 검사 루프
int prompt_and_read_valid(
    int sock, const char *prompt, char *out, int outsz,
    int (*validator)(const char *), const char *errmsg)
{
    while (1) {
        write(sock, prompt, strlen(prompt));
        if (read_line(sock, out, outsz) <= 0) return 0;
        if (!validator || validator(out)) break;
        write(sock, errmsg, strlen(errmsg));
    }
    return 1;
}


// ─── 자동 로그아웃 정리 스레드 ───────────────────────────────

void *clean_inactive_users(void *arg) {
    while (1) {
        sleep(60);
        mysql_query(conn,
            "DELETE FROM active_users "
            "WHERE TIMESTAMPDIFF(SECOND,last_ping_at,NOW())>90"
        );
        pthread_mutex_lock(&rooms_mutex);
        pthread_mutex_lock(&members_mutex);
        for (int i = 0; i < member_count; i++) {
            char q[256];
            snprintf(q, sizeof(q),
                     "SELECT COUNT(*) FROM active_users WHERE user_id='%s'",
                     members[i].id);
            if (mysql_query(conn, q)==0) {
                MYSQL_RES *r = mysql_store_result(conn);
                if (r) {
                    MYSQL_ROW row = mysql_fetch_row(r);
                    int cnt = row ? atoi(row[0]) : 0;
                    mysql_free_result(r);
                    if (cnt==0) {
                        handle_leave_room(members[i].id, members[i].sockfd);
                        close(members[i].sockfd);
                        for (int j=i; j<member_count-1; j++)
                            members[j]=members[j+1];
                        member_count--; i--;
                    }
                }
            }
        }
        pthread_mutex_unlock(&members_mutex);
        pthread_mutex_unlock(&rooms_mutex);
    }
    return NULL;
}






// ─── 클라이언트 핸들러 ───────────────────────────────────────
// 클라이언트 핸들러 (로그인/회원가입/채팅 등 메인 루프)
void *client_handler(void *arg) {
    client_info *ci = (client_info *)arg;
    int sock = ci->sock;
    char addr[32];
    strncpy(addr, ci->addr_str, sizeof(addr));
    free(ci);

    char msg[BUF_SIZE];
    char id[20], pw[100];
    char sql[256];
    // int logout_requested = 0;
    // int decremented = 0;    // ← 카운트 감소 한 번만 시도했는지 체크
    int decremented      = 0;  // connection_count 감소 여부
    int logged_in        = 0;  // active_users / members[] 등록 여부

    // 로그인/가입 루프
    while (1) {
    LOGIN_RETRY:
        while (1) {
            // send_clear_screen(sock);
            // 1. ID 입력
            write(sock, "ID 입력: \n", strlen("ID 입력: \n"));

            if (read_line(sock, id, sizeof(id)) <= 0) {
                // 로그인 도중 강제 종료
                printf("[강제종료] %s 로그인 중 연결 끊김\n", addr);
                fflush(stdout);
                goto CLEANUP;
            }

        // ── 입력값 앞뒤 공백 제거 ──
        // (read_line()은 '\n','\r'만 제거하므로, 추가로 스페이스 트리밍)
        {
            // 앞쪽
            char *p = id;
            while (*p == ' ') p++;
            if (p != id) memmove(id, p, strlen(p)+1);
            // 뒤쪽
            size_t L = strlen(id);
            while (L > 0 && id[L-1] == ' ') id[--L] = '\0';
        }

        // ── 디버그: 실제 받은 ID 값 확인 ──
        fprintf(stderr, "[DEBUG] Received ID='%s' (len=%zu)\n", id, strlen(id));

            // 로그인 전 ping 무시
            if (strncmp(id, "/ping", 5) == 0) {
                continue;
            }
            // 2. 계정 존재 여부 판단
            if (!db_id_exists(id)) {
                send_clear_screen(sock);
                // 신규 계정: 회원가입 플로우 (처음 입력된 id 사용)
                write(sock, "[신규 사용자] 회원가입 하시겠습니까? (Y/N): \n",
                      strlen("[신규 사용자] 회원가입 하시겠습니까? (Y/N): \n"));
                char yn[4];
                while (read_line(sock, yn, sizeof(yn)) > 0) {
                    if (yn[0] == 'Y' || yn[0] == 'y') break;
                    if (yn[0] == 'N' || yn[0] == 'n') goto LOGIN_RETRY;
                    write(sock, "Y 또는 N으로 다시 입력해주세요: \n",
                          strlen("Y 또는 N으로 다시 입력해주세요: \n"));
                }

                // 비밀번호, 이름, 성, 전화, 이메일 모두 정책 검증 후 입력
                char newpw[100], fname[50], lname[50], phone[32], email[100];
                if (!prompt_and_read_valid(sock, "[안내] 비밀번호는 8자 이상, 대/소/숫자/특수문자 1개 이상 포함 필요\n비밀번호 입력: \n",
                                          newpw, sizeof(newpw), is_valid_password,
                                          "[오류] 비밀번호 정책 위반: 다시 입력해주세요.\n")) return NULL;
                if (!prompt_and_read_valid(sock, "이름 입력: \n", fname, sizeof(fname), NULL, "")) return NULL;
                if (!prompt_and_read_valid(sock, "성 입력: \n", lname, sizeof(lname), NULL, "")) return NULL;
                if (!prompt_and_read_valid(sock, "전화번호 입력(숫자만): \n", phone, sizeof(phone), is_valid_phone,
                                          "[오류] 전화번호 형식 오류(숫자, 10~13자리)\n")) return NULL;
                if (!prompt_and_read_valid(sock, "이메일 입력: \n", email, sizeof(email), is_valid_email,
                                          "[오류] 이메일 형식 오류(@와 . 포함, 최소 5자)\n")) return NULL;
                strcpy(pw, newpw);

                // DB 등록 (비밀번호 해시 포함)
                if (!db_register_full(id, pw, fname, lname, phone, email)) {
                    write(sock, "[오류] 회원가입 실패. 다시 시도해주세요.\n",
                          strlen("[오류] 회원가입 실패. 다시 시도해주세요.\n"));
                    sleep(60);
                    goto LOGIN_RETRY;
                }
                // ─── 신규가입 직후, active_users 에도 바로 등록 ─────────────────
                {
                    char sql_act[256];
                    snprintf(sql_act, sizeof(sql_act),
                        "INSERT INTO active_users (user_id, login_at, last_ping_at) "
                        "VALUES ('%s', NOW(), NOW()) "
                        "ON DUPLICATE KEY UPDATE login_at = NOW(), last_ping_at = NOW()",
                        id);
                    if (mysql_query(conn, sql_act) != 0) {
                        fprintf(stderr, "[DB] active_users INSERT/UPDATE failed: %s\n",
                                mysql_error(conn));
                    }
                }
                write(sock, "[가입 성공] 로그인 중...\n",
                      strlen("[가입 성공] 로그인 중...\n"));
                sleep(1);
                break;
            } else {
                // 기존 계정: PW 입력 → 로그인 시도
                write(sock, "PW 입력: \n", strlen("PW 입력: \n"));
                if (read_line(sock, pw, sizeof(pw)) <= 0) {
                    goto CLEANUP;
                }
                // 로그인 전 ping 무시
                if (strncmp(pw, "/ping", 5) == 0) {
                    continue;
                }                
                if (!db_login(id, pw)) {
                    write(sock,
                          "[오류] 비밀번호가 틀렸습니다. 다시 시도해주세요.\n",
                          strlen("[오류] 비밀번호가 틀렸습니다. 다시 시도해주세요.\n"));
                    sleep(1);
                    continue; // 로그인 화면으로 복귀
                }
                // 중복 접속 확인
                pthread_mutex_lock(&members_mutex);
                int already = 0;
                for (int k = 0; k < member_count; k++) {
                    if (strcmp(members[k].id, id) == 0) { already = 1; break; }
                }
                pthread_mutex_unlock(&members_mutex);
                if (already) {
                    write(sock, "[오류] 해당 ID는 이미 접속 중입니다.\n",
                          strlen("[오류] 해당 ID는 이미 접속 중입니다.\n"));
                    sleep(1);
                    continue;
                }
                // active_users 등록
                char sql[128];
                snprintf(sql, sizeof(sql),
                         "INSERT INTO active_users (user_id, login_at) "
                         "VALUES ('%s', NOW()) "
                         "ON DUPLICATE KEY UPDATE login_at = NOW()",
                         id);
                mysql_query(conn, sql);
                logged_in = 1;
                break;
            }
        }

        // 로그인 성공 후 session 등록
        pthread_mutex_lock(&members_mutex);
        Member *m = &members[member_count++];
        db_get_member_info(id, m);
        m->sockfd = sock;
        m->in_room = 0;
        strcpy(m->current_room, "lobby");
        pthread_mutex_unlock(&members_mutex);

        printf("[로그인] 사용자 '%s' 로그인. 접속자: %d\n", id, member_count);
        fflush(stdout);

        send_clear_screen(sock);
        send_lobby_welcome(sock);
        notify_offline_note_count(id, sock);
        log_server(id, "로그인 완료");

        // ─── 채팅 세션 (내부 루프) ───────────────────────────
        while (1) {
            memset(msg, 0, sizeof(msg));
            int len = read(sock, msg, sizeof(msg)-1);
            if (len <= 0) {
                // 클라이언트가 강제 종료된 경우
                printf("[강제종료] %s 사용자 '%s' 연결 끊김 (len=%d)\n", 
                       addr, id, len);
                fflush(stdout);
                goto CLEANUP;
            }
            // 로그인 이후 채팅/명령 루프로 진입 전 ping 무시
            if (strncmp(msg, "/ping", 5) == 0) {
                char sql[128];
                snprintf(sql, sizeof(sql),
                         "UPDATE active_users SET last_ping_at=NOW() WHERE user_id='%s'", id);
                mysql_query(conn, sql);
                continue;
            }
            // 명령어 처리
            if (strncmp(msg, "/help", 5) == 0) {
                char *help_msg =
                "사용 가능한 명령어:\n"
                "/create 방이름 [public|private] [비밀번호]  - 새 채팅방 생성\n"
                "/join 방번호 [비밀번호]                     - 채팅방 입장\n"
                "/leave                                     - 채팅방 퇴장 및 로비 복귀\n"
                "/roomlist                                  - 전체 방 목록 조회\n"
                "/status                                    - 현재 방 참여자 목록 조회 (방 안에서만)\n"
                "/allusers                                  - 전체 접속자 목록 조회\n"
                "/note 대상ID 메시지                        - 오프라인 쪽지 남기기\n"
                "/w 대상ID 메시지                           - 실시간 귓속말\n"
                "/inbox                                     - 받은 쪽지 확인\n"
                "/kick 대상ID                               - 방장에서 상대 강퇴\n"
                "/owner 대상ID                              - 방장 권한 위임\n"
                "/modify pw <새비밀번호>                        - 비밀번호 수정\n"
                "/modify phone <새전화번호>                    - 전화번호 수정\n"
                "/modify email <새이메일>                      - 이메일 수정\n"
                "/delete                                    - 계정 탈퇴(비활성화)\n"
                "/admin                                     - 관리자 메뉴 진입\n"
                // "/admin_modify <대상ID> <field> <new_value> - 관리자 유저정보 수정\n"
                "/logout                                    - 로그인 화면으로 이동\n"
                ;
                write(sock, help_msg, strlen(help_msg));
            } else if (strncmp(msg, "/create", 7) == 0) {
                handle_create_room(id, sock, msg + 8);
            } else if (strncmp(msg, "/join", 5) == 0) {
                handle_join_room(id, sock, msg + 6);
            } else if (strncmp(msg, "/leave", 6) == 0) {
                handle_leave_room(id, sock);
            } else if (strncmp(msg, "/roomlist", 9) == 0) {
                handle_roomlist(sock);
            }else if (strncmp(msg, "/allusers", 9) == 0) {
                show_online_users(sock);
            } else if (strncmp(msg, "/status", 7) == 0) {
                // (1) 내 member 객체 찾기
                Member *caller = NULL;
                for (int i = 0; i < member_count; ++i) {
                    if (members[i].sockfd == sock) {
                        caller = &members[i];
                        break;
                    }
                }
                // (2) 방 안에 있으면 방 참여자 목록 출력, 아니면 오류 안내
                if (!caller || caller->in_room == 0) {
                    write(sock,
                        "[오류] 방에 입장 중일 때만 확인 가능합니다.\n",
                        strlen("[오류] 방에 입장 중일 때만 확인 가능합니다.\n"));
                } else {
                    show_room_members(sock, caller->current_room);
                }
            } else if (strncmp(msg, "/inbox", 6) == 0) {
                handle_view_notes(id, sock);
            } else if (strncmp(msg, "/note", 5) == 0) {
                // 오프라인 쪽지
                handle_offline_note(id, msg + 6);
            }else if (strncmp(msg, "/w", 2) == 0) {
                char target[20], message[ESC_MSG_MAX+1];
                const char *p = strchr(msg+3, ' ');
                if (!p) {
                    write(sock, "[오류] 대상ID와 메시지를 모두 입력하세요.\n", strlen("[오류] 대상ID와 메시지를 모두 입력하세요.\n"));
                    continue;
                }
                size_t id_len = p - (msg+3);
                if (id_len > sizeof(target)-1) id_len = sizeof(target)-1;
                strncpy(target, msg+3, id_len); target[id_len] = '\0';
                strncpy(message, p+1, ESC_MSG_MAX); message[ESC_MSG_MAX]='\0';
                message[strcspn(message, "\r\n")] = '\0';

                int res = handle_whisper_or_note(id, target, message, 0);
                if (res == 1) {
                    write(sock, "[알림] 귓속말 전송 완료.\n", strlen("[알림] 귓속말 전송 완료.\n"));
                } else if (res == 0 || res == 2) {
                    // 오프라인인 경우 쪽지 남기겠냐고 물음
                    write(sock, "[오프라인] 쪽지로 남기겠습니까? (Y/N): ", strlen("[오프라인] 쪽지로 남기겠습니까? (Y/N): "));
                    char yn[8] = {0};
                    int n = read_line(sock, yn, sizeof(yn));
                    if (n > 0 && (yn[0]=='Y'||yn[0]=='y')) {
                        int note_res = handle_whisper_or_note(id, target, message, 1);
                        if (note_res == 0) write(sock, "[알림] 쪽지를 남겼습니다.\n", strlen("[알림] 쪽지를 남겼습니다.\n"));
                        else write(sock, "[오류] 쪽지 저장 실패.\n", strlen("[오류] 쪽지 저장 실패.\n"));
                    } else {
                        write(sock, "[알림] 쪽지 남기기 취소됨.\n", strlen("[알림] 쪽지 남기기 취소됨.\n"));
                    }
                } else if (res == -1) {
                    write(sock, "[오류] 대상 유저가 존재하지 않습니다.\n", strlen("[오류] 대상 유저가 존재하지 않습니다.\n"));
                }

            } else if (strncmp(msg, "/kick", 5) == 0) {
                // 방장 권한으로 강퇴 명령
                char target[20];
                sscanf(msg + 6, "%s", target);

                // 1) 확인 요청
                char confirm_prompt[128];
                snprintf(confirm_prompt, sizeof(confirm_prompt),
                        "정말 '%s' 을(를) 강퇴하시겠습니까? (Y/N): ", target);
                write(sock, confirm_prompt, strlen(confirm_prompt));

                // 2) 응답 읽기
                char yn[4];
                if (read_line(sock, yn, sizeof(yn)) > 0 && (yn[0]=='Y' || yn[0]=='y')) {
                    // 3) 실제 강퇴 실행
                    handle_kick(id, target);
                } else {
                    // 취소 안내
                    write(sock,
                        "[알림] 강퇴가 취소되었습니다.\n",
                        strlen("[알림] 강퇴가 취소되었습니다.\n"));
                }
            } else if (strncmp(msg, "/owner", 6) == 0) {
                handle_owner(id, msg + 7);
            } else if (strncmp(msg, "/modify", 7) == 0) {
                // msg: "/modify ..." 이후 부분을 파싱
                char subcmd[16], param[100];
                int n = sscanf(msg + 8, "%15s %99[^\n]", subcmd, param);

                if (n != 2) {
                    write(sock,
                        "[오류] 사용법: /modify [pw|phone|email] <새값>\n",
                        strlen("[오류] 사용법: /modify [pw|phone|email] <새값>\n"));
                } else {
                    if (strcmp(subcmd, "pw") == 0) {
                        handle_modify_pw(id, param, sock);
                    } else if (strcmp(subcmd, "phone") == 0) {
                        handle_modify_phone(id, param, sock);
                    } else if (strcmp(subcmd, "email") == 0) {
                        handle_modify_email(id, param, sock);
                    } else {
                        write(sock,
                            "\n[오류] 사용법: /modify [pw|phone|email] <새값>\n",
                            strlen("\n[오류] 사용법: /modify [pw|phone|email] <새값>\n"));
                    }
                }
    
            } else if (strncmp(msg, "/delete", 7) == 0) {
                char yn[4];

                // 1) 탈퇴 확인
                write(sock, "정말 탈퇴하시겠습니까? (Y/N): ", strlen("정말 탈퇴하시겠습니까? (Y/N): "));
                if (read_line(sock, yn, sizeof(yn)) <= 0) {
                    write(sock, "[탈퇴 취소]\n", strlen("[탈퇴 취소]\n"));
                    // 로그인 화면으로 복귀
                    goto LOGIN_RETRY;
                }
                // 간혹 /ping 이 들어올 수 있으니 무시
                if (strncmp(yn, "/ping", 5) == 0) {
                    continue;
                }
                // 대소문자 구분 없이 확인
                if (yn[0]=='Y' || yn[0]=='y') {
                    // 2) DB 탈퇴 처리 + 세션 정리
                    if (db_delete(id)) {
                        write(sock, "[탈퇴 완료] 계정이 비활성화 되었습니다.\n",
                              strlen("[탈퇴 완료] 계정이 비활성화 되었습니다.\n"));
                        // active_users 테이블에서 제거
                        {
                            char sql_act[128];
                            snprintf(sql_act, sizeof(sql_act),
                                     "DELETE FROM active_users WHERE user_id='%s'",
                                     id);
                            mysql_query(conn, sql_act);
                        }
                        // in-memory members[]에서도 제거
                        pthread_mutex_lock(&members_mutex);
                        for (int i = 0; i < member_count; ++i) {
                            if (strcmp(members[i].id, id) == 0) {
                                for (int j = i; j < member_count-1; ++j)
                                    members[j] = members[j+1];
                                --member_count;
                                break;
                            }
                        }
                        pthread_mutex_unlock(&members_mutex);

                        // 3) 로그아웃: 로그인 화면으로
                        logged_in = 0;
                        printf("[탈퇴] 사용자 '%s' 탈퇴 완료. 현재 접속자: %d\n", id, member_count);
                        fflush(stdout);
                        decremented = 1; // CLEANUP 에서 connection_count 감소 안 함
                        close(sock);
                        sleep(1);
                        goto LOGIN_RETRY;
                    } else {
                        write(sock, "[오류] 탈퇴 처리 실패\n", strlen("[오류] 탈퇴 처리 실패\n"));
                        continue;
                    }
                } else {
                    // N 또는 다른 키면 취소
                    write(sock, "[탈퇴 취소]\n", strlen("[탈퇴 취소]\n"));
                    // 로그인 화면으로
                    goto LOGIN_RETRY;
                }
            // } else if (strncmp(msg, "/ping", 5) == 0) {
            //     char sql[128];
            //     snprintf(sql, sizeof(sql),
            //             "UPDATE active_users SET last_ping_at=NOW() WHERE user_id='%s'", id);
            //     mysql_query(conn, sql);
            //     continue;
            } else if (strncmp(msg, "/admin_modify", 13) == 0) {
                handle_admin_modify_user(id, sock, msg + 14);
            } else if (strncmp(msg, "/admin", 6) == 0) {
                if (!is_user_admin(id)) {
                    write(sock, "[오류] 관리자 권한이 필요합니다.\n", strlen("[오류] 관리자 권한이 필요합니다.\n"));
                    continue;
                }
                handle_admin_menu(id, sock);

            // 1) 사용자에게 로그아웃 안내
            } else if (strncmp(msg, "/logout", 7) == 0) {
                // 1) 사용자에게 안내
                write(sock,
                      "[로그아웃] 로그인 화면으로 돌아갑니다.\n",
                      strlen("[로그아웃] 로그인 화면으로 돌아갑니다.\n"));
                
                // 2) 서버 로그에 찍기
                printf("[로그아웃] 사용자 '%s' 로그아웃\n", id);
                fflush(stdout);

                // 3) DB/메모리 세션 정리
                snprintf(sql, sizeof(sql),
                         "DELETE FROM active_users WHERE user_id='%s'", id);
                mysql_query(conn, sql);
                pthread_mutex_lock(&members_mutex);
                for (int i = 0; i < member_count; i++) {
                    if (strcmp(members[i].id, id) == 0) {
                        if (members[i].in_room) handle_leave_room(id, sock);
                        for (int j = i; j < member_count-1; j++)
                            members[j] = members[j+1];
                        member_count--;
                        break;
                    }
                }
                pthread_mutex_unlock(&members_mutex);
                // 3) 로그인/가입 루프로 돌아가기
                logged_in = 0;
                sleep(1);
                goto LOGIN_RETRY;
            // 여기에 unrecognized command 처리 추가
            }else if (msg[0] == '/') {
                // 정의된 명령어가 아님
                write(sock, "[오류] 알 수 없는 명령어입니다. /help 를 참조하세요.\n",
                    strlen("[오류] 알 수 없는 명령어입니다. /help 를 참조하세요.\n"));
            }
            else {
                // 진짜 채팅 메시지인 경우에만 브로드캐스트
                broadcast_from(id, msg);
            }
        }
    }    
        // ────────────────────────────────────────────────────────

        // ─── CLEANUP ───────────────────────────────────────

CLEANUP:
    // 실제 연결 끊김 시 한 번만 실행
    if (logged_in) {
        // 방 나가기 + active_users 삭제 + members[] 정리
        pthread_mutex_lock(&members_mutex);
        for (int i = 0; i < member_count; i++) {
            if (members[i].sockfd == sock) {
                if (members[i].in_room)
                    handle_leave_room(members[i].id, sock);
                snprintf(sql, sizeof(sql),
                         "DELETE FROM active_users WHERE user_id='%s'",
                         members[i].id);
                mysql_query(conn, sql);
                for (int j = i; j < member_count-1; j++)
                    members[j] = members[j+1];
                member_count--;
                break;
            }
        }
        pthread_mutex_unlock(&members_mutex);
    }

    // connection_count 는 실제 소켓 close 시 한 번만 감소
    if (!decremented) {
        pthread_mutex_lock(&conn_count_mutex);
        connection_count--;
        pthread_mutex_unlock(&conn_count_mutex);
    }
    printf("[접속종료] %s 현재 서버접속: %d\n", addr, connection_count);
    fflush(stdout);
    close(sock);
    return NULL;
}
