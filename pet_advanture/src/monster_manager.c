#include "monster_manager.h"
#include "battle_manager.h"  // monsterPools, monsterDefs 가 extern 으로 선언됨
#include "map_manager.h"
#include "final_boss.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

// 최대 몬스터 수
SpawnedMonster mons[MAX_SPAWNED_MONSTERS];
int            monCount;

/// @brief 스폰 가능한 타일 판정
/// @param x 좌표 (토큰 단위)
/// @param y 좌표 (토큰 단위)
/// @return 바닥 타일(true)에서만 몬스터 스폰 허용
bool is_floor(int x, int y) {
    char c = current_map[y][x*4];
    // 오직 일반 바닥 토큰('0')에서만 스폰
    return c == TILE_FLOOR_TOKEN[0] || c == TILE_EVENT_TOKEN[0] || c == TILE_MAPD_TOKEN[0] || c == TILE_FBOSS_TOKEN[0] || c == TILE_GRESS_TOKEN[0];
}


// 1) 맵별 일반 몬스터 풀 (ID 1~30)
// 일반 몬스터 9개 + 보스 1개
int monsterPools[4][MAX_MONSTERS_PER_MAP] = {
    // map0: 마을
    { },
    // map1: 고대 숲
    {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10 },
    // map2: 사막 사원
    { 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 },
    // map3: 설원 고성
    { 21, 22, 23, 24, 25, 26, 27, 28, 29, 30 }
};
// 최종던전 풀: 준보스+최종보스 배열 (FINAL_POOL_SIZE 정의)
int monsterPoolsFinal[] = { 31,32,33,34,35,36,37,38,39,   /*준보스*/  40 /*최종보스*/ };
const int FINAL_POOL_SIZE = sizeof(monsterPoolsFinal)/sizeof(monsterPoolsFinal[0]);


void init_monsters(void) {
    srand((unsigned)time(NULL));
    monCount = 0;
}

void spawn_monsters_for_map(int mapIndex) {
    init_monsters();
    monCount = 0;
    // 마을(mapIndex==0)에는 몬스터 없음
    if (mapIndex == 0) return;

    // 던전1~3(mapIndex 1~3): 일반 몬스터 + 보스
    if (mapIndex >= 1 && mapIndex <= 3) {
        int pool = mapIndex;
        // 1) 일반 몬스터 스폰 (보스 제외한 풀[0 .. MAX_MONSTERS_PER_MAP-2] 중 랜덤)
        for (int i = 0; i < MAX_MONSTERS; i++) {
            int x, y;
            do {
                x = rand() % (MAP_TOKENS_W - 2) + 1;
                y = rand() % (HEIGHT - 2) + 1;
            } while (!is_floor(x, y) || current_map[y][x*4] == TILE_PORTAL_TOKEN[0] );
            mons[monCount].x = x;
            mons[monCount].y = y;
            int id = monsterPools[pool][ rand() % (MAX_MONSTERS_PER_MAP - 1) ];
            mons[monCount].base = monsterDefs[id];
            mons[monCount].base.isElite = false;
            mons[monCount].skillCooldown = 3;
            mons[monCount].alive = true;
            monCount++;
        }
        // 2) 보스 스폰 (풀의 마지막 요소를 고정 보스 ID로)
        {
            int bossId = monsterPools[pool][ MAX_MONSTERS_PER_MAP - 1 ];
            int bx = MAP_TOKENS_W / 2;
            int by = HEIGHT / 2;
            /* (중앙 위치 보정 로직 그대로) */
            mons[monCount].x = bx;
            mons[monCount].y = by;
            mons[monCount].base = monsterDefs[bossId];
            mons[monCount].base.isElite = true;
            mons[monCount].skillCooldown = 3;
            mons[monCount].alive = true;
            monCount++;
        }
    }
    // 최종 던전(mapIndex==4): 최종보스 + 주위 준보스
    else if (mapIndex == 4) {
        // 준보스 9마리를 반드시 모두 스폰 (ID 31~39)
        int fx = MAP_TOKENS_W / 2;
        int fy = HEIGHT / 2;
        int offsets[9][2] = { {-2,-2}, {-3,0}, {-2,2}, {0,-3}, {0,2}, {2,-2}, {3,0}, {2,2}, {0,4} };
        // 0,0 위치는 중앙. 최종보스 스폰 위치로 남겨둘 수도 있음, 필요 시 위치 조정
        
        for (int i = 0; i < 9; ++i) {
            int x = fx + offsets[i][0];
            int y = fy + offsets[i][1];

            // 바닥이 아니면 중앙에서 한 칸 위로 보정 (필요에 따라 보정 로직 강화)
            if (!is_floor(x, y)) { x = fx; y = fy - 1; }

            mons[monCount].x = x;
            mons[monCount].y = y;
            mons[monCount].base = monsterDefs[31 + i];  // 31~39 준보스 순서대로 스폰
            mons[monCount].alive = true;
            mons[monCount].skillCooldown = 3;
            mons[monCount].base.isElite = true;
            monCount++;
        }
        // 최종보스는 아직 스폰하지 않고, 나중에 모두 처치된 뒤에 뿌릴 예정
    if (mapIndex == MAP_FINAL_TEMPLE && finalBossSpawned) {
        extern SpawnedMonster mons[];
        extern int monCount;
        int finalId = 40;
        mons[monCount] = (SpawnedMonster){
            .x = ALTAR_X,
            .y = ALTAR_Y,
            .base = monsterDefs[finalId],
            .skillCooldown = 3,
            .alive = true
        };
        mons[monCount].base.isElite = true;
        monCount++;
    }
    }

}



void draw_entities(int px, int py) {
    for (int i = 0; i < monCount; i++) {
        if (!mons[i].alive) continue;
        if (mons[i].x == px && mons[i].y == py) continue; // 플레이어 우선 출력

        // 이모지 출력 위치로 커서 이동
        printf("\033[%d;%dH", mons[i].y + 2, mons[i].x * 2 + 1);

        // 각 몬스터 정의에 등록된 이모지로 출력
        printf("%s", mons[i].base.emoji);
    }
    // 커서 원위치: draw_ui() 쪽에서 재설정
}

SpawnedMonster* monster_at(int px, int py) {
    for (int i = 0; i < monCount; i++) {
        if (mons[i].alive && mons[i].x == px && mons[i].y == py)
            return &mons[i];
    }
    return NULL;
}

void remove_monster(SpawnedMonster *m) {
    m->alive = false;
}


// ✅ 강화된 함수: monster_manager.c
bool is_any_mini_alive(void) {
    for (int i = 0; i < monCount; i++) {
        // ID 31~39번만 준보스로 간주
        if (mons[i].alive && mons[i].base.id >= 31 && mons[i].base.id <= 39) {
            return true; // 살아있는 준보스 존재
        }
    }
    return false; // 모두 사망
}
