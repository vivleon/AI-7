// global.h
#ifndef GLOBAL_H
#define GLOBAL_H


#include "member.h"
#define BUF_SIZE 2048
#define ESC_MSG_MAX (BUF_SIZE - 1)
#include <pthread.h>

extern pthread_mutex_t members_mutex;
extern pthread_mutex_t rooms_mutex; 
extern Member members[MAX_CLIENTS];
extern int member_count;
extern int clnt_count;
extern int clnt_socks[100];
extern char clnt_ids[100][20];

#endif
