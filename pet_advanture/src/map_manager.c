// src/map_manager.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "map_manager.h"
#include "monster_manager.h"
#include "npc_manager.h"
#include "inventory_manager.h" 
#include "shop_manager.h"
#include "pet_manager.h"    // currentPet 선언을 위해

// ─── 제단(altar)에 올라갈 때만 최종 보스 전투 진입 ───
#include "final_boss.h"
#include "battle_manager.h"   // finalBossSpawned


extern Pet *currentPet;     // 전역 펫 포인터

// 다음 두 헬퍼는 아래에 정의될 것이므로, 미리 프로토타입만 선언
int get_map_level_min_for_index(int idx);
int get_map_level_max_for_index(int idx);

#define MAP_COUNT 5 

// 프로토타입 선언: 외부에 정의된 메뉴 함수
void draw_intro_screen(void);
int  draw_title_menu(void);
int  draw_pet_menu(void);

// 외부 함수 선언
extern bool save_game(void);
extern bool load_game(void);
extern void show_skill_menu(void);
extern void draw_intro_screen(void);
extern int  draw_title_menu(void);
extern int  draw_pet_menu(void);
extern void select_pet(int petId);



// ───────────────────────────────────────────────────────────────────
//  단일 키 입력 처리: 화살표 키 ESC-[A/B/C/D → w/s/d/a 로 매핑
// ───────────────────────────────────────────────────────────────────
static int get_input(void) {
    struct termios oldt, newt;
    unsigned char c, seq[2];
    // 터미널 설정 백업
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    // 비칸논 모드 + 에코 비활성
    newt.c_lflag &= ~(ICANON | ECHO);
    // 최소 1바이트, 타임아웃 없음
    newt.c_cc[VMIN]  = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // 첫 바이트 읽기
    if (read(STDIN_FILENO, &c, 1) != 1) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return 0;
    }

    // ESC(0x1b)가 아니면 바로 복원 후 반환
    if (c != 0x1b) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return c;
    }

    // ESC 시퀀스: '[' 와 방향 문자를 추가로 읽는다
    if (read(STDIN_FILENO, &seq[0], 1) != 1 ||
        read(STDIN_FILENO, &seq[1], 1) != 1) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return 0;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    if (seq[0] == '[') {
        switch (seq[1]) {
            case 'A': return 'w';  // 위
            case 'B': return 's';  // 아래
            case 'C': return 'd';  // 오른쪽
            case 'D': return 'a';  // 왼쪽
        }
    }
    return 0;
}

