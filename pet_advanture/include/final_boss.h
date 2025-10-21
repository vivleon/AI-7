// include/final_boss.h
#ifndef FINAL_BOSS_H
#define FINAL_BOSS_H

#include <stdbool.h>
#include "battle_manager.h"
#include "monster_manager.h"


// 파이널 보스 진입 조건 검사
bool check_final_boss_entry(void);

// 제단에서 보스 전투 시작
void enter_final_boss(void);

// 엔딩 연출
void show_ending(void);


// 제단(altar) 스폰 위치 (토큰 단위 좌표 — x:[0–31], y:[0–31])
#define ALTAR_X 16
#define ALTAR_Y 15


// 플레이어가 제단 위치에 있는지 확인
bool is_at_altar(int px, int py);


// include/final_boss.h
extern bool finalBossSpawned;


#endif // FINAL_BOSS_H
