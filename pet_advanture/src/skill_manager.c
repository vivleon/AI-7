// src/skill_manager.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "skill_manager.h"
#include "pet_manager.h"    // currentPet, SkillSet 정의

extern Pet *currentPet;

// 펫 종류별 스킬 풀
const int petSkillPool1[3][5] = {
    {  1,  2,  3,  4,  0 },
    {  5,  6,  7,  8,  0 },
    {  9, 10, 11, 12,  0 }
};
const int petSkillPool2[3][5] = {
    { 13, 14, 15, 16,  0 },
    { 17, 18, 19, 20,  0 },
    { 21, 22, 23, 24,  0 }
};
const int petSkillPool3[3][5] = {
    { 25, 26, 27, 28,  0 },
    { 29, 30, 31, 32,  0 },
    { 33, 34, 35, 36,  0 }
};

// ID→Skill 매핑 헬퍼
Skill create_skill(int id) {
    Skill s = {id, "미정 스킬", 10, 1, false};
    switch (id) {
        // 펫0 기본
        case  1: s.name="회오리 일격";   s.power=10;  s.spCost=10; break;
        case  2: s.name="바람의 칼날";   s.power=15;  s.spCost=20; break;
        case  3: s.name="번개 강타";     s.power=25;  s.spCost=20; break;
        case  4: s.name="회오리 방패";   s.power=15;  s.spCost=10; break;
        // 펫1 기본
        case  5: s.name="파괴의 일격";   s.power=10;  s.spCost=10; break;
        case  6: s.name="불꽃 참격";     s.power=18;  s.spCost=20; break;
        case  7: s.name="화염 폭발";     s.power=25;  s.spCost=30; break;
        case  8: s.name="불꽃 방패";     s.power=18;  s.spCost=10; break;
        // 펫2 기본
        case  9: s.name="물대포";         s.power=10;  s.spCost=10; break;
        case 10: s.name="물의 방패";     s.power=10;  s.spCost=10; break;
        case 11: s.name="순풍 돌격";     s.power=23;  s.spCost=30; break;
        case 12: s.name="물보라";         s.power=25;  s.spCost=20; break;

        // 1차 진화
        case 13: s.name="사막 열풍";     s.power=30;  s.spCost=130; break;
        case 14: s.name="모래 폭풍";     s.power=35;  s.spCost=140; break;
        case 15: s.name="태양의 분노";   s.power=40;  s.spCost=150; break;
        case 16: s.name="화염 올가미";   s.power=28;  s.spCost=130; break;
        case 17: s.name="바람의 춤";     s.power=32;  s.spCost=130; break;
        case 18: s.name="모래 칼날";     s.power=38;  s.spCost=104; break;
        case 19: s.name="사막 칼날";     s.power=34;  s.spCost=140; break;
        case 20: s.name="태양의 검";     s.power=50;  s.spCost=161; break;
        case 21: s.name="버섯강타";       s.power=45;  s.spCost=130; s.isUnique=true; break;
        case 22: s.name="독버섯 구름";   s.power=40;  s.spCost=140; s.isUnique=true; break;
        case 23: s.name="버섯 폭발";     s.power=48;  s.spCost=150; s.isUnique=true; break;
        case 24: s.name="균사 파동";     s.power=42;  s.spCost=140; s.isUnique=true; break;

        // 2차 진화
        case 25: s.name="빛의 일격";     s.power=60;  s.spCost=250; break;
        case 26: s.name="성스러운 빛";   s.power=780;  s.spCost=260; break;
        case 27: s.name="신성한 광휘";   s.power=80;  s.spCost=270; break;
        case 28: s.name="축복의 빛";     s.power=65;  s.spCost=250; break;
        case 29: s.name="어둠의 칼날";   s.power=68;  s.spCost=250; break;
        case 30: s.name="그림자 일격";   s.power=78;  s.spCost=262; break;
        case 31: s.name="암흑 파동";     s.power=85;  s.spCost=270; break;
        case 32: s.name="어둠의 저주";   s.power=82;  s.spCost=260; break;
        case 33: s.name="균형의 파동";   s.power=72;  s.spCost=250; break;
        case 34: s.name="조화의 손길";   s.power=74;  s.spCost=250; break;
        case 35: s.name="우주의 균형";   s.power=78;  s.spCost=270; break;
        case 36: s.name="조화의 심판";   s.power=95;  s.spCost=280; break;

        // 전설 스킬
        case 99: s.name="에테르 심판";   s.power=150; s.spCost=300; break;
    }
    return s;
}

// 초기화: 보유·장착 스킬 모두 리셋
void init_skills(void) {
    currentPet->skills.skillCount = 0;
    for (int i = 0; i < MAX_ACTIVE_SK; i++) {
        currentPet->skills.activeSkills[i] = 0;
    }
}

// 신규 습득 또는 중복 강화
void learn_skill(int skillId) {
    SkillSet *ss = &currentPet->skills;
    // 중복 검사
    for (int i = 0; i < ss->skillCount; i++) {
        if (ss->skills[i].id == skillId) {
            ss->skills[i].power = (int)(ss->skills[i].power * 1.5);
            printf("스킬 중복 강화! [%s] Power→%d\n",
                   ss->skills[i].name, ss->skills[i].power);
            return;
        }
    }
    // 신규 추가
    if (ss->skillCount < MAX_SKILLS) {
        ss->skills[ss->skillCount++] = create_skill(skillId);
        printf("새 스킬 습득! [%s]\n",
               ss->skills[ss->skillCount-1].name);
    } else {
        printf("스킬 슬롯이 가득 찼습니다.\n");
    }
}

// 전투용 스킬 장착
bool equip_skill(int slot, int skillId) {
    if (slot < 0 || slot >= MAX_ACTIVE_SK) return false;
    SkillSet *ss = &currentPet->skills;
    for (int i = 0; i < ss->skillCount; i++) {
        if (ss->skills[i].id == skillId) {
            ss->activeSkills[slot] = skillId;
            printf("스킬 장착: Slot %d ← %s\n",
                   slot+1, ss->skills[i].name);
            return true;
        }
    }
    printf("해당 스킬을 보유하고 있지 않습니다.\n");
    return false;
}

// 스킬 메뉴 출력
void show_skill_menu(void) {
    SkillSet *ss = &currentPet->skills;
    printf("\n=== 스킬 메뉴 ===\n보유 스킬:\n");
    for (int i = 0; i < ss->skillCount; i++) {
        Skill s = ss->skills[i];
        printf(" %2d) %s (Power:%d SP:%d)%s\n",
               s.id, s.name, s.power, s.spCost,
               s.isUnique ? " [U]" : "");
    }
    printf("장착 슬롯:\n");
    for (int i = 0; i < MAX_ACTIVE_SK; i++) {
        int sid = ss->activeSkills[i];
        const char *nm = sid ? create_skill(sid).name : "빈 슬롯";
        printf(" %d) %s\n", i+1, nm);
    }
}

// 레벨업 보상 스킬 드롭
void handle_level_up(int level, int petId) {
    srand((unsigned)time(NULL));
    const int (*pool)[5] = petSkillPool1;
    if      (level >= 20) pool = petSkillPool3;
    else if (level >= 10) pool = petSkillPool2;

    int sid = pool[petId][rand() % 50];
    if (sid) learn_skill(sid);
}

// 유니크 스킬 드롭
void handle_unique_drop(int skillId) {
    srand((unsigned)time(NULL));
    if (rand() % 100 < 25) {
        printf("유니크 스킬 드롭 기회!\n");
        learn_skill(skillId);
    }
}
