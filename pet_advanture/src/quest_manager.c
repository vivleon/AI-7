/* src/quest_manager.c */
#include <stdio.h>
#include <string.h>
#include "quest_manager.h"
#include "final_boss.h"    // check_final_boss_entry, enter_final_boss
#include "inventory_manager.h"


// define global quests array
Quest quests[MAX_QUESTS] = {
    {1, QUEST_MAIN, QSTATE_AVAILABLE, "이장 👴", "대혼란의 시작",
     "어서 와라, 젊은 모험가여!\n이 마을에 온 것을 환영하네.\n우선 나와 인사나 하게.\n",
     "허허, 잘 왔네. 이제 본격적인 모험을 시작하게! \n마을 상인을 찾아가보게나.", 0, 50, 100, NULL},
    {2, QUEST_MAIN, QSTATE_LOCKED,    "상인 🧙", "첫 물약 구매",
     "처음엔 물약이 필요할 걸세. \n마을 상인에게서 물약을 한 병 사오게.\n",
     "잘 했네! 앞으로 전투에서 유용하게 쓰게나.\n고대 숲 던전에 들어가 고대숲정령에게 말을 걸어보게나.\n", 1, 50,   100,   NULL},
    {3, QUEST_MAIN, QSTATE_LOCKED,    "고대숲정령 🐲", "고대숲 보스 토벌",
     "고대숲 깊숙한 곳에 보스가 버티고 있네. 처치해 오게.\n",
     "이제 숲의 평화가 다시 찾아왔네! 귀환하게.\n", 1, 200, 100, NULL},
    {4, QUEST_MAIN, QSTATE_LOCKED,    "길드마스터 🎖️", "사막 몬스터 5마리 사냥",
     "사막에서 몬스터 5마리를 사냥해 오게.\n",
     "믿기 힘들겠지만, 정말 5마리나 사냥했군! 칭찬해 주지.\n", 5, 300, 150, NULL},
    {5, QUEST_MAIN, QSTATE_LOCKED,    "광산 광부 🪨", "설원 스톤 3종 수집",
     "설원광산에서 3종의 크리스탈을 모아 와야 하네.\n",
     "모든 크리스탈을 가져왔구만!\n 2차 진화를 위한 준비가 끝났네.\n 마을로가서 의식의 사제를 찾게\n", 3, 400, 200, NULL},
    {6, QUEST_MAIN, QSTATE_LOCKED,    "의식의 사제 🔮", "2차 진화 의식",
     "진정한 진화를 위해 사제와 함께 의식을 거행하라.\n",
     "의식이 끝났네! 이제 더 강해질 수 있다네.(진화의 돌x3,lv20이상)\n 준비가 되었으면 다시 설원으로 돌아가 몬스터 한마리를 잡아보게\n", 0, 300,   600,   NULL},
    {7, QUEST_MAIN, QSTATE_LOCKED,    "네메시스 처단자👹", "최종 보스 네메시스 처치 대비",
     "모든 준보스들을 처치하고, 전설 속 최종 보스 네메시스를 처치할 준비를 해라.\n",
     "네메시스가 눈치채고 깨어나 버렸구만! 영웅이여, 준비하게\n", 9, 500,1000, "영웅"}
};

int questRemaining[MAX_QUESTS];

void init_quests(void) {
    for (int i = 0; i < MAX_QUESTS; ++i) {
        quests[i].state = (i == 0 ? QSTATE_AVAILABLE : QSTATE_LOCKED);
        questRemaining[i] = quests[i].target;
    }
}

void show_quest_board(void) {
    printf("\n=== 퀘스트 목록 ===\n");
    for (int i = 0; i < MAX_QUESTS; ++i) {
        Quest *q = &quests[i];
        if (q->state == QSTATE_AVAILABLE || q->state == QSTATE_ACTIVE) {
            const char *s = (q->state == QSTATE_ACTIVE) ? "진행중" : "수락 가능";
            if (q->target > 0) {
                printf("%2d) [%s] %s (%d/%d)\n", q->id, s, q->title,
                       q->target - questRemaining[i], q->target);
            } else {
                printf("%2d) [%s] %s\n", q->id, s, q->title);
            }
        }
    }
}

Quest* get_current_quest(void) {
    for (int i = 0; i < MAX_QUESTS; ++i)
        if (quests[i].state == QSTATE_ACTIVE)
            return &quests[i];
    return NULL;
}

void accept_quest(int questId) {
    Quest *q = &quests[questId-1];
    if (q->state != QSTATE_AVAILABLE) return;

    q->state = QSTATE_ACTIVE;
    printf("\n[%s]: %s\n", q->npcName, q->desc);
    printf("(계속하려면 Enter 키를 누르세요...)");
    while (getchar() != '\n');

    // target == 0 이면 바로 완료
    if (q->target == 0) {
        q->state = QSTATE_COMPLETED;
        extern int playerGold;
        playerGold += q->rewardGold;
        extern void gain_xp(int);
        gain_xp(q->rewardXP);
        printf("\n[%s 완료] %s - 보상 Gold %d, XP %d\n",
               q->title, q->onComplete, q->rewardGold, q->rewardXP);
        // 다음 퀘스트 오픈
        getchar();
        if (questId < MAX_QUESTS)
            quests[questId].state = QSTATE_AVAILABLE;
    }
}

void update_quest_progress(int questId, int amount) {
    int idx = questId - 1;
    Quest *q = &quests[idx];
    if (q->state != QSTATE_ACTIVE || q->target == 0) return;
    switch (questId) {
        case 1: // 인사 퀘스트
        case 6: // 의식 퀘스트
            questRemaining[idx] = 0;
            break;

        case 2: { // 물약 구매 퀘스트
            extern int get_item_count(int);
            int totalPotions = 0;
            for (int id = ITEM_HP_LOW; id <= ITEM_SP_HIGH; ++id) {
                totalPotions += get_item_count(id);
            }
            if (totalPotions > 0) questRemaining[idx] = 0;
            break;
        }

        case 3: // 고대숲 보스 토벌
            if (amount > 0)
                questRemaining[idx] = 0;
            break;
        case 7: // 최종 보스 처치
            questRemaining[idx] -= amount;
            break;

        default:
            // 나머지 일반 사냥/수집 퀘스트
            questRemaining[idx] -= amount;
            break;
    }


    if (questRemaining[idx] <= 0) {
        q->state = QSTATE_COMPLETED;
        extern int playerGold;
        playerGold += q->rewardGold;
        extern void gain_xp(int);
        gain_xp(q->rewardXP);
        printf("\n[%s 완료] %s - 보상 Gold %d, XP %d\n",
               q->title, q->onComplete, q->rewardGold, q->rewardXP);
        getchar();
        if (questId < MAX_QUESTS)
            quests[questId].state = QSTATE_AVAILABLE;
    }
}

void check_quest_completion(void) {
    if (quests[MAX_QUESTS-1].state == QSTATE_COMPLETED && check_final_boss_entry()) {
        enter_final_boss();
    }
}
