#ifndef BATTLE_MANAGER_H
#define BATTLE_MANAGER_H

#include <stdbool.h>
#include "skill_manager.h"
#include "monster_manager.h" 
#include "monster_def.h"

#define MAX_MONSTERS_PER_MAP 10

int get_current_dungeon_stone_id(void);
extern const int DUNGEON_STONE[5];

// 전방 선언: monster_manager.h 의 SpawnedMonster 로 정의됩니다.
typedef struct SpawnedMonster SpawnedMonster;

// 전투 액션 타입
typedef enum {
    ACTION_SKILL,
    ACTION_ITEM,
    ACTION_RUN
} ActionType;

// // ────────────────────────────────────────────────────────────
// // 몬스터 기본 정보 구조체
// // ────────────────────────────────────────────────────────────
// typedef struct Monster {
//     int   id;
//     const char *name;
//     char  emoji[16];
//     int   hp, maxHp;
//     int   atk, def, agi;
//     bool  isElite;
//     int   uniqueSkillId;
// } Monster;

// ────────────────────────────────────────────────────────────
// extern 몬스터 데이터
// ────────────────────────────────────────────────────────────

extern int     monsterPools[][MAX_MONSTERS_PER_MAP];
extern Monster monsterDefs[];


// // 외부에서 monsterPools, monsterDefs 를 참조할 수 있도록 선언
// extern int     monsterPools[][MAX_MONSTERS_PER_MAP];
// extern Monster monsterDefs[];


// ────────────────────────────────────────────────────────────
// 전투 상태
// ────────────────────────────────────────────────────────────
extern bool inBattle;
extern Monster currentMonster;
extern bool battleWon;

// 최종 보스 전투를 이미 연 적이 있는지
extern bool finalBossSpawned;

// ────────────────────────────────────────────────────────────
// 전투 관련 함수
// ────────────────────────────────────────────────────────────
// monsterId < 0: 랜덤, >=0: 지정 ID
// spawned: 해당 SpawnedMonster 포인터 전달
void      start_battle    (int monsterId, SpawnedMonster *spawned);
bool      in_battle       (void);
void      process_battle_turn(void);

#endif // BATTLE_MANAGER_H
