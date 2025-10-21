// src/console_ui.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "console_ui.h"
#include "map_manager.h"
#include "pet_manager.h"
#include "quest_manager.h"
#include "inventory_manager.h"
#include "monster_manager.h"
#include "shop_manager.h"
#include "save_load.h"
#include "battle_manager.h"

extern int playerX, playerY;
extern int playerGold;
extern Pet *currentPet;
extern Monster currentMonster;  // 전투 중 몬스터 상태 접근용


// // 전역 퀘스트 배열 및 남은 진행량 (quest_manager.h 에서 선언된 대로 포인터)
// extern Quest *quests_global;
// extern int *questRemaining;


#define MAX_SAVE_SLOTS 5
static bool menu_active = false;
static int menu_choice = 0;
static struct tm save_times[MAX_SAVE_SLOTS];
static int save_count = 0;

// 화살표 및 기타 키 입력 처리
static int get_input(void) {
    int ch = getchar();
    if (ch == '\033') {
        getchar(); // '['
        ch = getchar(); // 'A','B','C','D'
    }
    return ch;
}


void init_ui(void) {
    // 터미널 설정 필요시 구현
}


// 퀘스트, 인벤토리 조회 함수
extern Quest* get_current_quest(void);
extern int    get_item_count(int itemId);


// HUD 통합: 펫 상태(HP/SP/레벨/XP) 하단에 표시
void render_hud(void) {
    const char *C_BORDER = "\033[1;32m";
    const char *C_RESET  = "\033[0m";
    const char *C_HP = "\033[1;31m";
    const char *C_SP = "\033[1;34m";
    const char *C_XP = "\033[1;92m";
    const int barW = 10;

    printf("%s│ Name: %-9s %s Lv%3d  │  ATK:%4d   DEF:%4d   AGI:%4d    │%s\n",
           C_BORDER,
           currentPet->name,
           currentPet->emoji,
           currentPet->level,
           currentPet->atk, currentPet->def, currentPet->agi,
           C_RESET);

    // 펫 HP/SP 바
    int hp    = currentPet->hp,    maxHp = currentPet->maxHp;
    int sp    = currentPet->mp,    maxSp = currentPet->maxMp;
    int hpB   = hp * barW / maxHp;
    int spB   = sp * barW / maxSp;
    printf("%s│", C_BORDER);
    printf("%s ", C_RESET);
    printf("%sHP ", C_HP);
    for (int i = 0; i < hpB; i++) printf("▇▇▇▇");
    for (int i = hpB; i < barW; i++) printf("░░░░");
    printf("%s (%5d/%5d)     %s", C_BORDER, hp, maxHp, C_RESET);
    printf("%s│\n", C_BORDER);
    printf("│ %s", C_RESET);
    printf("%sSP ", C_SP);
    for (int i = 0; i < spB; i++) printf("▇▇▇▇");
    for (int i = spB; i < barW; i++) printf("░░░░");
    printf("%s (%5d/%5d)     %s", C_BORDER, sp, maxSp, C_RESET);
    printf("%s│", C_BORDER);
    printf("%s\n", C_RESET);

    // XP 바
    int xp    = currentPet->xp;
    int xpMax = currentPet->level * 100;
    int xpB   = xp * barW / xpMax;
    printf("%s│ XP %s", C_BORDER, C_XP);
    for (int i = 0; i < xpB; i++) printf("▇▇▇▇");
    for (int i = xpB; i < barW; i++) printf("░░░░");
    printf("%s %s(%5d/%5d)     │%s\n",C_XP, C_BORDER, xp, xpMax, C_RESET);
        

    printf("%s└───────────────────────────────────────────────────────────────┘\n", C_BORDER);

        // 추가: 오른쪽에 퀘스트 목록을 그려주기
    // totalW 는 draw_map 에서 한 문자당 2칸 너비로 사용했다고 가정한 맵 너비

                  
\

}