// 맵 데이터 (1:벽,0:통로,2:이벤트,4:포탈)
char map1[HEIGHT][WIDTH+1] = {
    "PINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINE4   PINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPALMWATRWATRWATR",     //1
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   0   DRG20   0   0   0   0   0   0   0   0   0   0   PALMWATRSQIDWATRWATR",     //2
    "PINE0   0   0   0   0   0   0   0   CAST0   0   0   0   0   SIGN0   0   0   0   0   0   0   0   0   0   0   PALMWATRWATRBOATWATR",     //3
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRWATRWATRWATR",     //4
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATR5   WATRWATR",     //5
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRWATRWATRWATR",     //6 
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRFISHWATRWATR",     //7 
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRWATRSHIPWATRWATR",     //8
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRWATRWATRWATR5   WATR",     //9
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRWATRWATRWATR5   WATR",     //10
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRWATRWATRWATRWATRWATR",     //11
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRWATRWATRWATRWATRWATR",     //12
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   FOUN0   0   0   0   0   0   0   0   0   0   PALMWATRWATRWATRWATRWATRWATR",     //13
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   FOUNFOUNFOUN0   0   0   0   0   0   0   0   0   PALMWATRWATRWATRBOATPALMWATR",     //14
    "PINESIGN0   0   0   0   0   0   0   0   0   0   0   0   FOUN0   0   0   0   0   0   0   0   0   0   0   PALMWATRPALMPALMSIGNPALM",     //15
    "4   DRG10   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRPALM0   BOSS4   ",     //16
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRPALM0   PALMPALM",     //17
    "PINE0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMDOCKPALM0   PALMWATR",     //18
    "PINEFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOW0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRPALM0   PALMWATR",     //19
    "PINEFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOW0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRPALM0   PALMWHA ",     //20
    "PINEFLOWFLOWFLOWFLOWHSE2HSE2HSE3HSE1FLOW0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMPALM0   PALMWATR",     //21
    "PINEFLOWFLOWFLOWFLOWHSE1HSE1FLOWFLOWFLOWFLOWINN 0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATR",     //22
    "PINEFLOWFLOWFLOWFLOWHSE1HSE1FLOWFLOWFLOWFLOWFLOW0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMPALMPALMPALMWATR",     //23
    "PINEFLOWFLOWFLOWFLOWHSE1HSE1FLOWFLOWFLOWFLOWFLOW0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRFISHWATRWATR",     //24
    "PINEFLOWFLOWFLOWFLOWHSE1HSE1FLOWFLOWFLOWFLOWFLOWELDR0   0   0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRWATRWATRWATR",     //25
    "PINEFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWHSE1FLOW0   0   0   0   0   MARK0   0   0   0   0   0   0   PALMWATRWATRBOATWATR",     //26
    "PINEFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWHSE10   0   0   0   0   0   0   0   0   0   0   ELFS0   0   PALMWATRWATRWATR5   ",     //27
    "PINEFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWWIZDFLOWFLOWFLOWHSE10   0   0   0   0   0   0   0   0   0   0   0   PALMWATRWATRWATRWATR",     //28
    "PINEFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWHSE30   0   SIGN0   0   0   0   0   0   0   0   0   0   0   0   PALMWATRWATRWATR",     //29
    "PINEFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOWFLOW0   0   0   DRG30   0   0   0   0   0   0   0   0   0   0   0   PALMWATRWATRWATR",     //30
    "PINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINE4   PINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPALMWATRWATR"      //31
}; 
//   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31  32


char map2[HEIGHT][WIDTH+1] = {
    "PINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINE",         //1
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   TREETREEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   TREETREEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   TREETREEG   2   2   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   TREETREEG   G   PINE", 
    "PINEG   TREETREEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   TREETREEG   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   TREEG   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   TREEG   G   G   G   G   G   TREEG   G   G   TREETREEG   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   TREETREEG   G   G   G   G   TREEG   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   TREEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "4   SAFEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   DRG24   ",         //16
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   TREEG   TREEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   TREETREEG   G   G   G   G   G   G   G   G   TREEG   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   G   TREEG   G   G   G   G   G   TREETREEG   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   TREEG   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   TREEG   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   TREEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   TREETREEG   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   TREEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   TREEG   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE",         //25
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   TREEG   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   TREEG   G   G   G   G   TREEG   G   G   G   G   G   G   G   G   G   G   TREETREEG   G   PINE", 
    "PINEG   G   G   TREETREETREEG   TREETREEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   TREEG   G   PINE", 
    "PINEG   G   TREETREEG   G   G   TREEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEG   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   PINE", 
    "PINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINEPINE"          //31
}; 
//   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31  32


char map3[HEIGHT][WIDTH+1] = {
    "PALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALM4   PALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALM",     //1
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   SAFE6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //2
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //3
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //4
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   5   5   5   5   5   5   5   6   6   6   6   6   6   6   PALM",     //5
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   5   WATRWATRWATRWATR5   5   6   6   6   6   6   6   6   PALM",     //6 
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   5   WATRWATRWATRWATR5   5   6   6   6   6   6   6   6   PALM",     //7 
    "PALM6   6   6   6   5   5   5   5   5   6   6   6   6   6   6   6   5   5   5   5   5   5   5   6   6   6   6   6   6   6   PALM",     //8
    "PALM6   6   6   6   5   WATRWATRWATR5   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //9
    "PALM6   6   6   6   5   WATRWATRWATR5   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //10
    "PALM6   6   6   6   5   5   5   5   5   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //11
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //12
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //13
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //14
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //15
    "4   DRG16   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   DRG34   ",     //16
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //17
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //18
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //19
    "PALM6   6   6   6   5   5   5   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //20
    "PALM6   6   6   6   5   WATR5   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //21
    "PALM6   6   6   6   5   5   5   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //22
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   5   5   5   5   5   6   6   6   6   6   6   6   6   6   6   6   PALM",     //23
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   5   WATRWATRWATR5   6   6   6   6   6   6   6   6   6   6   6   PALM",     //24
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   5   WATRWATRWATR5   6   6   6   6   6   6   6   6   6   6   6   PALM",     //25
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   5   WATRWATRWATR5   6   6   6   6   6   6   6   6   6   6   6   PALM",     //26
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   5   5   5   5   5   6   6   6   6   6   6   6   6   6   6   6   PALM",     //27
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //28
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //29
    "PALM6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   6   PALM",     //30
    "PALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALMPALM"      //31
}; 
//   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31  32


