// include/skill_manager.h
#ifndef SKILL_MANAGER_H
#define SKILL_MANAGER_H

#include <stdbool.h>

#define MAX_SKILLS     8    // 보유 가능한 최대 스킬 수
#define MAX_ACTIVE_SK  4    // 전투 시 장착 가능한 스킬 슬롯 수

typedef struct {
    int         id;         // 스킬 고유 ID
    const char* name;       // 스킬 이름
    int         power;      // 스킬 위력
    int         spCost;     // 소모 SP
    bool        isUnique;   // 유니크 스킬 여부
} Skill;

typedef struct {
    Skill skills[MAX_SKILLS];
    int   skillCount;           // 현재 보유 스킬 수
    int   activeSkills[MAX_ACTIVE_SK]; // 전투에 장착된 스킬 ID 목록
} SkillSet;

// 펫 종류별 스킬 풀 (petId 0,1,2 × 진화 단계 3단계, 각 5스킬)
extern const int petSkillPool1[3][5];
extern const int petSkillPool2[3][5];
extern const int petSkillPool3[3][5];

// 함수 프로토타입
void init_skills(void);
void learn_skill(int skillId);
bool equip_skill(int slot, int skillId);
void show_skill_menu(void);
void handle_level_up(int level, int petId);
void handle_unique_drop(int skillId);
// 스킬 ID에 해당하는 스킬 정보를 반환
Skill create_skill(int id);

#endif // SKILL_MANAGER_H
