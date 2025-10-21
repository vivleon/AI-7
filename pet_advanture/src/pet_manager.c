// src/pet_manager.c

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>            // usleep
#include <stdlib.h>            // rand, srand
#include <time.h>
#include "save_load.h"
#include "pet_manager.h"
#include "quest_manager.h"
#include "skill_manager.h"
#include "console_ui.h"     // draw_battle_ui()

Pet pets[MAX_PETS];
Pet *currentPet = NULL;
int playerGold = 0;
int crystalCount = 0;  // 획득한 스톤 수


// 내부 헬퍼: 펫 기본 정보 초기화
static void init_pet_data(void) {
    // Pet0: 공격형
    pets[0].id = 0;
    strcpy(pets[0].name, "구름이   ");
    strcpy(pets[0].emoji, "☁️ ");
    pets[0].level = 1; pets[0].xp = 0;
    pets[0].hp = pets[0].maxHp = 100;
    pets[0].mp = pets[0].maxMp = 50;
    pets[0].atk = 15; pets[0].def = 5; pets[0].agi = 10;
    pets[0].evolved1 = pets[0].evolved2 = false;
    pets[0].evoChoice = 0;
    pets[0].skills.skillCount = 0;
    
    // Pet1: 방어형
    pets[1].id = 1;
    strcpy(pets[1].name, "불꽃이  ");
    strcpy(pets[1].emoji, "🔥");
    pets[1].level = 1; pets[1].xp = 0;
    pets[1].hp = pets[1].maxHp = 110;
    pets[1].mp = pets[1].maxMp = 40;
    pets[1].atk = 10; pets[1].def = 15; pets[1].agi = 8;
    pets[1].evolved1 = pets[1].evolved2 = false;
    pets[1].evoChoice = 0;
    pets[1].skills.skillCount = 0;

    // Pet2: 회복형
    pets[2].id = 2;
    strcpy(pets[2].name, "방울이  ");
    strcpy(pets[2].emoji, "💧");
    pets[2].level = 1; pets[2].xp = 0;
    pets[2].hp = pets[2].maxHp = 90;
    pets[2].mp = pets[2].maxMp = 60;
    pets[2].atk = 12; pets[2].def = 6; pets[2].agi = 12;
    pets[2].evolved1 = pets[2].evolved2 = false;
    pets[2].evoChoice = 0;
    pets[2].skills.skillCount = 0;
}

// 게임 초기화: 펫, 자원 초기화
void init_game(void) {
    init_pet_data();
    currentPet = &pets[0];
    playerGold = 500;
    crystalCount = 0;
}
void select_pet(int petId) {
    if (petId < 0 || petId >= MAX_PETS) petId = 0;
    currentPet = &pets[petId];
    init_skills();       // 스킬 초기화
    
    // 기본 스킬 자동 습득: 펫별 4개 스킬 풀 전체 부여
    for (int i = 0; i < MAX_ACTIVE_SK; i++) {
        int sid = petSkillPool1[petId][i];
        if (sid <= 0) continue;
        learn_skill(sid);
    }
    
    printf("확인 후 [ENTER]\n");
    getchar();
}

void gain_xp(int amount) {
    if (!currentPet) return;
    currentPet->xp += amount;
    printf("XP +%d (현재 %d / %d)\n",
           amount,
           currentPet->xp,
           currentPet->level * 100);
    while (currentPet->xp >= currentPet->level * 100) {
        currentPet->xp -= currentPet->level * 100;
        currentPet->level++;
        currentPet->maxHp += 20;
        currentPet->hp = currentPet->maxHp;
        currentPet->maxMp += 10;
        currentPet->mp = currentPet->maxMp;
        currentPet->atk += 3;
        currentPet->def += 2;
        currentPet->agi += 1;
        printf("레벨업! Lv %d 🎉\n", currentPet->level);

        // 기존 레벨업 보상
        handle_level_up(currentPet->level, petId_for_current());

        // 레벨업 보상 및 자동 진화 시도
        if (!currentPet->evolved1 && try_evolve1()) {
            ; // 1차 진화 성공
        }
        else if (currentPet->evolved1 && !currentPet->evolved2 && try_evolve2()) {
            ; // 2차 진화 성공
        }
    }
}


// 진화 시 간단한 콘솔 애니메이션 효과
static void show_evolution_effect(void) {
    const char *frames[] = {
        "   ✦        ",
        "      ✦     ",
        "         ✦  ",
        "      ✦     "
    };
    for (int r = 0; r < 3; r++) {
        for (int i = 0; i < 4; i++) {
            printf("\r%s", frames[i]);
            fflush(stdout);
            usleep(200000);
        }
    }
    printf("\r             \r"); // Clear line
}

bool try_evolve1(void) {
    if (!currentPet || currentPet->evolved1) return false;
    if (currentPet->level >= 10 && crystalCount >= 1) {
        draw_battle_ui();
        // 이펙트
        printf("\n=== 1차 진화 시작 ===\n");
        show_evolution_effect();
        crystalCount--;
        currentPet->evolved1 = true;

        // 이름 변경 (펫별)
        switch (currentPet->id) {
            case 0: strcpy(currentPet->name, "하늘구름 "); break;   // 구름이 → 하늘구름
            case 1: strcpy(currentPet->name, "화염불꽃 "); break;   // 불꽃이 → 화염불꽃
            case 2: strcpy(currentPet->name, "푸른파도 ");   break;   // 물방울 → 청류물
        }

        printf("★ %s 가 1차 진화하여 '%s' 로!", currentPet->emoji, currentPet->name);
        printf(" 새로운 스킬 풀을 사용할 수 있습니다!\n\n");
        evolve1_choose_skills();
        return true;
    }
    return false;
}

