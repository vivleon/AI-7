// include/console_ui.h
#ifndef CONSOLE_UI_H
#define CONSOLE_UI_H

#include <stdbool.h>  // bool 정의

// UI 초기화 및 화면 갱신
void init_ui(void);

// 전체 UI 그리기 (맵 + HUD)
void draw_ui(int px, int py);

// HUD(상태창)만 그리기
void render_hud(void);

// 전투 중 UI 즉시 갱신
void draw_battle_ui(void);

// 상점 UI 즉시 갱신
void draw_shop_ui(void);

// 메뉴 활성화 처리 (메뉴 입력 받기)
bool handle_menu_input(void);


void draw_quest_board_ui(int start_row, int start_col);

#endif // CONSOLE_UI_H