char map4[HEIGHT][WIDTH+1] = { 
    "MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN 4   MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN ",     //1
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   SAFEW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //2
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //3
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //4
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //5
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //6 
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //7 
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //8
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //9
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //10
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //11
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //12
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //13
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //14
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //15
    "4   DRG1W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //16
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //17
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //18
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //19
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //20
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //21
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //22
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //23
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //24
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //25
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //26
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //27
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //28
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //29
    "MTN W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   MTN ",     //30
    "MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN MTN "      //31
}; 
//   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31  32



char map5[HEIGHT][WIDTH+1] = { 
    "WHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBL4   WHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBL",     //1
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   SAFEW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //2
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //3
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //4
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //5
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //6 
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //7 
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //8
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //9
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //10
    "WHBLW   W   W   W   W   W   W   W   W   BLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHW   W   W   W   W   W   W   W   W   WHBL",     //11
    "WHBLW   W   W   W   W   W   W   W   W   BLWH0   0   0   0   0   0   0   0   0   0   BLWHW   W   W   W   W   W   W   W   W   WHBL",     //12
    "WHBLW   W   W   W   W   W   W   W   W   BLWH0   0   0   W   W   W   W   0   0   0   BLWHW   W   W   W   W   W   W   W   W   WHBL",     //13
    "WHBLW   W   W   W   W   W   W   W   W   BLWH0   0   W   W   W   W   W   W   0   0   BLWHW   W   W   W   W   W   W   W   W   WHBL",     //14
    "WHBLW   W   W   W   W   W   W   W   W   BLWH0   W   W   W   W   W   W   W   W   0   BLWHW   W   W   W   W   W   W   W   W   WHBL",     //15
    "WHBLW   W   W   W   W   W   W   W   W   BLWH0   W   W   W   W   W   W   W   W   0   BLWHW   W   W   W   W   W   W   W   W   WHBL",     //16
    "WHBLW   W   W   W   W   W   W   W   W   BLWH0   W   W   W   W   W   W   W   W   0   BLWHW   W   W   W   W   W   W   W   W   WHBL",     //17
    "WHBLW   W   W   W   W   W   W   W   W   BLWH0   W   W   W   W   W   W   W   W   0   BLWHW   W   W   W   W   W   W   W   W   WHBL",     //18
    "WHBLW   W   W   W   W   W   W   W   W   BLWH0   0   W   W   W   W   W   W   0   0   BLWHW   W   W   W   W   W   W   W   W   WHBL",     //19
    "WHBLW   W   W   W   W   W   W   W   W   BLWH0   0   0   0   W   W   0   0   0   0   BLWHW   W   W   W   W   W   W   W   W   WHBL",     //20
    "WHBLW   W   W   W   W   W   W   W   W   BLWH0   0   0   0   W   W   0   0   0   0   BLWHW   W   W   W   W   W   W   W   W   WHBL",     //21
    "WHBLW   W   W   W   W   W   W   W   W   BLWHBLWHBLWHBLWHBLWHBLWHW   BLWHBLWHBLWHBLWHBLWHW   W   W   W   W   W   W   W   W   WHBL",     //22
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //23
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //24
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //25
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //26
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //27
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //28
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //29
    "WHBLW   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   W   WHBL",     //30
    "WHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBLWHBL"      //31
};  
//   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27  


