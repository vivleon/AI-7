// src/battle_manager.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "battle_manager.h"
#include "monster_manager.h" // SpawnedMonster 정의 위해 필요
#include "pet_manager.h"
#include "console_ui.h"        // 새로 추가: UI 갱신 함수 선언
#include "inventory_manager.h"  // 물약 개수 확인
#include "quest_manager.h"
#include "shop_manager.h"
#include "map_manager.h" // get_current_map_index()
#include "save_load.h"
#include "final_boss.h"  
#include "monster_def.h"



extern Pet *currentPet;
extern int playerGold;
extern void gain_xp(int amount);

bool inBattle = false;
Monster currentMonster;
static SpawnedMonster *currentSpawn;
bool battleWon = false;





// void init_monsters(void) {
//     srand((unsigned)time(NULL));  // → 여기서만!
//     monCount = 0;
//}

// 1) 맵별 일반 몬스터 풀 (ID 1~30)
// 일반 몬스터 9개 + 보스 1개
extern int monsterPools[4][MAX_MONSTERS_PER_MAP];
extern int monsterPoolsFinal[];
extern const int FINAL_POOL_SIZE;

// 2) 몬스터 정의 (ID 1~30)
Monster monsterDefs[41] = {
    {0}, // dummy

    // --- map2: 고대 숲 일반 몬스터 ---
    { 1, "꿀떡이 슬라임",    "🟢", 30, 30,  5,  2,  8, false, 0 },
    { 2, "나뭇잎 푸딩",      "🌿", 35, 35,  6,  3,  7, false, 0 },
    { 3, "뿌리 도다리",      "🌱", 40, 40,  7,  4,  6, false, 0 },
    { 4, "버섯 꼬마버섯",    "🗿", 45, 45,  8,  5,  5, false, 0 },
    { 5, "솔방울 수호자",    "🌲", 50, 50, 10,  6,  6, false, 0 },
    { 6, "개구리 풍뎅이",    "🐸", 32, 32,  4,  3, 10, false, 0 },
    { 7, "꽃잎 나비님",      "🦋", 28, 28,  3,  2, 12, false, 0 },
    { 8, "도토리 배지",      "🌰", 38, 38,  5,  4,  8, false, 0 },
    { 9, "오크 버섯이",      "🍄", 55, 55, 12,  6,  4, false, 0 },
    {10, "버섯대왕 버섯이",  "🎭", 180, 180, 20, 10, 10, true,  11 },

    // --- map3: 사막 사원 일반 몬스터 ---
    {11, "모래 미니렛",      "🐀", 140, 140,  8,  4,  7, false, 0 },
    {12, "사막 모래여우",    "🦊", 145, 145,  9,  5,  8, false, 0 },
    {13, "모래벌레 구더기",  "🐛", 150, 150, 10,  6,  6, false, 0 },
    {14, "낙타 행상인",      "🐫", 160, 160, 12,  7,  5, false, 0 },
    {15, "사원 파피루스",    "📜", 135, 135,  7,  3,  9, false, 0 },
    {16, "태양의 불뱀",      "🐍", 155, 155, 11,  8,  6, false, 0 },
    {17, "열기 램프",        "🪔", 148, 148,  9,  5,  7, false, 0 },
    {18, "모래 저승사자",    "🦂", 170, 170, 15, 10,  4, false, 0 },
    {19, "환영의 미라",      "🧟", 165, 165, 14,  9,  5, false, 0 },
    {20, "사막의 여왕 퍼플", "🌪️", 295, 295, 25, 20, 12, true, 21 },

    // --- map4: 설원 고성 일반 몬스터 ---
    {21, "눈송이 슬라임",    "❄️", 235, 235,  6,  3, 10, false, 0 },
    {22, "얼음 토끼",        "🐇", 240, 240,  7,  4, 11, false, 0 },
    {23, "빙하 곰돌이",      "🐻‍❄️",260, 260, 12,  8,  5, false, 0 },
    {24, "얼음정령 소환수",  "🌬️", 245, 245,  8,  5,  9, false, 0 },
    {25, "눈꽃 요정",        "❄️", 250, 250, 10,  6, 10, false, 0 },
    {26, "얼음 송어",        "🐟", 230, 230,  4,  3, 12, false, 0 },
    {27, "눈사람 쿠키",      "⛄", 255, 255, 11,  7,  6, false, 0 },
    {28, "빙결 늑대",        "🐺", 265, 265, 14,  9,  7, false, 0 },
    {29, "설원 왕거북",      "🐢", 275, 275, 18, 12,  4, false, 0 },
    {30, "블리자드 레아",    "👹",500,500,30, 25, 15, true, 31 },

    // map5 (준비된 최종보스 풀 예시로 31~40 예약)
    {31, "네메시스 그림자",  "🌑",750,750,40, 230, 320, true, 32 },
    {32, "네메시스 분노",    "🛡️",1999,1999,245, 235, 322, true, 33 },
    {33, "네메시스 폭풍",    "⛈️",3970,3970,350, 240, 324, true, 34 },
    {34, "네메시스 심판",    "⚖️",3000,3000,255, 245, 226, true, 35 },
    {35, "네메시스 파멸",    "💀",4200,4200,260, 250, 328, true, 36 },
    {36, "에테르 수호자",    "✨",2999,2999,199, 499, 399, true, 37 },
    {37, "암흑의 군주",      "🕸️",4200,4200,250, 240, 218, true, 38 },
    {38, "혼돈의 기운",      "🔮",4300,4300,255, 245, 220, true, 39 },
    {39, "종말의 예언자",    "📜",3840,3840,360, 350, 322, true, 40 },
    {40, "네메시스",        "👑",39999,39999,1450, 1450, 450, true, 0 }
};




