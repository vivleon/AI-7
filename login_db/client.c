// client.c — select() 기반 ncurses 채팅 클라이언트

#include <locale.h>
#include <ncursesw/ncurses.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <stdbool.h>
#include <ctype.h>             // 변경: isspace() 사용을 위한 헤더
#include "global.h"  // BUF_SIZE = 2048

#define INPUT_HEIGHT 3

static WINDOW *chat_win, *input_win;
static char input_buf[BUF_SIZE];

// Nagle 알고리즘 비활성화
static void disable_nagle(int sock) {
    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
}

void init_ui() {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    chat_win  = newwin(rows - INPUT_HEIGHT, cols, 0, 0);
    input_win = newwin(INPUT_HEIGHT, cols, rows - INPUT_HEIGHT, 0);

    scrollok(chat_win, TRUE);
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 2, "> ");
    wrefresh(chat_win);
    wrefresh(input_win);
}

void end_ui() {
    delwin(chat_win);
    delwin(input_win);
    endwin();
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "사용법: %s <IP> <PORT>\n", argv[0]);
        exit(1);
    }

    init_ui();

    // 1) 서버 연결
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { end_ui(); perror("socket"); exit(1); }
    disable_nagle(sock);

    struct sockaddr_in serv = {0};
    serv.sin_family = AF_INET;
    serv.sin_port   = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &serv.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        end_ui(); perror("connect"); exit(1);
    }

    // 2) 로그인/가입 프롬프트 처리
    bool lobby_cleared = false;
    char buf[BUF_SIZE];
    while (1) {
        int n = read(sock, buf, BUF_SIZE - 1);
        if (n <= 0) { end_ui(); exit(1); }
        buf[n] = '\0';

        // [로비에 입장하였습니다] 메시지 감지
        if (strstr(buf, "[로비에 입장하였습니다]")) {
            if (!lobby_cleared) {
                werase(chat_win);
                wrefresh(chat_win);
                lobby_cleared = true;
            }
            break;
        }

        wprintw(chat_win, "%s", buf);
        wrefresh(chat_win);

        // 서버 요청에 대한 입력
        echo();
        werase(input_win);
        box(input_win, 0, 0);
        mvwprintw(input_win, 1, 2, "> ");
        wrefresh(input_win);
        wgetnstr(input_win, input_buf, BUF_SIZE - 1);
        noecho();

        // 입력 전송 (단순 그대로)
        dprintf(sock, "%s\n", input_buf);
    }

    // 3) UI 초기화 및 환영 메시지
    werase(input_win);
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 2, "> ");
    wrefresh(chat_win);
    wrefresh(input_win);

    wprintw(chat_win, "[로비에 입장하였습니다] 명령어는 /help 참고\n");
    wrefresh(chat_win);

    // 4) select() 기반 채팅 루프
    bool join_cleared = false, leave_cleared = false, logout_cleared = false;
    int maxfd = sock > STDIN_FILENO ? sock : STDIN_FILENO;
    while (1) {
        fd_set rd;
        FD_ZERO(&rd);
        FD_SET(sock, &rd);
        FD_SET(STDIN_FILENO, &rd);

        if (select(maxfd + 1, &rd, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        // 4.1) 서버 메시지 수신
        if (FD_ISSET(sock, &rd)) {
            char buf2[BUF_SIZE];
            int n = read(sock, buf2, BUF_SIZE - 1);
            if (n <= 0) break;
            buf2[n] = '\0';
            char *start = buf2, *p;
            while ((p = strchr(start, '\n'))) {
                *p = '\0';
                // 화면 클리어 이벤트
                if (strstr(start, "[입장 성공]") && !join_cleared) {
                    werase(chat_win); wrefresh(chat_win);
                    join_cleared = false;
                } else if (strstr(start, "[퇴장] 로비로 이동합니다.") && !leave_cleared) {
                    werase(chat_win); wrefresh(chat_win);
                    leave_cleared = true;
                } else if (strstr(start, "[로그아웃] 로그인 화면으로 돌아갑니다.") && !logout_cleared) {
                    werase(chat_win); wrefresh(chat_win);
                    logout_cleared = true;
                }
                wprintw(chat_win, "%s\n", start);
                start = p + 1;
            }
            if (*start) wprintw(chat_win, "%s", start);
            wrefresh(chat_win);
        }

        // 4.2) 사용자 입력 처리
        if (FD_ISSET(STDIN_FILENO, &rd)) {
            curs_set(1);
            echo();
            werase(input_win);
            box(input_win, 0, 0);
            mvwprintw(input_win, 1, 2, "> ");
            wrefresh(input_win);

            wgetnstr(input_win, input_buf, BUF_SIZE - 1);

            noecho();
            curs_set(0);

            // 변경: 공백만 있으면 전송하지 않음
            char *p2 = input_buf;
            while (*p2 && isspace((unsigned char)*p2)) p2++;
            if (*p2) {
                dprintf(sock, "%s\n", input_buf);
            }

            werase(input_win);
            box(input_win, 0, 0);
            mvwprintw(input_win, 1, 2, "> ");
            wrefresh(input_win);
        }
    }

    close(sock);
    end_ui();
    return 0;
}