// ────────────────────────────────────────────────────────
// 전체 UI 그리기: 컬러 헤더 → 맵 → 엔티티 → 구분선 → HUD
// ────────────────────────────────────────────────────────
void draw_ui(int px, int py) {
    // 화면 전체 클리어 및 초기화
    printf("\033[H\033[J");
    // 컬러 코드
    const char *C_TITLE  = "\033[1;32m";
    const char *C_RESET  = "\033[0m";
    const char *C_BORDER = "\033[1;32m";


    // 1) 컬러 헤더 바
    char header[128];
    snprintf(header, sizeof(header),
        " MAP:%-10s Lv[%2d ~ %2d] ",
        get_current_map_name(),
        get_map_level_min(), get_map_level_max()
    );
    int totalW = WIDTH / 2;  // draw_map 에서 한 문자당 2칸 너비를 쓴다고 가정
    int hlen   = (int)strlen(header);
    // int padL   = (totalW - hlen) / 2;
    printf("%s┌───────────────────", C_TITLE);
    printf("%s%s%s", C_TITLE, header, C_RESET);
    printf("%s───────────────────┐\n", C_TITLE);

    // 2) 맵 + 엔티티
    draw_map(px, py);
    draw_npcs(px, py);
    draw_shops(px, py);
    draw_entities(px, py);

    //    ─── 3) NPC/상점 찍은 뒤, 커서를 맵 아래로 옮겨서 구분선을 찍음 ─────────────
    // 헤더 1줄 + 맵 HEIGHT 줄 → 구분선은 (HEIGHT+2)번째 줄의 1열에서
    printf("\033[%d;1H", HEIGHT + 1);


       // 3) 퀘스트 보드 — 여기서만!
    {
        int quest_col= 1;   // 맵 우측으로 5칸
        int quest_row= HEIGHT + 2;            // 헤더 바로 아래부터
        draw_quest_board_ui(quest_row, quest_col);
    }
    // 3) 구분선
    printf("%s┌───────────────────────────────────────────────────────────────┐\n", C_TITLE);

    // 5) Gold / XP / Quest

    const int stoneIds[] = {
        ITEM_STONE_DUNGEON1,
        ITEM_STONE_DUNGEON2,
        ITEM_STONE_DUNGEON3,
        ITEM_STONE_DUNGEON4,
        ITEM_STONE_DUNGEON5,
        ITEM_STONE_DUNGEON6,
        ITEM_STONE_DUNGEON7
    };
    int stoneCount = 0;
    for (int i = 0; i < 7; i++) {
        stoneCount += get_item_count(stoneIds[i]);
    }
    // int    xp = currentPet->xp, xpMax = currentPet->level * 100;
    Quest *q = get_current_quest();
    // 퀘스트 진행도 계산용 변수 선언
    int done = 0, total = 0;
    if (q && q->target > 0) {
        done  = q->target - questRemaining[q->id - 1];
        total = q->target;
    }
    printf("%s│ Gold:%10d                | Stones:%3d                   │%s\n",
        C_BORDER, playerGold,
        stoneCount,
        C_RESET
    );

    // 6) Potion Counts
    int hL = get_item_count(ITEM_HP_LOW),
        hM = get_item_count(ITEM_HP_MID),
        hH = get_item_count(ITEM_HP_HIGH),
        sL = get_item_count(ITEM_SP_LOW),
        sM = get_item_count(ITEM_SP_MID),
        sH = get_item_count(ITEM_SP_HIGH);
    printf("%s│ ITEMS || HP | L:%3d M:%3d H:%3d  | SP | L:%3d M:%3d H:%3d     │%s\n",
        C_BORDER, hL, hM, hH, sL, sM, sH, C_RESET
    );

    render_hud();

    // // 메뉴 활성화 시
    // if (menu_active) {
    //     printf("\n=== 메뉴 ===\n");
    //     printf("1) 저장  2) 불러오기  3) 보유 스킬 보기  4) 마을로 가기  5) 초기화면으로  6) 종료\n");
    //     printf("선택: ");
    //     int sel = getchar() - '0';
    //     while (getchar() != '\n');  // 남은 입력 버퍼 비우기

    //     switch (sel) {
    //         case 1:
    //             if (save_count < MAX_SAVE_SLOTS) {
    //                 save_times[save_count++] = *localtime(&(time_t){time(NULL)});
    //                 save_game();
    //                 printf("게임이 저장되었습니다.\n");
    //             } else {
    //                 printf("저장 슬롯이 가득 찼습니다.\n");
    //             }
    //             break;
    //         case 2:
    //             if (load_game()) printf("세이브 데이터를 불러왔습니다.\n");
    //             else             printf("저장된 게임이 없습니다.\n");
    //             break;
    //         case 3:
    //             show_skill_menu();
    //             break;
    //         case 4:
    //             load_map(0);  // 마을로 이동
    //             break;
    //         case 5:
    //             exit(0);      // 초기화면으로 (프로그램 종료)
    //             break;
    //         case 6:
    //             printf("게임을 종료합니다.\n");
    //             exit(0);
    //             break;
    //         default:
    //             break;
    //     }
    //     refresh_ui();
    //     menu_active = false;

    // }
}