// ───────────────────────────────────────────────────────────────────
//  맵 이름/레벨 테이블
// ───────────────────────────────────────────────────────────────────
static const char* mapNames[]    = { "리즈마을","고대숲","사막사원","설원고성", "제룬의 제단" };
static const int   mapLevelMin[] = { 1,         1,       6,          13,         18 };
static const int   mapLevelMax[] = {999,        999,      999,          999,         999 };



// ───────────────────────────────────────────────────────────────────
//  전역 포인터 초기화
// ───────────────────────────────────────────────────────────────────
char (*current_map)[WIDTH+1] = NULL;
static int petX, petY;


void init_maps(void) {
    current_map = map1;
    petX = petY = 0;  // 펫은 플레이어 출발 위치(0,0)에
    init_monsters();
    init_npcs();
    init_shops();
    load_map(0);
    printf("맵 전환: %s\n", get_current_map_name());

}


// static int getch(void) {
//     struct termios oldt, newt;
//     int ch;
//     tcgetattr(STDIN_FILENO, &oldt);
//     newt = oldt;
//     newt.c_lflag &= ~(ICANON | ECHO);
//     newt.c_cc[VMIN] = 1;                     // 최소 입력 문자 수를 1로 설정
//     newt.c_cc[VTIME] = 0;                    // 최소 읽기 대기 시간을 0으로 설정
//     tcsetattr(STDIN_FILENO, TCSANOW, &newt);
//     ch = getchar();
//     tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
//     return ch;
// }

// 맵 전환
void load_map(int mapIndex) {
    switch (mapIndex) {
        case 0: current_map = map1; break;
        case 1: current_map = map2; break;
        case 2: current_map = map3; break;
        case 3: current_map = map4; break;
        case 4: current_map = map5; break;
        default: current_map = map1; break;
    }

    //     // 맵 중앙으로 플레이어 위치 초기화
    // playerX = MAP_TOKENS_W / 2;
    // playerY = HEIGHT      / 2;

    // // 펫은 플레이어 바로 뒤(바로 이전 위치)가 아니라, 동기화해서 중앙에
    // petX = playerX;
    // petY = playerY;

    // 새로운 맵에 진입하면 펫도 다시 출발점에
    petX = petY = 0;
    
    //새로운 맵에 진입하면 몬스터·보스 리스폰
    spawn_monsters_for_map(mapIndex);
    spawn_npcs_for_map(mapIndex);
    spawn_shops_for_map(mapIndex);
    printf("맵 전환: %s\n", get_current_map_name());
}


