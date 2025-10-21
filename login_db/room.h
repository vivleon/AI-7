#ifndef ROOM_H
#define ROOM_H

#include "member.h"  // for Member type and MAX_CLIENTS

#define MAX_ROOM   10
#define MAX_MEMBER 100

// // Room 구조체 정의
// typedef struct {
//     int  in_use;                     // 방 사용 여부
//     int  room_id;                    // DB와 연동된 고유 ID
//     char name[50];                   // 방 이름
//     char owner[20];                  // 방장 ID
//     int  is_public;                  // 공개 여부 (1: 공개, 0: 비공개)
//     char password[20];               // 비공개 방 비밀번호
//     char members[MAX_MEMBER][20];    // 방에 참가 중인 사용자 ID 리스트
//     int  member_count;               // 현재 참가자 수
// } Room;

// 서버 시작/재시작 시 in-memory 방 초기화
void init_rooms(void);

// 채팅방 생성, 입장, 퇴장, 목록 조회
void handle_create_room(const char *id, int sock, const char *args);
void handle_join_room(const char *id, int sock, const char *args);
void handle_leave_room(const char *id, int sock);
void handle_roomlist(int sock);

// 방장 전용: 강퇴 및 방장 권한 위임
void handle_kick(const char *owner, const char *args);
void handle_owner(const char *owner, const char *args);

#endif // ROOM_H
