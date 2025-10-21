#include "chat_util.h"
#include "global.h"
#include "member.h"
#include <mysql/mysql.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "db.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define ESC_MSG_MAX (BUF_SIZE - 1)

extern Member members[MAX_CLIENTS];
extern int member_count;
extern MYSQL *conn;  

#ifndef TCP_QUICKACK
#  define TCP_QUICKACK 12
#endif


// 1) 안전한 줄 단위 입력
ssize_t read_line(int sock, char *buf, size_t size) {
    size_t i = 0;
    char c;
    while (i < size - 1) {
        ssize_t n = read(sock, &c, 1);
        if (n <= 0) break;
        if (c == '\r') continue;
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

// 2) 남은 입력 버퍼 비우기
void flush_input(int sock) {
    char c;
    // 개행이 나올 때까지 읽어서 버린다
    while (read(sock, &c, 1) > 0 && c != '\n');
}

// 3) 화면 클리어 시그널 전송
void send_clear_screen(int sock) {
   // 화면 클리어 코드는 클라이언트(UI)에서 처리하도록 제거
   /* const char *clear = "\033[2J\033[H";
      write(sock, clear, strlen(clear)); */
}

// 4) 같은 방 사용자에게 브로드캐스트
void broadcast_from(const char *sender_id, const char *msg) {
    // 시간 포맷
    time_t t = time(NULL);
    struct tm tm_buf;
    localtime_r(&t, &tm_buf);
    char datetimebuf[20];
    strftime(datetimebuf, sizeof(datetimebuf), "%Y-%m-%d %H:%M:%S", &tm_buf);

    // 메시지 정리
    char content[ESC_MSG_MAX+1];
    strncpy(content, msg, ESC_MSG_MAX);
    content[ESC_MSG_MAX] = '\0';
    content[strcspn(content, "\r\n")] = '\0';

    // 보낼 방 찾기
    char sender_room[50] = {0};
    for (int i = 0; i < member_count; i++) {
        if (strcmp(members[i].id, sender_id) == 0) {
            strncpy(sender_room, members[i].current_room, sizeof(sender_room)-1);
            break;
        }
    }

    // 실제 전송
    char buffer[2048];
    // 남은 공간 계산: "[id] " + content + " (ts)\n"
    size_t id_len = strlen(sender_id);
    size_t ts_len = strlen(datetimebuf);
    // 전체 버퍼 크기에서 id, timestamp, 포맷 오버헤드(대괄호·공백·괄호·개행) 빼고 콘텐츠에 할당
    int max_content = (int)(sizeof(buffer) - id_len - ts_len - 8);
    if (max_content < 0) max_content = 0;
    // %.*s 로 콘텐츠 길이를 명시적으로 제한
    snprintf(buffer, sizeof(buffer),
             "[%s] %.*s (%s)\n",
             sender_id,
             max_content, content,
             datetimebuf);
    for (int i = 0; i < member_count; i++) {
        int fd = members[i].sockfd;
        if (fd <= 0 ||
            strcmp(members[i].current_room, sender_room) != 0)
            continue;

        // // 송신자 본인은 브로드캐스트에서 제외
        // if (strcmp(members[i].id, sender_id) == 0)
        //     continue;

        // 1) 빠른 ACK 으로 버퍼를 곧바로 비워 달라 요청
#ifdef TCP_QUICKACK
        {
            int one = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &one, sizeof(one));
        }
#endif
        // 2) 실제 전송
        // 송신자를 제외하지 않고 모두에게 전송
        send(fd, buffer, strlen(buffer), MSG_NOSIGNAL);
    }

    // DB 기록
    if (!db_log_chat(sender_room, sender_id, content, datetimebuf)) {
        fprintf(stderr,
                "[DB_LOG ERROR] chat_logs insert failed: errno=%u, msg='%s'\n",
                mysql_errno(conn),
                mysql_error(conn));
    }
}
// 5) 서버 로그 파일 기록
void log_server(const char *id, const char *event) {
    FILE *fp = fopen("server_log.txt", "a");
    if (!fp) return;
    fprintf(fp, "%s [%s] %s\n", "", id, event);
    fclose(fp);
}

// 6) 로비 환영 메시지
void send_lobby_welcome(int sock) {
    const char *msg = "[로비에 입장하였습니다] 명령어는 /help 참고\n";
    write(sock, msg, strlen(msg));
}

// 7) 접속자 목록 조회
void show_online_users(int sock) {
    // 1) DB에서 데이터 가져오기
    if (mysql_query(conn,
        "SELECT user_id, DATE_FORMAT(login_at,'%Y-%m-%d %H:%i:%s') "
        "FROM active_users"
    ) != 0) {
        const char *err = "[오류] DB 조회 실패\n";
        write(sock, err, strlen(err));
        return;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    // 2) 헤더 출력
    const char *title = "[현재 접속자]\n";
    write(sock, title, strlen(title));
    char header[64];
    // ID 칸은 20글자 폭, 로그인시간 칸은 가변폭
    snprintf(header, sizeof(header), "%-20s | %s\n", "ID", "로그인시간");
    write(sock, header, strlen(header));
    // 구분선 (ID 칸 20 + 구분자 3 + 로그인시간 칸 길이)
    write(sock, "----------------------+-------------------\n",
          strlen("----------------------+-------------------\n"));

    // 3) 각 행 출력
    MYSQL_ROW row;
    int cnt = 0;
    while ((row = mysql_fetch_row(res))) {
        char line[128];
        // ID 칸 고정폭, 시간은 그대로
        snprintf(line, sizeof(line), "%-20s | %s\n", row[0], row[1]);
        write(sock, line, strlen(line));
        cnt++;
    }
    mysql_free_result(res);

    // 4) 총합 출력
    char footer[64];
    snprintf(footer, sizeof(footer), "총 접속자 수: %d\n", cnt);
    write(sock, footer, strlen(footer));
}

// 7) 귓속말 핸들러

void handle_note(const char *sender, const char *args) {
    // 1) 파싱: target, message
    char target[20], message[ESC_MSG_MAX+1];
    const char *p = strchr(args, ' ');
    if (!p) return;
    size_t tlen = p - args;
    if (tlen >= sizeof(target)) tlen = sizeof(target) - 1;
    strncpy(target, args, tlen);
    target[tlen] = '\0';
    strncpy(message, p + 1, ESC_MSG_MAX);
    message[ESC_MSG_MAX] = '\0';
    
    // 2) DB 로깅 (thread‑safe)
    time_t t = time(NULL);
    struct tm tm_buf;
    if (localtime_r(&t, &tm_buf) == NULL) memset(&tm_buf, 0, sizeof(tm_buf));
    char ts[20];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);   if (!db_log_note(sender, target, message, ts)) {
        fprintf(stderr, "[DB_NOTE ERROR]\n");
    }

    // 3) 대상 ID 유효성 검사
    if (!db_id_exists(target)) {
        // 발신자에게 에러 알림
        for (int i = 0; i < member_count; i++) {
            if (strcmp(members[i].id, sender) == 0) {
                write(members[i].sockfd,
                      "[오류] 상대 유저가 존재하지 않습니다.\n",
                      strlen("[오류] 상대 유저가 존재하지 않습니다.\n"));
                return;
            }
        }
        return;
    }

    // 4) members[]에서 로그인 중 여부 확인 & 귓속말 전송
    int delivered = 0;
    for (int i = 0; i < member_count; i++) {
        if (strcmp(members[i].id, target) == 0) {
            // 상대가 로그인 중
            char buf[ESC_MSG_MAX + 64];
            snprintf(buf, sizeof(buf),
                     "[귓속말 from %s] %s (%s)\n",
                     sender, message, ts);
            write(members[i].sockfd, buf, strlen(buf));
            delivered = 1;
            break;
        }
    }

    // 5) 발신자에게 결과 안내
    for (int i = 0; i < member_count; i++) {
        if (strcmp(members[i].id, sender) == 0) {
            if (delivered) {
                write(members[i].sockfd,
                      "\n",
                      strlen("\n"));
            } else {
                write(members[i].sockfd,
                      "[알림] 상대가 오프라인입니다. 쪽지를 남겼습니다.\n",
                      strlen("[알림] 상대가 오프라인입니다. 쪽지를 남겼습니다.\n"));
            }
            break;
        }
    }
}

// 8) 방 참여자 목록 조회
void show_room_members(int sock, const char *room_name) {
    // 1) 제목
    const char *title = "[현재 방 참여자]\n";
    write(sock, title, strlen(title));

    // 2) 헤더 (ID 칸: 20글자 폭)
    char header[32];
    snprintf(header, sizeof(header), "%-20s\n", "ID");
    write(sock, header, strlen(header));

    // 3) 구분선
    write(sock, "--------------------\n", strlen("--------------------\n"));

    // 4) 참여자 출력
    int cnt = 0;
    for (int i = 0; i < member_count; ++i) {
        if (members[i].in_room &&
            strcmp(members[i].current_room, room_name) == 0)
        {
            char line[32];
            snprintf(line, sizeof(line), "%-20s\n", members[i].id);
            write(sock, line, strlen(line));
            cnt++;
        }
    }

    // 5) 총 참여자 수
    char footer[64];
    snprintf(footer, sizeof(footer), "총 참여자 수: %d\n", cnt);
    write(sock, footer, strlen(footer));
}

// 통합 귓속말+쪽지 핸들러
int handle_whisper_or_note(const char *sender, const char *target, const char *message, int try_offline_note) {
    time_t t = time(NULL);
    struct tm tm_buf;
    if (localtime_r(&t, &tm_buf) == NULL) memset(&tm_buf, 0, sizeof(tm_buf));
    char ts[20];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    // 대상 유저 존재 체크
    if (!db_id_exists(target)) return -1; // 대상없음

    // 온라인 여부 체크
    for (int i = 0; i < member_count; i++) {
        if (strcmp(members[i].id, target) == 0) {
            // 온라인: 실시간 전달
            char buf[ESC_MSG_MAX+64];
            snprintf(buf, sizeof(buf), "[귓속말 from %s] %s (%s)\n", sender, message, ts);
            write(members[i].sockfd, buf, strlen(buf));
            return 1; // 실시간 전달 완료
        }
    }

    // 오프라인 → 쪽지 저장
    if (try_offline_note) {
        if (!db_log_note(sender, target, message, ts)) return -2; // 쪽지 저장 실패
        return 0; // 쪽지 저장 성공
    }
    // 오프라인이지만 쪽지 남기지 않기로 함
    return 2;
}