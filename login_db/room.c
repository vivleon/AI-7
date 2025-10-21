// [room.c]
#include "room.h"
#include "chat_util.h"
#include "member.h"
#include "global.h"      // members[], member_count 사용
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mysql/mysql.h>
#include "db.h"          // extern MYSQL *conn;
#include <time.h>
#include "account.h"     // notify_offline_note_count 선언


#define MAX_ROOM   10
#define MAX_MEMBER 100
#define SQL_BUF_SIZE 4096

typedef struct {
    int  in_use;
    int  room_id;
    char name[50];
    char owner[20];
    int  is_public;
    char password[20];
    char members[MAX_MEMBER][20];
    int  member_count;
} ChatRoom;

// typedef ChatRoom Room;

// Room rooms[MAX_ROOM];
static ChatRoom rooms[MAX_ROOM];
extern Member members[MAX_CLIENTS];
extern int    member_count;
extern MYSQL *conn;      // 전역 DB 연결 객체

// // DB 기준 room_id 를 in-memory 인덱스로 매핑해 주는 함수
// static int find_room_index_by_db_id(int db_id) {
//     for (int i = 0; i < MAX_ROOM; i++) {
//         if (rooms[i].in_use && rooms[i].room_id == db_id) {
//             return i;
//         }
//     }
//     return -1;
// }

static int find_room_index_by_display(int display_idx) {
    int cnt = 0;
    for (int i = 0; i < MAX_ROOM; ++i) {
        if (!rooms[i].in_use) continue;
        if (cnt == display_idx) return i;
        cnt++;
    }
    return -1;
}


void init_rooms() {
    for (int i = 0; i < MAX_ROOM; i++) {
        rooms[i].in_use = 0;
        rooms[i].room_id = i;
        rooms[i].member_count = 0;
    }
}



