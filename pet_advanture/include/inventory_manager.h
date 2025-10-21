// include/inventory_manager.h
#ifndef INVENTORY_MANAGER_H
#define INVENTORY_MANAGER_H

#include <stdbool.h>

#define MAX_INVENTORY_SLOTS  20

// 아이템 ID 정의
#define ITEM_HP_LOW    1
#define ITEM_HP_MID    2
#define ITEM_HP_HIGH   3
#define ITEM_SP_LOW    4
#define ITEM_SP_MID    5
#define ITEM_SP_HIGH   6

typedef struct {
    int  itemId;
    int  quantity;
} InventorySlot;

// 전역 인벤토리 배열
extern InventorySlot inventory[MAX_INVENTORY_SLOTS];
extern int inventoryCount;

// 함수 프로토타입
void init_inventory(void);
bool add_item(int itemId, int qty);
bool use_item(int itemId);
int  get_item_count(int itemId);
void show_inventory(void);

#endif // INVENTORY_MANAGER_H
