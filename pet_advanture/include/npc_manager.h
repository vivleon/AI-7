// include/npc_manager.h

#ifndef NPC_MANAGER_H
#define NPC_MANAGER_H

#include <stdbool.h>
#include "quest_manager.h"
#include "map_manager.h"

#define MAX_MAP_NPCS 16

// NPC 구조체: 퀘스트 연관 ID, 이름, 이모지, 좌표, 보스 여부
typedef struct {
    int         questId;   // 연관된 퀘스트 ID
    const char *name;      // NPC 이름
    const char *emoji;     // NPC 이모지
    int         x, y;      // 맵 상 좌표
    bool        isBoss;    // 보스 여부
} NPC;

// 퀘스트 배열 및 진행상태 extern 선언
extern Quest quests[MAX_QUESTS];
extern int   questRemaining[MAX_QUESTS];

// NPC 관리 함수들
void init_npcs(void);
void draw_npcs(int px, int py);
NPC* npc_at(int px, int py);
void spawn_npc(int questId, const char *name, const char *emoji, int x, int y);
void spawn_boss(int questId, const char *name, const char *emoji, int x, int y);
void spawn_npcs_for_map(int mapIndex);

#endif // NPC_MANAGER_H
