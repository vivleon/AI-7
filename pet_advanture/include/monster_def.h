// ✅ monster_def.h (새로 생성)
#ifndef MONSTER_DEF_H
#define MONSTER_DEF_H

#include <stdbool.h>

typedef struct {
    int id;
    char name[32];
    char emoji[16];
    int hp, maxHp;
    int atk, def, spd;
    bool isElite;
    int uniqueSkillId;
} Monster;

#endif // MONSTER_DEF_H