// 방 생성
void handle_create_room(const char *id, int sock, const char *args) {
    pthread_mutex_lock(&rooms_mutex);
    char name[50] = "", visibility[10] = "", pw[20] = "";
    int n = sscanf(args, "%49s %9s %19s", name, visibility, pw);

    int is_public = 1;

    if (n >= 2) {
        if (strcmp(visibility, "private") == 0)
            is_public = 0;
        else if (strcmp(visibility, "public") == 0)
            is_public = 1;
    }

    for (int i = 0; i < MAX_ROOM; i++) {
        if (!rooms[i].in_use) {
            rooms[i].in_use = 1;
            rooms[i].room_id = i;

            strncpy(rooms[i].name, name, sizeof(rooms[i].name) - 1);
            rooms[i].name[sizeof(rooms[i].name) - 1] = '\0';

            strncpy(rooms[i].owner, id, sizeof(rooms[i].owner) - 1);
            rooms[i].owner[sizeof(rooms[i].owner) - 1] = '\0';

            rooms[i].is_public = is_public;

            strncpy(rooms[i].password, pw, sizeof(rooms[i].password) - 1);
            rooms[i].password[sizeof(rooms[i].password) - 1] = '\0';

            // members 배열 첫번째 멤버는 방장 id 복사
            strncpy(rooms[i].members[0], id, sizeof(rooms[i].members[0]) - 1);
            rooms[i].members[0][sizeof(rooms[i].members[0]) - 1] = '\0';

            rooms[i].member_count = 1;

            // 2) DB에 rooms INSERT (AUTO_INCREMENT)
            {
                time_t t = time(NULL);
                struct tm tm_buf;
                localtime_r(&t, &tm_buf);
                char dtbuf[20];
                strftime(dtbuf, sizeof(dtbuf), "%Y-%m-%d %H:%M:%S", &tm_buf);

                char sql[SQL_BUF_SIZE];
                snprintf(sql, sizeof(sql),
                    "INSERT INTO rooms "
                    "(name, owner, is_public, password, created_at) "
                    "VALUES ('%s','%s',%d,'%s','%s')",
                    name, id, rooms[i].is_public, pw, dtbuf);
                if (mysql_query(conn, sql)) {
                    fprintf(stderr, "[DB_ROOM CREATE ERROR] %s\n", mysql_error(conn));
                } else {
                    rooms[i].room_id = mysql_insert_id(conn);
                }

                snprintf(sql, sizeof(sql),
                         "INSERT INTO room_members (room_id, user_id) "
                         "VALUES (%d,'%s')",
                         rooms[i].room_id, id);
                if (mysql_query(conn, sql)) {
                    fprintf(stderr, "[DB_ROOMMEM INSERT ERROR] %s\n", mysql_error(conn));
                }
            }

            int display_idx = 0;
            for (int j = 0; j < i; ++j) {
                if (rooms[j].in_use) display_idx++;
            }

            char msg[100];
            snprintf(msg, sizeof(msg), "[채팅방 생성 완료] 방번호: %d\n", display_idx);
            send_clear_screen(sock);
            write(sock, msg, strlen(msg));

            char server_evt[128];
            snprintf(server_evt, sizeof(server_evt), "[룸생성] %s → %s (public=%d)\n",
                     id, rooms[i].name, rooms[i].is_public);
            log_server(id, server_evt);

            for (int idx = 0; idx < member_count; idx++) {
                if (strcmp(members[idx].id, id) == 0) {
                    strncpy(members[idx].current_room, rooms[i].name, sizeof(members[idx].current_room) - 1);
                    members[idx].current_room[sizeof(members[idx].current_room) - 1] = '\0';
                    members[idx].in_room = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&rooms_mutex);
            return;
        }
    }

    write(sock, "[오류] 채팅방 생성 실패: 최대 수 초과\n", strlen("[오류] 채팅방 생성 실패: 최대 수 초과\n"));
    pthread_mutex_unlock(&rooms_mutex);
}


// 방 입장
void handle_join_room(const char *id, int sock, const char *args) {
    pthread_mutex_lock(&rooms_mutex);
    Member *caller = NULL;
    for (int i = 0; i < member_count; ++i) {
        if (strcmp(members[i].id, id) == 0) {
            caller = &members[i];
            break;
        }
    }
    if (caller && caller->in_room == 1) {
        write(sock, "[오류] 다른 방에 이미 입장 중입니다. 로비에서만 이동 가능합니다.\n",
              strlen("[오류] 다른 방에 이미 입장 중입니다. 로비에서만 이동 가능합니다.\n"));
        pthread_mutex_unlock(&rooms_mutex);
        return;
    }

    int display_idx;
    char pw[20] = "";

    sscanf(args, "%d %s", &display_idx, pw);

    int idx = find_room_index_by_display(display_idx);
    if (idx < 0) {
        write(sock, "[오류] 존재하지 않는 방입니다.\n", strlen("[오류] 존재하지 않는 방입니다.\n"));
        pthread_mutex_unlock(&rooms_mutex);
        return;
    }

    if (!rooms[idx].is_public && strcmp(pw, rooms[idx].password) != 0) {
        write(sock, "[오류] 비밀번호가 틀렸습니다.\n", strlen("[오류] 비밀번호가 틀렸습니다.\n"));
        pthread_mutex_unlock(&rooms_mutex);
        return;
    }

    for (int j = 0; j < rooms[idx].member_count; j++) {
        if (strcmp(rooms[idx].members[j], id) == 0) {
            write(sock, "[알림] 이미 입장한 방입니다.\n", strlen("[알림] 이미 입장한 방입니다.\n"));
            pthread_mutex_unlock(&rooms_mutex);
            return;
        }
    }

    // 멤버 추가 시 strncpy + null 종료
    strncpy(rooms[idx].members[rooms[idx].member_count], id, sizeof(rooms[idx].members[0]) - 1);
    rooms[idx].members[rooms[idx].member_count][sizeof(rooms[idx].members[0]) - 1] = '\0';

    rooms[idx].member_count++;


    // DB에 room_members INSERT
    {
        char sql[128];
        snprintf(sql, sizeof(sql),
                 "INSERT INTO room_members (room_id, user_id) "
                 "VALUES (%d,'%s')",
                 rooms[idx].room_id, id);
        if (mysql_query(conn, sql)) {
            fprintf(stderr, "[DB_ROOMMEM INSERT ERROR] %s\n",
                    mysql_error(conn));
        }
    }

    // 클라이언트 응답 및 이동 처리
    
    // display index 계산 (in-use된 방 중 idx 이전의 개수를 세기)

    int display_idx2 = 0;
    for (int j = 0; j < idx; ++j) {
        if (rooms[j].in_use) display_idx2++;
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "[입장 성공] 방번호: %d, 방제목: %s\n", display_idx2, rooms[idx].name);
    send_clear_screen(sock);
    write(sock, msg, strlen(msg));

    for (int k = 0; k < member_count; k++) {
        if (strcmp(members[k].id, id) == 0) {
            strncpy(members[k].current_room, rooms[idx].name, sizeof(members[k].current_room) - 1);
            members[k].current_room[sizeof(members[k].current_room) - 1] = '\0';
            members[k].in_room = 1;
            break;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
}




// 방 퇴장
void handle_leave_room(const char *id, int sock) {
    pthread_mutex_lock(&rooms_mutex);
    for (int i = 0; i < MAX_ROOM; i++) {
        if (!rooms[i].in_use) continue;
        for (int j = 0; j < rooms[i].member_count; j++) {
            if (strcmp(rooms[i].members[j], id) == 0) {
                // in-memory 참가자 제거
                for (int k = j; k < rooms[i].member_count - 1; k++) {
                    strcpy(rooms[i].members[k], rooms[i].members[k + 1]);
                }
                rooms[i].member_count--;

                // ── 2) DB에서 해당 참가자 삭제 ──
                {
                    char sql[128];
                    snprintf(sql, sizeof(sql),
                             "DELETE FROM room_members "
                             "WHERE room_id=%d AND user_id='%s'",
                             rooms[i].room_id, id);
                    if (mysql_query(conn, sql)) {
                        fprintf(stderr, "[DB_ROOMMEM DEL ERROR] %s\n",
                                mysql_error(conn));
                    }
                }

                // ── 3) 방장 이탈 시 권한 이관 ──
                if (strcmp(rooms[i].owner, id) == 0 && rooms[i].member_count > 0) {
                    char new_owner[20];
                    strncpy(new_owner, rooms[i].members[0], sizeof(new_owner) - 1);
                    new_owner[sizeof(new_owner) - 1] = '\0';

                    strncpy(rooms[i].owner, new_owner, sizeof(rooms[i].owner) - 1);
                    rooms[i].owner[sizeof(rooms[i].owner) - 1] = '\0';

                    // 3.2) DB 갱신
                    {
                        char sql2[128];
                        snprintf(sql2, sizeof(sql2),
                                 "UPDATE rooms SET owner='%s' WHERE room_id=%d",
                                 new_owner, rooms[i].room_id);
                        if (mysql_query(conn, sql2) != 0) {
                            fprintf(stderr,
                                    "[DB_ROOM OWNER UPDATE ERROR] %s\n",
                                    mysql_error(conn));
                        }
                    }
                }

                // ── 4) 마지막 퇴장 시 방 완전 삭제 ──
                if (rooms[i].member_count == 0) {
                    rooms[i].in_use = 0;
                    // DB rooms에서도 삭제
                    char sql[128];
                    snprintf(sql, sizeof(sql),
                             "DELETE FROM rooms WHERE room_id=%d",
                             rooms[i].room_id);
                    if (mysql_query(conn, sql)) {
                        fprintf(stderr, "[DB_ROOM DEL ERROR] %s\n",
                                mysql_error(conn));
                    }
                }

               // ── 5) 클라이언트 로비 복귀 안내 ──
               send_clear_screen(sock);
               write(sock, "[퇴장] 로비로 이동합니다.\n", strlen("[퇴장] 로비로 이동합니다.\n"));
                send_lobby_welcome(sock);
                notify_offline_note_count(id, sock);
                // ─── 여기에 추가 ───
                // in-memory Member 배열에서도 in_room 플래그를 0 으로, current_room을 "lobby"로 리셋
                for (int u = 0; u < member_count; ++u) {
                    if (strcmp(members[u].id, id) == 0) {
                        members[u].in_room = 0;
                        strncpy(members[u].current_room, "lobby",
                                sizeof(members[u].current_room)-1);
                        members[u].current_room[sizeof(members[u].current_room)-1] = '\0';
                        break;
                    }
                }
                pthread_mutex_unlock(&rooms_mutex);
                return;
            }
        }
    }
    write(sock, "[알림] 참여 중인 방이 없습니다.\n", strlen("[알림] 참여 중인 방이 없습니다.\n"));
    pthread_mutex_unlock(&rooms_mutex);
}


void handle_roomlist(int sock) {
    pthread_mutex_lock(&rooms_mutex);
    char line[256];
    // 헤더(컬럼명) 정렬
    snprintf(line, sizeof(line),
        "%-4s %-20s %-12s %-4s %-4s\n",
        "번호", "제목", "방장", "인원", "공개");
    write(sock, line, strlen(line));

    int display_idx = 0;
    for (int i = 0; i < MAX_ROOM; ++i) {
        if (!rooms[i].in_use) continue;
        snprintf(line, sizeof(line),
            "%-4d %-20s %-12s %-4d %-4s\n",
            display_idx,
            rooms[i].name,
            rooms[i].owner,
            rooms[i].member_count,
            rooms[i].is_public ? "O" : "X");
        write(sock, line, strlen(line));
        display_idx++;
    }
    if (display_idx == 0) {
        write(sock, "[알림] 활성 방이 없습니다.\n",
            strlen("[알림] 활성 방이 없습니다.\n"));
    }
    pthread_mutex_unlock(&rooms_mutex);
}

void handle_kick(const char *owner, const char *args) {
    pthread_mutex_lock(&rooms_mutex);
    char target[20];
    sscanf(args, "%19s", target);

    // 1) 자기 자신 강퇴 차단
    if (strcmp(owner, target) == 0) {
        for (int m = 0; m < member_count; ++m) {
            if (strcmp(members[m].id, owner) == 0) {
                write(members[m].sockfd,
                      "[오류] 본인을 강퇴할 수 없습니다.\n",
                      strlen("[오류] 본인을 강퇴할 수 없습니다.\n"));
                pthread_mutex_unlock(&rooms_mutex);
                return;
            }
        }
    }

    // 2) 방장 소유 방 검색
    int room_idx = -1;
    for (int i = 0; i < MAX_ROOM; ++i) {
        if (rooms[i].in_use && strcmp(rooms[i].owner, owner) == 0) {
            room_idx = i;
            break;
        }
    }
    if (room_idx < 0) {
        for (int m = 0; m < member_count; ++m) {
            if (strcmp(members[m].id, owner) == 0) {
                write(members[m].sockfd,
                      "[오류] 방장 권한이 있는 방을 찾을 수 없습니다.\n",
                      strlen("[오류] 방장 권한이 있는 방을 찾을 수 없습니다.\n"));
                pthread_mutex_unlock(&rooms_mutex);
                return;
            }
        }
    }

    // 3) 대상자가 같은 방에 있는지 확인
    int found = 0;
    int pos = -1;
    for (int j = 0; j < rooms[room_idx].member_count; ++j) {
        if (strcmp(rooms[room_idx].members[j], target) == 0) {
            found = 1;
            pos = j;
            break;
        }
    }
    if (!found) {
        for (int m = 0; m < member_count; ++m) {
            if (strcmp(members[m].id, owner) == 0) {
                write(members[m].sockfd,
                      "[오류] 대상 사용자가 같은 방에 없습니다.\n",
                      strlen("[오류] 대상 사용자가 같은 방에 없습니다.\n"));
                pthread_mutex_unlock(&rooms_mutex);
                return;
            }
        }
    }

    // 4) 강퇴 처리
    for (int m = 0; m < member_count; ++m) {
        if (strcmp(members[m].id, target) == 0 &&
            members[m].in_room == 1 &&
            strcmp(members[m].current_room, rooms[room_idx].name) == 0) {
            write(members[m].sockfd,
                  "[강퇴] 방에서 추방되었습니다. 로비로 이동합니다.\n",
                  strlen("[강퇴] 방에서 추방되었습니다. 로비로 이동합니다.\n"));
            members[m].in_room = 0;
            strncpy(members[m].current_room, "lobby", sizeof(members[m].current_room) - 1);
            members[m].current_room[sizeof(members[m].current_room) - 1] = '\0';
            send_lobby_welcome(members[m].sockfd);
            found = 1;
            break;
        }
    }

    // 4-2) in-memory 방 멤버 배열에서 제거
    for (int k = pos; k < rooms[room_idx].member_count - 1; ++k) {
        strncpy(rooms[room_idx].members[k], rooms[room_idx].members[k + 1], sizeof(rooms[room_idx].members[0]) - 1);
        rooms[room_idx].members[k][sizeof(rooms[room_idx].members[0]) - 1] = '\0';
    }
    rooms[room_idx].member_count--;

    // 4-3) DB 동기화
    char sql[128];
    snprintf(sql, sizeof(sql),
             "DELETE FROM room_members WHERE room_id=%d AND user_id='%s'",
             rooms[room_idx].room_id, target);
    mysql_query(conn, sql);

    // 4-4) 방장에게 완료 메시지
    for (int m = 0; m < member_count; ++m) {
        if (strcmp(members[m].id, owner) == 0) {
            write(members[m].sockfd,
                  "[알림] 강퇴가 완료되었습니다.\n",
                  strlen("[알림] 강퇴가 완료되었습니다.\n"));
            break;
        }
    }

    log_server(owner, "강퇴 실행됨");
    pthread_mutex_unlock(&rooms_mutex);
}


void handle_owner(const char *owner, const char *args) {
    pthread_mutex_lock(&rooms_mutex);
    char next_owner[20];
    sscanf(args, "%19s", next_owner);

    for (int i = 0; i < MAX_ROOM; i++) {
        if (!rooms[i].in_use) continue;
        if (strcmp(rooms[i].owner, owner) != 0) continue;
        for (int j = 0; j < rooms[i].member_count; j++) {
            if (strcmp(rooms[i].members[j], next_owner) == 0) {
                // 안전하게 복사 및 null 종료
                strncpy(rooms[i].owner, next_owner, sizeof(rooms[i].owner) - 1);
                rooms[i].owner[sizeof(rooms[i].owner) - 1] = '\0';

                log_server(owner, "방장 권한 위임");
                pthread_mutex_unlock(&rooms_mutex);
                return;
            }
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
}

