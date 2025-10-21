// include/save_load.h
#ifndef SAVE_LOAD_H
#define SAVE_LOAD_H

#include <stdbool.h>

// 저장 파일 경로
#define SAVE_FILE "save_data.bin"

// 함수 프로토타입
bool save_game(void);
bool load_game(void);

// 추가: 저장 존재 여부 확인
bool save_exists(void);

#endif // SAVE_LOAD_H
