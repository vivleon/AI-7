// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>    // TCP_NODELAY
#include <pthread.h>
#include <signal.h>
#include "handler.h"
#include "db.h"
#include <mysql/mysql.h>

#define PORT 9011

extern MYSQL *conn;               // db_connect() 에서 세팅되는 전역 커넥션
int serv_sock;
int connection_count = 0;
pthread_mutex_t conn_count_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int sock;
    char addr_str[32];
} client_info;

void handle_sigint(int sig) {
    // DB 초기화
    if (mysql_query(conn, "SET FOREIGN_KEY_CHECKS=0"))
        fprintf(stderr, "[DB ERROR] FK 해제 실패: %s\n", mysql_error(conn));
    if (mysql_query(conn, "DELETE FROM active_users"))
        fprintf(stderr, "[DB ERROR] active_users 초기화 실패: %s\n", mysql_error(conn));
    if (mysql_query(conn, "DELETE FROM room_members"))
        fprintf(stderr, "[DB ERROR] room_members 초기화 실패: %s\n", mysql_error(conn));
    if (mysql_query(conn, "DELETE FROM rooms"))
        fprintf(stderr, "[DB ERROR] rooms 초기화 실패: %s\n", mysql_error(conn));
    if (mysql_query(conn, "SET FOREIGN_KEY_CHECKS=1"))
        fprintf(stderr, "[DB ERROR] FK 복원 실패: %s\n", mysql_error(conn));

    db_close();
    close(serv_sock);
    printf("\n[서버 종료] 안전 종료 완료\n");
    exit(0);
}

int main() {
    signal(SIGINT, handle_sigint);
    // 소켓 write 시 SIGPIPE 방지
    signal(SIGPIPE, SIG_IGN);

    if (!db_connect()) {
        fprintf(stderr, "DB 연결 실패. 서버 종료.\n");
        exit(1);
    }

    // 재시작 시 남은 상태 초기화
    if (mysql_query(conn, "SET FOREIGN_KEY_CHECKS=0"))
        fprintf(stderr, "[DB ERROR] FK 해제 실패: %s\n", mysql_error(conn));
    if (mysql_query(conn, "DELETE FROM active_users"))
        fprintf(stderr, "[DB ERROR] active_users 초기화 실패: %s\n", mysql_error(conn));
    if (mysql_query(conn, "DELETE FROM room_members"))
        fprintf(stderr, "[DB ERROR] room_members 초기화 실패: %s\n", mysql_error(conn));
    if (mysql_query(conn, "DELETE FROM rooms"))
        fprintf(stderr, "[DB ERROR] rooms 초기화 실패: %s\n", mysql_error(conn));
    if (mysql_query(conn, "SET FOREIGN_KEY_CHECKS=1"))
        fprintf(stderr, "[DB ERROR] FK 복원 실패: %s\n", mysql_error(conn));

    serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in serv_adr = {0};
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(PORT);

    if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) < 0) {
        perror("bind"); db_close(); exit(1);
    }
    if (listen(serv_sock, 5) < 0) {
        perror("listen"); db_close(); exit(1);
    }

    printf("[서버 시작] 포트 %d에서 대기 중...\n", PORT);

    while (1) {
        struct sockaddr_in clnt_adr;
        socklen_t clnt_adr_sz = sizeof(clnt_adr);
        int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
        if (clnt_sock < 0) continue;

        // Nagle 알고리즘 끄기
        int flag = 1;
        setsockopt(clnt_sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        // 빠른 ACK 요청
        #ifdef TCP_QUICKACK
        setsockopt(clnt_sock, IPPROTO_TCP, TCP_QUICKACK, &flag, sizeof(flag));
        #endif
        client_info *ci = malloc(sizeof(*ci));
        ci->sock = clnt_sock;
        inet_ntop(AF_INET, &clnt_adr.sin_addr, ci->addr_str, sizeof(ci->addr_str));
        snprintf(ci->addr_str + strlen(ci->addr_str),
                 sizeof(ci->addr_str)-strlen(ci->addr_str),
                 ":%d", ntohs(clnt_adr.sin_port));

        pthread_mutex_lock(&conn_count_mutex);
        connection_count++;
        printf("[접속] %s 연결 (총 %d)\n", ci->addr_str, connection_count);
        pthread_mutex_unlock(&conn_count_mutex);

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_handler, ci) != 0) {
            perror("pthread_create");
            free(ci);
            close(clnt_sock);
            continue;
        }
        pthread_detach(tid);
    }

    return 0;
}
