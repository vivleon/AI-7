// src/save_load.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>   // ← 추가
#include "save_load.h"
#include "pet_manager.h"
#include "map_manager.h"
#include "quest_manager.h"
#include "inventory_manager.h"

extern Pet pets[MAX_PETS];
extern Pet *currentPet;
extern int playerGold;
extern int crystalCount;
extern int playerX, playerY;

// questFlags는 quest_manager.c 에서 정의되어야 합니다.
extern Quest quests[MAX_QUESTS];
extern int questRemaining[MAX_QUESTS];

bool save_game(void) {
    FILE *fp = fopen(SAVE_FILE, "wb");
    if (!fp) {
        perror("세이브 파일 열기 실패");
        return false;
    }
    // 1) 펫 정보 전체
    fwrite(pets, sizeof(Pet), MAX_PETS, fp);
    // 2) 현재 펫 인덱스
    int idx = petId_for_current();
    fwrite(&idx, sizeof(int), 1, fp);
    // 3) 자원
    fwrite(&playerGold,   sizeof(int), 1, fp);
    fwrite(&crystalCount, sizeof(int), 1, fp);
    // 4) 플레이어 위치
    fwrite(&playerX, sizeof(int), 1, fp);
    fwrite(&playerY, sizeof(int), 1, fp);
    // 5) 인벤토리
    fwrite(&inventoryCount, sizeof(int), 1, fp);
    for (int i = 0; i < inventoryCount; i++) {
        fwrite(&inventory[i], sizeof(InventorySlot), 1, fp);
    }
    // 6) 퀘스트 상태
    // 퀘스트 남은 진행량 저장
    for (int i = 0; i < MAX_QUESTS; ++i) {
        fwrite(&questRemaining[i], sizeof(int), 1, fp);
    }
    fclose(fp);
    // printf("게임이 저장되었습니다.\n");
    return true;
}

bool load_game(void) {
    FILE *fp = fopen(SAVE_FILE, "rb");
    if (!fp) {
        return false;
    }
    // 1) 펫 정보
    fread(pets, sizeof(Pet), MAX_PETS, fp);
    // 2) 현재 펫 인덱스
    int idx;
    fread(&idx, sizeof(int), 1, fp);
    select_pet(idx);
    // 3) 자원
    fread(&playerGold,   sizeof(int), 1, fp);
    fread(&crystalCount, sizeof(int), 1, fp);
    // 4) 위치
    fread(&playerX, sizeof(int), 1, fp);
    fread(&playerY, sizeof(int), 1, fp);
    // 5) 인벤토리
    fread(&inventoryCount, sizeof(int), 1, fp);
    for (int i = 0; i < inventoryCount; i++) {
        fread(&inventory[i], sizeof(InventorySlot), 1, fp);
    }
    // 6) 퀘스트 상태
    // 퀘스트 남은 진행량 불러오기
    for (int i = 0; i < MAX_QUESTS; ++i) {
        fread(&questRemaining[i], sizeof(int), 1, fp);
    }
    fclose(fp);
    printf("세이브 데이터를 불러왔습니다.\n");
    return true;
}


bool save_exists(void) {
    struct stat st;
    return stat(SAVE_FILE, &st) == 0;
}
