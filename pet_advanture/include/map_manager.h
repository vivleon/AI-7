// include/map_manager.h
#ifndef MAP_MANAGER_H
#define MAP_MANAGER_H

#include <stdbool.h>

// 맵 인덱스 정의
// MAP_TOWN: 마을, MAP_FOREST: 고대 숲, MAP_DESERT: 사막 사원, MAP_SNOW: 설원 고성,
// MAP_EVOLUTION_TEMPLE: 진화의 전당, MAP_FINAL_TEMPLE: 제룬의 제단
typedef enum {
    MAP_TOWN,               // 0
    MAP_FOREST,             // 1
    MAP_DESERT,             // 2
    MAP_SNOW,               // 3
    MAP_FINAL_TEMPLE,   // 4
    MAP_COUNT
} MapIndex;


bool is_floor(int x, int y);

int  get_map_level_min_for_index(int);   // in map_manager.h  
int  get_map_level_max_for_index(int);   // in map_manager.h  


// 토큰 수와 문자열 폭 정의
#define MAP_TOKENS_W    32
#define WIDTH           (MAP_TOKENS_W * 4)  // 64
#define HEIGHT          32

// 각 토큰은 정확히 4문자: 코드＋공백×(4-코드길이)
// '1' = 벽, '0' = 통로, '20' = 이벤트, '4' = 포탈
#define TILE_WALL_TOKEN   "1   "
#define TILE_FLOOR_TOKEN  "0   "
#define TILE_EVENT_TOKEN  "2   "
#define TILE_PORTAL_TOKEN "4   "
#define TILE_MAPD_TOKEN "6   "
#define TILE_FBOSS_TOKEN "W   "
#define TILE_GRESS_TOKEN "G   "
#define TILE_PINE_TOKEN "PINE""PALM"
#define TILE_BLWH_TOKEN "BLWH"
#define TILE_WHBL_TOKEN "WHBL"
#define TILE_PALM_TOKEN "PALM"
#define TILE_MTN_TOKEN "MTN "

// 몬스터·NPC·상점 모듈
#include "monster_manager.h"
#include "npc_manager.h"
#include "shop_manager.h"

// 엔티티 타입
typedef enum {
    ENTITY_NPC,
    ENTITY_MERCHANT,
    ENTITY_MONSTER
} EntityType;


// 엔티티 데이터
typedef struct {
    EntityType type;
    int        x, y;
    int        id;       // NPC/몬스터 구분용 ID
} Entity;

#define MAX_ENTITIES  16

// 플레이어, 펫, 몬스터 추적
extern int playerX, playerY;
// ─── 현재 선택된 맵을 가리키는 포인터 (널문자 포함 WIDTH+1)
extern char (*current_map)[WIDTH+1];

// ─── 맵 데이터 (WIDTH+1 크기: 널문자까지)
extern char map1[HEIGHT][WIDTH+1];
extern char map2[HEIGHT][WIDTH+1];
extern char map3[HEIGHT][WIDTH+1];
extern char map4[HEIGHT][WIDTH+1];
extern char map5[HEIGHT][WIDTH+1];

// 초기화 및 맵 전환
void init_maps(void);
void load_map(int mapIndex);

// 렌더링 및 입력 처리
void draw_map(int px, int py);
void handle_map_input(int *px, int *py);


// 포탈/이벤트 처리
void process_events(int px, int py);



// 엔티티 렌더링 및 조회
void spawn_entities(int mapIndex);
void draw_npcs(int px, int py);
void draw_shops(int px, int py);

// 몬스터/NPC/상점 스폰
void init_monsters(void);
void spawn_monsters_for_map(int mapIndex);
void init_npcs(void);
void spawn_npcs_for_map(int mapIndex);
void init_shops(void);
void spawn_shops_for_map(int mapIndex);


// HUD용 헬퍼
const char* get_current_map_name(void);
int         get_map_level_min(void);
int         get_map_level_max(void);
int         get_current_map_index(void);
int         get_current_dungeon_stone_id(void);

#endif // MAP_MANAGER_H
