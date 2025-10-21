// src/npc_manager.c

#include <stdio.h>
#include <stdbool.h>
#include "npc_manager.h"
#include "quest_manager.h"
#include "map_manager.h"
#include "final_boss.h"

// // extern으로 선언된 퀘스트 데이터와 진행 상황 배열
extern Quest quests[MAX_QUESTS];
extern int   questRemaining[MAX_QUESTS];

// 내부 NPC 배열
static NPC mapNpcs[MAX_MAP_NPCS];
static int npcCount = 0;

// 맵 초기화 시 NPC 리스트 초기화
void init_npcs(void) {
    npcCount = 0;
}

void draw_npcs(int px, int py) {
    for (int i = 0; i < npcCount; i++) {
        NPC *n = &mapNpcs[i];
        if (!n->isBoss && (n->x == px && n->y == py)) continue;
        printf("\033[%d;%dH%s %s", n->y + 2, n->x * 2 + 1, n->emoji, n->name);
    }
}

NPC* npc_at(int px, int py) {
    for (int i = 0; i < npcCount; i++) {
        NPC *n = &mapNpcs[i];
        if (n->x == px && n->y == py) return n;
    }
    return NULL;
}

void spawn_npc(int questId, const char *name, const char *emoji, int x, int y) {
    if (npcCount < MAX_MAP_NPCS) {
        mapNpcs[npcCount++] = (NPC){ questId, name, emoji, x, y, false };
    }
}

void spawn_boss(int questId, const char *name, const char *emoji, int x, int y) {
    if (npcCount < MAX_MAP_NPCS) {
        mapNpcs[npcCount++] = (NPC){ questId, name, emoji, x, y, true };
    }
}

void spawn_npcs_for_map(int mapIndex) {
    init_npcs();
    switch (mapIndex) {
        case MAP_TOWN:
            spawn_npc(1, "이장", "👴", 10, 5);
            spawn_npc(2, "놀러나온 상점 주인", "🛒", 12, 7);
            spawn_npc(6, "의식 집행자", "🔮", 16, 9);
            break;
        case MAP_FOREST:
            spawn_boss(3, "고대숲정령", "🐲", 20, 10);
            break;
        case MAP_DESERT:
            spawn_npc(4, "길드장", "🎖️", 5, 5);
            break;
        case MAP_SNOW:
            spawn_npc(5, "학자", "🧙", 8, 12);
            break;
        case MAP_FINAL_TEMPLE:
            spawn_npc(7, "네메시스 처단자", "👹", 16, 22);
            break;
        default:
            break;
    }
}