bool try_evolve2(void) {
    if (!currentPet || !currentPet->evolved1 || currentPet->evolved2) return false;
   // 설원 스톤 수집 퀘스트(quest id 7) 완료 여부
    if (quests[5].state != QSTATE_COMPLETED) return false;
    if (currentPet->level >= 20 && crystalCount >= 3) {
        printf("\n=== 2차 진화 시작 ===\n");
        show_evolution_effect();
        crystalCount -= 3;
        currentPet->evolved2 = true;

        // 2차 진화 이름 분기
        int choice = 0;
        printf("2차 진화 선택: 1) 빛  2) 어둠  3) 균형 \n선택: ");
        scanf("%d", &choice);
        if (choice < 1 || choice > 3) choice = 1;

        // 이름 설정
        const char *newName;
        if (currentPet->id == 0) {
            // 구름 계열
            if      (choice == 1) newName = "천사구름 ";
            else if (choice == 2) newName = "검은구름 ";
            else                  newName = "천둥구름 ";
        }
        else if (currentPet->id == 1) {
            // 불꽃 계열
            if      (choice == 1) newName = "성염불꽃 ";
            else if (choice == 2) newName = "암염불꽃 ";
            else                  newName = "폭염불꽃 ";
        }
        else {
            // 물 계열
            if      (choice == 1) newName = "빛물방울 ";
            else if (choice == 2) newName = "깊은바다 ";
            else                  newName = "소용돌이 ";
        }
        strcpy(currentPet->name, newName);

        printf("★ %s 가 2차 진화하여 '%s' 로!", currentPet->emoji, currentPet->name);
        printf(" 진화 완료!\n\n");
        evolve2_choose_skills();
        return true;
    }
    return false;
}
int petId_for_current(void) {
    for (int i = 0; i < MAX_PETS; i++) {
        if (&pets[i] == currentPet) return i;
    }
    return 0;
}

bool research_pet(int amt) {
    extern int playerGold;
    if (playerGold < amt) return false;
    playerGold -= amt;
    printf("%d 골드를 지불하고 펫 연구를 진행했습니다.\n", amt);
    int inc = amt / 100;
    // 랜덤으로 스탯 강화 (예: ATK, DEF, AGI 중 하나 +1)
    int r = rand() % 3;
    if (r == 0) currentPet->atk += inc;
    else if (r == 1) currentPet->def += inc;
    else currentPet->agi += inc;
    return true;
}

void evolve1_choose_skills(void) {
    printf("1차 진화 스킬 풀 중 4개를 선택하세요:\n");
    // petSkillPool2: [petId][5]
    int petId = petId_for_current();
    const int *pool = petSkillPool2[petId];
    for (int i = 0; i < 5; i++) {
        Skill sk = create_skill(pool[i]);
        printf(" %d) %s (SP %d, Power %d)\n", i+1, sk.name, sk.spCost, sk.power);
    }
    printf("선택 (예: 1 2 3 4): ");
    int choices[4];
    for (int i = 0; i < 4; i++) scanf("%d", &choices[i]);
    printf("선택된 스킬 습득 중...\n");
    // 기존 스킬 재할당: 사용자에게 교체 요청
    printf("교체할 기존 스킬 슬롯 4개를 선택하세요 (1~%d): ", currentPet->skills.skillCount);
    int oldSlots[4];
    for (int i = 0; i < 4; i++) scanf("%d", &oldSlots[i]);
    // 제거
    for (int i = 0; i < 4; i++) {
        int idx = oldSlots[i] - 1;
        if (idx >= 0 && idx < currentPet->skills.skillCount) {
            // simple overwrite: move last skill into idx
            currentPet->skills.skills[idx] = currentPet->skills.skills[--currentPet->skills.skillCount];
        }
    }
    // 신규 습득
    for (int i = 0; i < 4; i++) {
        learn_skill(pool[choices[i]-1]);
    }
    printf("1차 진화 스킬 선택 완료!\n");
}

void evolve2_choose_skills(void) {
    printf("2차 진화 스킬 풀 중 4개를 선택하세요:\n");
    int petId = petId_for_current();
    const int *pool = petSkillPool3[petId];
    for (int i = 0; i < 5; i++) {
        Skill sk = create_skill(pool[i]);
        printf(" %d) %s (SP %d, Power %d)\n", i+1, sk.name, sk.spCost, sk.power);
    }
    printf("선택 (예: 1 2 3 4): ");
    int choices[4];
    for (int i = 0; i < 4; i++) scanf("%d", &choices[i]);
    printf("선택된 스킬 습득 중...\n");
    // 교체 로직 동일
    printf("교체할 기존 스킬 슬롯 4개를 선택하세요 (1~%d): ", currentPet->skills.skillCount);
    int oldSlots[4];
    for (int i = 0; i < 4; i++) scanf("%d", &oldSlots[i]);
    for (int i = 0; i < 4; i++) {
        int idx = oldSlots[i] - 1;
        if (idx >= 0 && idx < currentPet->skills.skillCount) {
            currentPet->skills.skills[idx] = currentPet->skills.skills[--currentPet->skills.skillCount];
        }
    }
    for (int i = 0; i < 4; i++) {
        learn_skill(pool[choices[i]-1]);
    }
    printf("2차 진화 스킬 선택 완료!\n");
}