// ───────────────────────────────────────────────────────────────────
//  맵 그리기: 4문자 토큰 단위로 잘라서 → 고정폭 출력
//  플레이어(🧙) → 펫(🐲) → 타일 순서로 우선순위
// ───────────────────────────────────────────────────────────────────
void draw_map(int px, int py) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int tx = 0; tx < MAP_TOKENS_W; tx++) {
            int b = tx * 4;
            if      (tx == px && y == py) {
                // 플레이어
                printf("🧙");
            }
            else if (tx == petX && y == petY) {
                // 선택된 펫 이모지
                printf("%s", currentPet->emoji);
            }
            else {
                char tkn[5];
                memcpy(tkn, &current_map[y][b], 4);
                tkn[4] = '\0';

                // ───── 기본 구역 ─────
                if      (!strcmp(tkn, "1   ")) printf("🟫");   // 벽
                else if (!strcmp(tkn, "0   ")) printf("⬛");   // 통로
                else if (!strcmp(tkn, "2   ")) printf("🪨");   // 이벤트
                else if (!strcmp(tkn, "4   ")) printf("🌀");   // 포탈
                else if (!strcmp(tkn, "6   ")) printf("🏜️ ");    //사막 바닥
                else if (!strcmp(tkn, "5   ")) printf("🏝️ ");
                // ───── 컬러 블럭 ─────
                else if (!strcmp(tkn, "R   ")) printf("🟥");
                else if (!strcmp(tkn, "O   ")) printf("🟧");
                else if (!strcmp(tkn, "Y   ")) printf("🟨");
                else if (!strcmp(tkn, "G   ")) printf("🟩");
                else if (!strcmp(tkn, "B   ")) printf("🟦");
                else if (!strcmp(tkn, "P   ")) printf("🟪");
                else if (!strcmp(tkn, "W   ")) printf("⬜");

                // ───── 무기/방어 ─────
                else if (!strcmp(tkn, "WHBL")) printf("🔳");
                else if (!strcmp(tkn, "BLWH")) printf("🔲");
                else if (!strcmp(tkn, "KNIF")) printf("🔪");
                else if (!strcmp(tkn, "AXE ")) printf("🪓");
                else if (!strcmp(tkn, "GUN ")) printf("🔫");
                else if (!strcmp(tkn, "SHLD")) printf("🛡️ ");
                else if (!strcmp(tkn, "VEST")) printf("🦺");

                // ───── 자원/보물 ─────
                else if (!strcmp(tkn, "RING")) printf("💍");
                else if (!strcmp(tkn, "GEM ")) printf("💎");
                else if (!strcmp(tkn, "GOLD")) printf("💰");
                else if (!strcmp(tkn, "POTN")) printf("🧪");
                else if (!strcmp(tkn, "SMOK")) printf("🚬");

                // ───── 환경 ─────
                else if (!strcmp(tkn, "FOUN")) printf("⛲");
                else if (!strcmp(tkn, "RECY")) printf("♻️ ");
                else if (!strcmp(tkn, "SAFE")) printf("🔰");
                else if (!strcmp(tkn, "CNST")) printf("🚧");
                else if (!strcmp(tkn, "BONE")) printf("🦴");

                // ───── 자연/풍경 ─────
                else if (!strcmp(tkn, "MTN ")) printf("🗻");
                else if (!strcmp(tkn, "BRDG")) printf("🌁");
                else if (!strcmp(tkn, "SKY ")) printf("🌌");
                else if (!strcmp(tkn, "FIRE")) printf("🎇");
                else if (!strcmp(tkn, "SPRK")) printf("🎆");
                else if (!strcmp(tkn, "STAR")) printf("🌠");
                else if (!strcmp(tkn, "PALM")) printf("🌴");
                else if (!strcmp(tkn, "SEED")) printf("🌱");
                else if (!strcmp(tkn, "LEAF")) printf("🌿");
                else if (!strcmp(tkn, "CLVR")) printf("🍀");
                else if (!strcmp(tkn, "BAMB")) printf("🎍");
                else if (!strcmp(tkn, "WISH")) printf("🎋");
                else if (!strcmp(tkn, "LEFF")) printf("🍃");
                else if (!strcmp(tkn, "TREE")) printf("🌳");
                else if (!strcmp(tkn, "PINE")) printf("🌲");

                // ───── 바다/배 ─────
                else if (!strcmp(tkn, "SHIP")) printf("🚢");
                else if (!strcmp(tkn, "BOAT")) printf("⛵");
                else if (!strcmp(tkn, "SURF")) printf("🏄");
                else if (!strcmp(tkn, "WATR")) printf("🌊");
                else if (!strcmp(tkn, "WHA ")) printf("🐳");
                else if (!strcmp(tkn, "SQID")) printf("🦑");

                // ───── 표지판 ─────
                else if (!strcmp(tkn, "DANG")) printf("⚠️ ");
                else if (!strcmp(tkn, "DRG1")) printf("1️⃣ ");
                else if (!strcmp(tkn, "DRG2")) printf("2️⃣ ");
                else if (!strcmp(tkn, "DRG3")) printf("3️⃣ ");
                else if (!strcmp(tkn, "BOSS")) printf("☠️ ");
                else if (!strcmp(tkn, "DOCK")) printf("🛶");
                else if (!strcmp(tkn, "SIGN")) printf("🪧");
                else if (!strcmp(tkn, "FOUN")) printf("⛲");
                else if (!strcmp(tkn, "HEAD")) printf("👴");   // 이장
                else if (!strcmp(tkn, "ELDR")) printf("👵");   // 노인
                else if (!strcmp(tkn, "WIZD")) printf("🧙");   // 마법사
                else if (!strcmp(tkn, "ELFS")) printf("🧝");   // 엘프
                else if (!strcmp(tkn, "GUAR")) printf("🛡️");   // 경비
                else if (!strcmp(tkn, "MERH")) printf("🧑‍🌾"); // 상인
                else if (!strcmp(tkn, "SHOP")) printf("🏪");
                else if (!strcmp(tkn, "MARK")) printf("🎪");
                else if (!strcmp(tkn, "BASK")) printf("🧺");
                else if (!strcmp(tkn, "FOOD")) printf("🍞");
                else if (!strcmp(tkn, "FISH")) printf("🐟");
                else if (!strcmp(tkn, "HSE1")) printf("🏠");
                else if (!strcmp(tkn, "HSE2")) printf("🏡");
                else if (!strcmp(tkn, "HSE3")) printf("🏘️ ");
                else if (!strcmp(tkn, "CAST")) printf("🏰");
                else if (!strcmp(tkn, "CHUR")) printf("⛪");
                else if (!strcmp(tkn, "INN ")) printf("🏨");
                else if (!strcmp(tkn, "HUT ")) printf("🛖");
                else if (!strcmp(tkn, "FLOW")) printf("🌸");
                else printf("    ");  // 알 수 없는 경우 공백
            }
        }
        putchar('\n');
    }
}