// 던전 맵별 스톤 아이템 테이블
const int DUNGEON_STONE[5] = {
    0,
    ITEM_STONE_DUNGEON1,
    ITEM_STONE_DUNGEON2,
    ITEM_STONE_DUNGEON3,
    0
};

static Skill get_skill_by_id(int id) {
    // 간단 예시: id 매핑
    Skill s = {id, "Unknown", 0,0,false};
    // 실제 스킬 데이터는 별도 배열에서 로드
    return s;
}

void start_battle(int monsterId, struct SpawnedMonster *spawned) {
    inBattle  = true;
    battleWon = false;
    currentSpawn   = spawned;

    // 던전 재진입 시 플래그 리셋
    if (get_current_map_index() == MAP_FINAL_TEMPLE && !spawned) {
        finalBossSpawned = false;
    }


    // SpawnedMonster가 전달되면, 미리 위치된 몬스터 정보를 그대로 사용
    if (spawned) {
        currentMonster = spawned->base;
        currentSpawn   = spawned;
    }
    // 직접 ID 지정 시
    else if (monsterId >= 0) {
        currentMonster = monsterDefs[monsterId];
        currentSpawn   = NULL;
    }
    // 그 외: 드물게 fallback 랜덤 스폰
    else {
        int mapIdx = get_current_map_index();
        int idx = monsterPools[0][ rand() % MAX_MONSTERS_PER_MAP ];
        if (mapIdx >= 1 && mapIdx <= 3) {
            // 80% 일반, 20% 보스
            if (rand() % 100 < 80) {
                idx = monsterPools[mapIdx-1][ rand() % (MAX_MONSTERS_PER_MAP - 1) ];
            } else {
                idx = monsterPools[mapIdx-1][ MAX_MONSTERS_PER_MAP - 1 ];
            }
        } else if (mapIdx == 4) {
            idx = monsterPoolsFinal[ FINAL_POOL_SIZE - 1 ];
        }
        currentMonster = monsterDefs[idx];
        currentSpawn   = NULL;
    }

    printf("\n== 전투 시작! 몬스터 등장: %s %s ==\n",
           currentMonster.emoji, currentMonster.name);
}

bool in_battle(void) {
    return inBattle;
}



