/* quest_manager.h */
#ifndef QUEST_MANAGER_H
#define QUEST_MANAGER_H

#include <stdbool.h>

#define MAX_QUESTS 7

typedef enum {
    QUEST_MAIN,
    QUEST_SIDE
} QuestType;

typedef enum {
    QSTATE_LOCKED,
    QSTATE_AVAILABLE,
    QSTATE_ACTIVE,
    QSTATE_COMPLETED
} QuestState;

// 퀘스트 구조체
typedef struct {
    int          id;
    QuestType    type;
    QuestState   state;
    const char  *npcName;
    const char  *title;
    const char  *desc;
    const char  *onComplete;
    int          target;       // 요구 수량
    int          rewardGold;
    int          rewardXP;
    const char  *rewardTitle;
} Quest;

// 배열과 진행 상태 전역 선언
extern Quest quests[MAX_QUESTS];
extern int   questRemaining[MAX_QUESTS];


void init_quests(void);
void show_quest_board(void);
Quest* get_current_quest(void);
void accept_quest(int questId);
void update_quest_progress(int questId, int amount);
void check_quest_completion(void);

#endif // QUEST_MANAGER_H