// ───────────────────────────────────────────────────────────────────
//  입력 처리: WASD → 펫은 플레이어 “이전 위치”로 따라다님
// ───────────────────────────────────────────────────────────────────
void handle_map_input(int *px, int *py) {
    int ch = get_input();
    if (!ch) return;

    int nx = *px, ny = *py;
    if      (ch == 'w') ny--;
    else if (ch == 's') ny++;
    else if (ch == 'a') nx--;
    else if (ch == 'd') nx++;
    else return;

    if (nx < 0 || nx >= MAP_TOKENS_W || ny < 0 || ny >= HEIGHT) return;
    if (
    strncmp(&current_map[ny][nx*4], TILE_WALL_TOKEN, 4) != 0 &&
    strncmp(&current_map[ny][nx*4], TILE_PINE_TOKEN, 4) != 0 &&
    strncmp(&current_map[ny][nx*4], TILE_BLWH_TOKEN, 4) != 0 &&
    strncmp(&current_map[ny][nx*4], TILE_WHBL_TOKEN, 4) != 0 &&
    strncmp(&current_map[ny][nx*4], TILE_PALM_TOKEN, 4) != 0 &&
    strncmp(&current_map[ny][nx*4], TILE_MTN_TOKEN, 4) != 0
    ) {
        extern int petX, petY;
        petX = *px;
        petY = *py;
        *px  = nx;
        *py  = ny;
        // 이동 후 자동 저장
        save_game();
    }
}


// ───────────────────────────────────────────────────────────────────
//  이벤트/포탈 처리 (레벨 제한 추가)
// ───────────────────────────────────────────────────────────────────

