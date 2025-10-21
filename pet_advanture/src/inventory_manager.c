// src/inventory_manager.c
#include <stdio.h>
#include "inventory_manager.h"

InventorySlot inventory[MAX_INVENTORY_SLOTS];
int inventoryCount = 0;

void init_inventory(void) {
    inventoryCount = 0;
    for (int i = 0; i < MAX_INVENTORY_SLOTS; i++) {
        inventory[i].itemId = 0;
        inventory[i].quantity = 0;
    }
}

bool add_item(int itemId, int qty) {
    // 이미 있는 슬롯 찾기
    for (int i = 0; i < inventoryCount; i++) {
        if (inventory[i].itemId == itemId) {
            inventory[i].quantity += qty;
            return true;
        }
    }
    // 신규 슬롯
    if (inventoryCount < MAX_INVENTORY_SLOTS) {
        inventory[inventoryCount].itemId = itemId;
        inventory[inventoryCount].quantity = qty;
        inventoryCount++;
        return true;
    }
    printf("인벤토리 공간이 부족합니다!\n");
    return false;
}

bool use_item(int itemId) {
    for (int i = 0; i < inventoryCount; i++) {
        if (inventory[i].itemId == itemId && inventory[i].quantity > 0) {
            inventory[i].quantity--;
            if (inventory[i].quantity == 0) {
                // 슬롯 정리
                for (int j = i; j < inventoryCount - 1; j++) {
                    inventory[j] = inventory[j + 1];
                }
                inventoryCount--;
            }
            return true;
        }
    }
    printf("해당 아이템이 없습니다.\n");
    return false;
}

int get_item_count(int itemId) {
    for (int i = 0; i < inventoryCount; i++) {
        if (inventory[i].itemId == itemId) {
            return inventory[i].quantity;
        }
    }
    return 0;
}

void show_inventory(void) {
    printf("\n=== 인벤토리 ===\n");
    if (inventoryCount == 0) {
        printf("빈 공간입니다.\n");
        return;
    }
    for (int i = 0; i < inventoryCount; i++) {
        printf("%2d) ItemID:%d  Qty:%d\n",
               i + 1,
               inventory[i].itemId,
               inventory[i].quantity);
    }
    printf("==============\n");
}
