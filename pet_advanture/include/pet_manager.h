// include/pet_manager.h
#ifndef PET_MANAGER_H
#define PET_MANAGER_H

#include <stdbool.h>
#include "skill_manager.h" 

#define MAX_PETS       3
#define MAX_PET_NAME   32

// 진화용 돌
#define ITEM_EVOLUTION_STONE 100

typedef struct {
    int   id;                          // 펫 ID (0~2)
    char  name[MAX_PET_NAME];          // 펫 이름
    char  emoji[8];                    // 이모지
    int   level;                       // 레벨
    int   xp;                          // 경험치
    int   hp,    maxHp;                // 체력
    int   mp,    maxMp;                // 스킬포인트
    int   atk,   def,    agi;          // 스탯
    bool  evolved1;                    // 1차 진화 여부
    bool  evolved2;                    // 2차 진화 여부
    int   evoChoice;                   // 2차 진화 선택지 (1=빛,2=어둠,3=균형)
    // 스킬
    SkillSet skills;                   // include/skill_manager.h
} Pet;

// 1차 진화 (Lv>=10 & 스톤1개)
 bool try_evolve1(void);
// 사용자 선택을 통한 1차 진화 스킬 교체
void evolve1_choose_skills(void);
// 2차 진화 (Lv>=20 & 스톤3종 & questFlags)
bool try_evolve2(void);
// 사용자 선택을 통한 2차 진화 스킬 교체
void evolve2_choose_skills(void);

// 전역 펫 배열 및 현재 선택 펫 포인터
extern Pet pets[MAX_PETS];
extern Pet *currentPet;

// 플레이어 자원
extern int playerGold;
extern int crystalCount;  // 스톤 개수

// 초기화 및 시작
void init_game(void);
void select_pet(int petId);

// 경험치 & 레벨업
void gain_xp(int amount);

// 진화
bool try_evolve1(void);   // 1차 진화 (Lv>=10 & 스톤1개)
bool try_evolve2(void);   // 2차 진화 (Lv>=20 & 스톤3종 & questFlags)

// 펫 ID 조회 (currentPet → 배열 인덱스)
int  petId_for_current(void);


bool research_pet(int amt);
#endif // PET_MANAGER_H