void process_events(int px, int py) {
    

    // ─── 제단(altar)에 올라갈 때만 단 한 번 보스전 시작 ───

    if (get_current_map_index() == MAP_FINAL_TEMPLE
        && is_at_altar(px, py)
        && check_final_boss_entry()
        && !finalBossSpawned)
    {
        finalBossSpawned = true;
        enter_final_boss();
        return;
    }

    if (current_map[py][px*4] != TILE_PORTAL_TOKEN[0]) return;

    int cur = get_current_map_index();
    int target = -1;
    switch (cur) {
        case 0: // 마을
            if      (px == 0  && py == 15) target = 1;
            else if (px == 15 && py == 0)  target = 2;
            else if (px == 15 && py == 30) target = 3;
            else if (px == 31 && py == 15) target = 4;
            break;
        case 1: // 맵2
            if      (px == 0  && py == 15) target = 0;
            else if (px == 31 && py == 15) target = 2;
            break;
        case 2: // 맵3
            if      (px == 15 && py == 0)  target = 0;
            else if (px == 0  && py == 15) target = 1;
            else if (px == 31 && py == 15) target = 3;
            break;
        case 3: // 맵4
            if      (px == 15 && py == 0)  target = 0;
            else if (px == 0  && py == 15) target = 2;
            break;
        case 4: // 최종던전
            if (px == 15 && py == 0)       target = 0;
            break;
    }

    // switch (cur) {
    //     case 0: // 마을
    //         if      (px == 0 && py == HEIGHT/2)               target = 1;
    //         else if (px == MAP_TOKENS_W/2 && py == 0)         target = 2;
    //         else if (px == MAP_TOKENS_W/2 && py == HEIGHT-1)  target = 3;
    //         else if (px == MAP_TOKENS_W-1 && py == HEIGHT/2)  target = 4;
    //         break;
    //     case 1: // 맵2
    //         if      (px == 0 && py == HEIGHT/2)               target = 0;
    //         else if (px == MAP_TOKENS_W-1 && py == HEIGHT/2)  target = 2;
    //         break;
    //     case 2: // 맵3
    //         if      (px == MAP_TOKENS_W/2 && py == 0)         target = 0;
    //         else if (px == 0 && py == HEIGHT/2)               target = 1;
    //         else if (px == MAP_TOKENS_W-1 && py == HEIGHT/2)  target = 3;
    //         break;
    //     case 3: // 맵4
    //         if      (px == MAP_TOKENS_W/2 && py == 0)         target = 0;
    //         else if (px == 0 && py == HEIGHT/2)               target = 2;
    //         break;
    //     case 4: // 최종던전
    //         if (px == MAP_TOKENS_W/2 && py == 0)              target = 0;
    //         break;
    // }

    if (target < 0) return;

    // 레벨 제한 체크
    int minLv = get_map_level_min_for_index(target);
    int maxLv = get_map_level_max_for_index(target);
    if (currentPet->level < minLv) {
        printf("\033[1;31m레벨 %d 이상이어야 이 포탈을 이용할 수 있습니다!\033[0m\n", minLv);
        usleep(1000000);
        return;
    }

    if (currentPet->level > maxLv) {
        printf("\033[1;31m레벨 %d 이하만 이용할 수 있는 포탈입니다!\033[0m\n", maxLv);
        usleep(1000000);
        return;
    }

    // process_events() 안에서 포탈 타겟 결정 직후, load_map(target) 이전에
    // 각 던전 맵별로 해당 스톤이 있는지 검사
    int requiredStone = 0;
    switch (target) {
    case 2: requiredStone = ITEM_STONE_DUNGEON1; break;
    case 3: requiredStone = ITEM_STONE_DUNGEON2; break;
    case 4: requiredStone = ITEM_STONE_DUNGEON3; break;
    // 마을(0)이나 최종던전(4)은 스톤 불필요
    }
    if (requiredStone) {
    if (get_item_count(requiredStone) < 1) {
        printf("\033[1;31m해당 던전의 스톤이 필요합니다!\033[0m\n");
        usleep(1000000);
        return;
    }
    }

    load_map(target);
}


// ───────────────────────────────────────────────────────────────────
//  인덱스 기반 맵별 최소/최대 레벨 반환
// ───────────────────────────────────────────────────────────────────
int get_map_level_min_for_index(int idx) {
    static const int MIN_LV[] = { 1, 1, 6, 13, 18};
    if (idx < 0 || idx >= 5) idx = 0;
    return MIN_LV[idx];
}

int get_map_level_max_for_index(int idx) {
    static const int MAX_LV[] = { 999, 999, 999, 999, 999};
    if (idx < 0 || idx >= 5) idx = 0;
    return MAX_LV[idx];
}
// ───────────────────────────────────────────────────────────────────
//  HUD 헬퍼
// ───────────────────────────────────────────────────────────────────

static int idx(void) {
    if      (current_map == map1) return 0;
    else if (current_map == map2) return 1;
    else if (current_map == map3) return 2;
    else if (current_map == map4) return 3;
    else if (current_map == map5) return 4;
    return 0;
}

const char* get_current_map_name(void){ static const char* N[]={"리즈마을","고대숲","사막사원","설원고성","제룬의 제단"}; return N[idx()]; }
int get_map_level_min(void){ static int M[]={1,1,6,13,18}; return M[idx()]; }
int get_map_level_max(void){ static int M[]={999,999,999,999,999}; return M[idx()]; }
int get_current_map_index(void){ return idx(); }

extern const int DUNGEON_STONE[5];  // 전역으로 정의된 DUNGEON_STONE

int get_current_dungeon_stone_id(void) {
    int idx = get_current_map_index();
    if (idx < 0 || idx > 4) return 0;
    return DUNGEON_STONE[idx];
}