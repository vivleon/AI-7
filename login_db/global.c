// global.c
#include "member.h"
#include "global.h"
#include <pthread.h>

pthread_mutex_t members_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_mutex   = PTHREAD_MUTEX_INITIALIZER; 

Member members[MAX_CLIENTS];
int member_count = 0;
int clnt_count = 0;
int clnt_socks[100];
char clnt_ids[100][20];
