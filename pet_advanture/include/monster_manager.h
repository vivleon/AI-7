#ifndef MONSTER_MANAGER_H
#define MONSTER_MANAGER_H

#include <stdbool.h>
#include "monster_def.h"




#define BOSS_PER_MAP       1
#define MAX_MONSTERS      10

#define ITEM_STONE_DUNGEON1  1001
#define ITEM_STONE_DUNGEON2  1002
#define ITEM_STONE_DUNGEON3  1003
#define ITEM_STONE_DUNGEON4  1004
#define ITEM_STONE_DUNGEON5  1005
#define ITEM_STONE_DUNGEON6  1006
#define ITEM_STONE_DUNGEON7  1007



// ────────────────────────────────────────────────────────────
// 맵 위에 스폰된 몬스터
// ────────────────────────────────────────────────────────────
typedef struct SpawnedMonster {
    int    x, y;            // 토큰 단위 좌표
    Monster base;           // battle_manager 의 Monster 정보
    int    skillCooldown;
    bool   alive;
} SpawnedMonster;

#define MAX_SPAWNED_MONSTERS  128
// 전역 스폰 몬스터 배열과 개수 (battle_manager, npc_manager 등에서 사용)
extern SpawnedMonster mons[MAX_SPAWNED_MONSTERS];
extern int            monCount;

// 몬스터 시스템 초기화 (맵 이동 전 호출)
void     init_monsters(void);

// mapIndex에 맞춰 맵 위에 몬스터 스폰
void     spawn_monsters_for_map(int mapIndex);

// 플레이어(px,py) 기준으로 몬스터들과 NPC 그리기
void     draw_entities(int px, int py);

// 해당 좌표에 살아있는 몬스터가 있으면 포인터, 없으면 NULL
SpawnedMonster* monster_at(int px, int py);

// 몬스터 제거 처리 (예: 전투 승리 시 alive=false)
void     remove_monster(SpawnedMonster *m);
bool     is_any_mini_alive(void);  // ⬅️ 새로 정의할 함수 선언
#endif // MONSTER_MANAGER_H