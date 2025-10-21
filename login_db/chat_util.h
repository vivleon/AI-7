// [chat_util.h]
#ifndef CHAT_UTIL_H
#define CHAT_UTIL_H

#include <unistd.h>   // ssize_t 정의
#include <stddef.h>   // size_t 정의
#include <string.h>


ssize_t read_line(int sock, char *buf, size_t size);  
void flush_input(int sock);  // ✅ 추가
void send_clear_screen(int sock);
void broadcast_from(const char *sender_id, const char *msg);
void log_server(const char *id, const char *event);
void send_lobby_welcome(int sock);
void show_online_users(int sock);
void handle_note(const char *sender, const char *args);
void show_room_members(int sock, const char *room_name);

#endif