// 전투나 상점 이용 중 UI를 즉시 갱신하는 헬퍼
void refresh_ui(int px, int py) {
    printf("\033[H\033[J");
    draw_ui(px, py);
}

// ───────────────────────────────────────────────────────────────────
// 전투 중 UI 즉시 갱신 및 몬스터 상태창 표시
// ───────────────────────────────────────────────────────────────────

void draw_battle_ui(void) {
    const char *C_MONSTER = "\033[1;31m";
    const char *C_RESET   = "\033[0m";
    const int barW = 20;

    // 기본 UI 먼저 그리기
    refresh_ui(playerX, playerY);

    // 몬스터 상태창 오버레이 (하단 위치)
    int row = HEIGHT + 13;
    printf("\033[%d;1H", row);
    printf("%s== 전투: 몬스터 %s %s ==%s\n", C_MONSTER,
           currentMonster.emoji, currentMonster.name, C_RESET);
    int mhp  = currentMonster.hp;
    int mmax = currentMonster.maxHp;
    int mBar = mhp * barW / mmax;
    printf("%sHP ", C_MONSTER);
    for (int i = 0; i < mBar; i++) printf("█");
    for (int i = mBar; i < barW; i++) printf("░");
    printf(" (%3d/%3d)%s\n", mhp, mmax, C_RESET);
}

// ───────────────────────────────────────────────────────────────────
// 상점 이용 중 UI 즉시 갱신
// ───────────────────────────────────────────────────────────────────
void draw_shop_ui(void) {
    extern int playerX, playerY;
    refresh_ui(playerX, playerY);
}



bool handle_menu_input(void) {
    int c = get_input();
    if (c=='m' || c=='M') {
        menu_active = !menu_active;
        return true;
    }
    return false;
}


void draw_quest_board_ui(int start_row, int start_col) {
    int row = start_row;
    printf("\033[%d;%dH ▷▷▷ 퀘스트 목록 ◁◁◁                                        \n", row++, start_col);
    for (int i = 0; i < MAX_QUESTS; ++i) {
        Quest *q = &quests[i];
        if (q->state == QSTATE_AVAILABLE || q->state == QSTATE_ACTIVE) {
            const char *s = q->state == QSTATE_ACTIVE ? "진행중" : "수락 가능";
            if (q->target > 0) {
                printf(" \033[%d;%dH ✔︎ %2d) [%s] %s  (%d / %d)    \n",
                       row, start_col,
                       q->id, s, q->title,
                       q->target - questRemaining[i],
                       q->target);
            } else {
                printf(" \033[%d;%dH ✔︎ %2d) [%s] %s          \n",
                       row, start_col,
                       q->id, s, q->title);
            }
            row++;
        }
    }
}
