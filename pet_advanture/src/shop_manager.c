#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "shop_manager.h"
#include "console_ui.h"
#include "inventory_manager.h"
#include "pet_manager.h"


// shop_manager.c 상단부 또는 include 절 이후에 추가
void update_quest_progress(int quest_id, int progress);

extern Pet *currentPet;
extern int playerGold;

typedef struct {
    int x, y;
    const char *emoji;
    bool visible;
} Shop;
#define MAX_SHOPS 2

static Shop shops[MAX_SHOPS];
static int shopCount;

void init_shops(void) {
    shopCount = 0;
}

void spawn_shops_for_map(int mapIndex) {
    shopCount = 0;
    // 마을(mapIndex==0)에만 상점
    if (mapIndex == 0) {
        shops[shopCount++] = (Shop){ .x = 18, .y = 16,
            .emoji   = "🏪",
            .visible = true
        };
    }
}

void draw_shops(int px, int py) {
    for (int i = 0; i < shopCount; i++) {
        Shop *s = &shops[i];
        if (!s->visible) continue;
        if (s->x == px && s->y == py) continue;
        printf("\033[%d;%dH", s->y + 2, s->x*2 +1);
        printf("%s", s->emoji);
    }
}

bool shop_at(int px, int py) {
    for (int i = 0; i < shopCount; i++) {
        Shop *s = &shops[i];
        if (s->visible && s->x == px && s->y == py)
            return true;
    }
    return false;
}

void move_to_shop_heal(void) {
    printf("[회복실]로 이동했습니다. HP/SP가 회복됩니다.\n");
    currentPet->hp = currentPet->maxHp;
    currentPet->mp = currentPet->maxMp;
}

void show_shop_menu(void) {
    int ch;
    do {
        printf("\n=== 점빵 메뉴 ===\n");
        printf("1) 회복 서비스\n");
        printf("2) 물약 구매\n");
        printf("3) 포춘 쿠키 가챠\n");
        printf("4) 펫 연구\n");
        printf("5) 나가기\n");
        printf("선택: ");
        if (scanf("%d", &ch) != 1) { while(getchar()!='\n'); ch=5; }
        while(getchar()!='\n');

        switch (ch) {
            case 1: {
                int cost = currentPet->level * 100;
                if (playerGold < cost) {
                    printf("골드가 부족합니다.\n");
                } else {
                    playerGold -= cost;
                    currentPet->hp = currentPet->maxHp;
                    currentPet->mp = currentPet->maxMp;
                    printf("완전 회복! (-%dG)\n", cost);
                    getchar();
                    draw_shop_ui();
                }
            } break;
            case 2:
                // 물약 구매 로직 (기존 buy_potions 함수 내용 복사)
                printf("\n[물약 구매]\n1) HP 낮은급-50G 2) HP 중급-100G 3) HP 고급-200G\n"
                       "4) SP 낮은급-50G 5) SP 중급-100G 6) SP 고급-200G\n7) 나가기\n선택: ");
                {
                    int p; if (scanf("%d",&p)!=1){ while(getchar()!='\n'); break; }
                    while(getchar()!='\n');
                    int id=0, price=0;
                    if (p==1){id=ITEM_HP_LOW;  price=50;}
                    else if(p==2){id=ITEM_HP_MID;  price=100;}
                    else if(p==3){id=ITEM_HP_HIGH; price=200;}
                    else if(p==4){id=ITEM_SP_LOW;  price=50;}
                    else if(p==5){id=ITEM_SP_MID;  price=100;}
                    else if(p==6){id=ITEM_SP_HIGH; price=200;}
                    else break;
                    if (playerGold < price) {
                        printf("골드가 부족합니다.\n");
                    } else if (add_item(id,1)) {
                        playerGold -= price;
                        printf("구매완료: ID%d ×1 (-%dG)\n", id, price);
                        update_quest_progress(2, 0);
                    }

                    getchar();
                    draw_shop_ui();
                }
                break;
            case 3: {
                // 가챠
                int cost =  100;
                if (playerGold < cost) {
                    printf("골드가 부족합니다.\n");
                } else {
                    playerGold -= cost;
                    srand((unsigned)time(NULL));
                    int r = rand()%100;
                    if      (r<40) printf("꽝!\n");
                    else if (r<80) { int g= rand()%201 + cost * 1.5; playerGold+=g; printf("골드+%d\n",g); }
                    else if (r<90) { int x=rand()%101+50; gain_xp(x); printf("XP+%d\n",x); }
                    else if (r<99) { int sid=rand()%10+1; printf("스킬획득 ID%d\n",sid); learn_skill(sid); }
                    else           { printf("전설스킬획득!\n"); learn_skill(99); }
                    getchar();
                    draw_shop_ui();
                }
            } break;
            case 4: {
                // 펫 연구
                printf("\n투자할 골드(100단위): ");
                int amt; if (scanf("%d",&amt)!=1){ while(getchar()!='\n'); break; }
                while(getchar()!='\n');
                if (amt%100!=0) { printf("100단위 입력\n"); }
                else if (!research_pet(amt)) {
                    printf("골드가 부족하거나 실패\n");
                } else {
                    printf("연구성공 (-%dG)\n", amt);
                    getchar();
                    draw_shop_ui();
                }
            } break;
            default: break;
        }
    } while (ch>=1 && ch<=4);
}
