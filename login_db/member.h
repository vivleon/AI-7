// [member.h]
#ifndef MEMBER_H
#define MEMBER_H

#define MAX_CLIENTS 100  // 👈 추가: 최대 클라이언트 수 정의


typedef struct {
    char id[20];
    char pw[20];
    char fname[50];
    char lname[50];
    char ph[20];
    char email[50];
    int disabled;
    int is_admin; 
    
    // 런타임용
    int sockfd;
    int in_room;
    char current_room[50];
} Member;

#endif