void process_battle_turn(void) {
    // 1) 플레이어 턴
    draw_battle_ui();
    printf("\n");
    printf("\n[턴제 전투] 내 턴\n");
    printf("1) 스킬  2) 물약  3) 도망\n선택: ");
    int ch; scanf("%d",&ch);
    switch (ch) {
        case 1: {
            // 스킬 선택 메뉴 출력
            if (currentPet->skills.skillCount == 0) {
                printf("스킬을 보유하고 있지 않습니다. 기본 공격 실행\n");
                int dmg = currentPet->atk - currentMonster.def;
                if (dmg < 1) dmg = 1;
                currentMonster.hp -= dmg;
                printf("\n기본 공격!\n 몬스터에 %d 데미지\n", dmg);
                break;
            }
            printf("=== 스킬 목록 ===\n");
            for (int i = 0; i < currentPet->skills.skillCount; i++) {
                Skill sk = currentPet->skills.skills[i];
                printf(" %d) %s (Power:%d, SP:%d)%s\n",
                       i+1, sk.name, sk.power, sk.spCost,
                       sk.isUnique ? " [U]" : "");
            }
            printf("선택 (숫자): ");
            int si; scanf("%d", &si);
            if (si < 1 || si > currentPet->skills.skillCount) {
                printf("잘못된 선택입니다. 턴을 넘깁니다.\n");
                getchar();
                break;
            }
            Skill sk = currentPet->skills.skills[si-1];
            if (currentPet->mp < sk.spCost) {
                printf("SP가 부족합니다!\n");
                getchar();
                break;
            }
            currentPet->mp -= sk.spCost;
            int dmg = sk.power + currentPet->atk - currentMonster.def;
            if (dmg < 1) dmg = 1;
            currentMonster.hp -= dmg;
            printf("\n스킬 '%s' 사용!\n 몬스터에 %d 데미지\n", sk.name, dmg);
            getchar();
        } break;

        case 2: {
            // 물약 사용
            printf("1) HP 물약  2) SP 물약\n선택: ");
            int pi; scanf("%d",&pi);
            if(pi==1)
            {
                printf("1) HP 하급 2) HP 중급 3) HP 상급");
                int hp_potion; scanf("%d",&hp_potion);
                if(pi==1 && hp_potion== 1 && use_item(ITEM_HP_LOW))
                {
                    currentPet->hp = currentPet->hp + 30;
                    if(currentPet->hp > currentPet->maxHp)
                    {
                        currentPet->hp = currentPet->maxHp;
                    }
                    printf("하급 HP 물약 사용\nHP 30 회복!\n");
                    // 즉시 UI 갱신: HP/SP, 물약 개수 업데이트
                    getchar();
                    draw_battle_ui();
                }
                if(pi==1 && hp_potion== 2 && use_item(ITEM_HP_MID))
                {
                    currentPet->hp = currentPet->hp + 50;
                    if(currentPet->hp > currentPet->maxHp)
                    {
                        currentPet->hp = currentPet->maxHp;
                    }
                    printf("중급 HP 물약 사용\nHP 50 회복!\n");
                    // 즉시 UI 갱신: HP/SP, 물약 개수 업데이트
                    getchar();
                    draw_battle_ui();
                }
                if(pi==1 && hp_potion== 3 && use_item(ITEM_HP_HIGH))
                {
                    currentPet->hp = currentPet->hp + 100;
                    if(currentPet->hp > currentPet->maxHp)
                    {
                        currentPet->hp = currentPet->maxHp;
                    }
                    printf("상급 HP 물약 사용\nHP 100 회복!\n");
                    // 즉시 UI 갱신: HP/SP, 물약 개수 업데이트
                    getchar();
                    draw_battle_ui();
                }
            }
            else if(pi==2)
            {
                printf("1) SP 하급 2) SP 중급 3) SP 상급");
                int mp_potion; scanf("%d",&mp_potion);
                if(pi==1 && mp_potion== 1 && use_item(ITEM_SP_LOW))
                {
                    currentPet->mp = currentPet->mp + 10;
                    if(currentPet->mp > currentPet->maxMp)
                    {
                        currentPet->mp = currentPet->maxMp;
                    }
                    printf("하급 SP 물약 사용\nSP 30 회복!\n");
                    getchar();
                    draw_battle_ui();
                }
                if(pi==1 && mp_potion== 2 && use_item(ITEM_SP_MID))
                {
                    currentPet->mp = currentPet->mp + 20;
                    if(currentPet->mp > currentPet->maxMp)
                    {
                        currentPet->mp = currentPet->maxMp;
                    }
                    printf("중급 SP 물약 사용\nSP 50 회복!\n");
                    getchar();
                    draw_battle_ui();
                }
                if(pi==1 && mp_potion== 3 && use_item(ITEM_SP_HIGH))
                {
                    currentPet->mp = currentPet->mp + 30;
                    if(currentPet->mp > currentPet->maxMp)
                    {
                        currentPet->mp = currentPet->maxMp;
                    }
                    printf("상급 SP 물약 사용\nSP 100 회복!\n");
                    getchar();
                    draw_battle_ui();
                }
            }
            else
            {
                printf("사용 불가\n");
            }
        } break;
        case 3: {
            if(rand()%100 < 80) {
                printf("도망 성공!\n");
                inBattle = false;
                return;
            } else {
                printf("도망 실패…\n");
            }
        } break;
        default: break;
    }

    // 2) 몬스터 턴
    if(currentMonster.hp>0) {
        printf("\n몬스터 턴!\n");
        int dmg = currentMonster.atk - currentPet->def;
        if(dmg<1) dmg=1;
        currentPet->hp -= dmg;
        printf("%s의 공격! 나의 펫에 %d 데미지\n",
               currentMonster.name, dmg);
        getchar();
        // 즉시 UI 갱신
        draw_battle_ui();
    }

    // 3) 전투 종료 체크
    if(currentMonster.hp <= 0) {
        printf("\n승리! 몬스터 격파!\n");
        inBattle = false;
        
        
        int gold = rand()%100 + 50;
        int xp   = rand()%50  + 15 * (currentPet->level);
        playerGold += gold;
        gain_xp(xp);
        printf("골드 +%d  XP +%d\n", gold,xp);
        
        

        // **스폰된 몬스터 제거**
        if (currentSpawn) currentSpawn->alive = false;

        if (get_current_map_index() == MAP_FINAL_TEMPLE) {
            // ─── 준보스(31~39) 처치 시 퀘스트 진행  ───
            
            int mapIdx = get_current_map_index();
            if (mapIdx == MAP_FINAL_TEMPLE
                    && currentMonster.isElite
                    && currentMonster.id >= 31
                    && currentMonster.id <= 39)
            {
                update_quest_progress(7, 1);
            }
            

            // // ─── 퀘스트7 완료 시 최종보스 스폰  ───
            // extern Quest quests[MAX_QUESTS];
            // if (quests[6].state == QSTATE_COMPLETED && !finalBossSpawned) {
            //     finalBossSpawned = true;
            //     printf("\n[DEBUG] 퀘스트7 완료 → 최종보스 스폰 중...\n");
            //     getchar();
            //     int finalId = 40;  // 네메시스 ID
            //     int fx = MAP_TOKENS_W/2, fy = HEIGHT/2;
            //     mons[monCount] = (SpawnedMonster){
            //         .x = fx, .y = fy,
            //         .base = monsterDefs[finalId],
            //         .skillCooldown = 3,
            //         .alive = true
            //     };
            //     mons[monCount].base.isElite = true;
            //     monCount++;
            //     printf("☠️ 최종 보스 '%s'가 제단 중앙에 등장! (좌표:%d,%d) ☠️\n\n",
            //        monsterDefs[finalId].name, fx, fy);
            //     getchar();
            // }
        }


        // 준보스 스톤 드롭 & 유니크 스킬
        if (currentMonster.isElite && rand()%100 < 50) {
            int stoneId = DUNGEON_STONE[ get_current_map_index() ];
            if (stoneId) {
                if (add_item(stoneId,1)) {
                    printf("스톤 드랍! ID:%d ×1 획득\n", stoneId);
                }
            }
            
        }

        // 예시: 10% 확률로 진화 돌 드랍
        if (rand() % 30== 0) {
            crystalCount++;
            printf("★ 진화의 돌을 획득했다! (총 %d개)\n", crystalCount);
        }

        if(currentMonster.isElite) {
            // 유니크 스킬 드롭 로직
        }

        // ───────────────────────────────────
        // 퀘스트 진행 업데이트
        // ───────────────────────────────────
        // 현재 활성 메인 퀘스트 가져오기
        Quest *mq = get_current_quest();
        if (mq && mq->type == QUEST_MAIN && mq->state == QSTATE_ACTIVE) {
            int qid    = mq->id;
            int mapIdx = get_current_map_index();
            // printf("[DEBUG] Active Quest ID=%d, mapIdx=%d, isElite=%d\n",
            //        qid, mapIdx, currentMonster.isElite);
            getchar();        
            switch (qid) {
                case 2:  // 첫 물약 구매
                    // 이 퀘스트는 battle에서가 아니라 shop 이용 후 update_quest_progress(2,0) 호출하도록 분리 권장
                    update_quest_progress(2, 0);
                    break;

                case 3:  // 고대숲 보스 토벌
                    if (mapIdx == 1 && currentMonster.isElite) {
                        update_quest_progress(3, 1);
                    }
                    break;

                case 4:  // 사막 몬스터 5마리 사냥
                    if (mapIdx == 2) {
                        update_quest_progress(4, 1);
                    }
                    break;

                case 5:  // 설원 스톤 3종 수집 (여기선 스톤 드랍 로직 후엔 수집 완료로 처리)
                    if (mapIdx == 3) {
                        update_quest_progress(5, 1);
                    }
                    break;

                case 6:  // 2차 진화 의식 (target==0)
                    update_quest_progress(6, 0);
                    break;

                case 7:  // 최종 보스 네메시스 처치
                    if (currentMonster.isElite && currentMonster.uniqueSkillId == 0) {
                        update_quest_progress(7, 1);
                    }
                    break;

                default:
                    // 기타 퀘스트 (예: side quest ID 11,13,15 등)도 이곳에 분기 추가
                    break;
            }
        }
        check_quest_completion();
        save_game();
        getchar();
        battleWon = true;
        
    }

    // ───────────────────────────────────
    // 전투 종료 체크: 펫 사망
    // ───────────────────────────────────
    if(currentPet->hp <= 0) {
        save_game();
        printf("\n패배… 회복실로 이동합니다.\n");
        getchar();
        inBattle = false;
        getchar();
        move_to_shop_heal();
    }
}
