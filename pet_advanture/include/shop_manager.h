// include/shop_manager.h
#ifndef SHOP_MANAGER_H
#define SHOP_MANAGER_H

#include <stdbool.h>

// 가챠 티켓 ID (인벤토리 모듈과 일치시킬 것)
#define ITEM_GACHA_TICKET 7

// 쇼핑 메뉴 진입
void show_shop_menu(void);


void heal_service(void);
void buy_potions(void);
void gacha_fortune_cookie(void);
void pet_research(void);

// 유틸 (외부에서 호출)
void move_to_shop_heal(void);

// 상점 타일 위에 있으면 쇼핑 메뉴 진입
bool shop_at(int px, int py);

#endif // SHOP_MANAGER_H
