// password_utils.c
#include <string.h>
#include <ctype.h>
#include <openssl/sha.h>
#include "password_utils.h"
#include <stdio.h>


void hash_password_sha256(const char *password, char *hash_buf) {
    // SHA256 해시 생성
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)password, strlen(password), hash);

    // 해시값을 16진수 문자열로 변환
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(hash_buf + (i * 2), "%02x", hash[i]);
    hash_buf[64] = '\0';
}


// 8자 이상, 대/소/숫자/특수문자 각 1개 이상
int is_valid_password(const char *pw) {
    int len = strlen(pw);
    int has_upper = 0, has_lower = 0, has_digit = 0, has_special = 0;
    if (len < 8) return 0;
    for (int i = 0; i < len; i++) {
        if (isupper(pw[i])) has_upper = 1;
        else if (islower(pw[i])) has_lower = 1;
        else if (isdigit(pw[i])) has_digit = 1;
        else if (ispunct(pw[i])) has_special = 1;
    }
    return has_upper && has_lower && has_digit && has_special;
}

// 간이 전화번호 유효성(숫자만, 길이 10~13)
int is_valid_phone(const char *ph) {
    int len = strlen(ph);
    if (len < 10 || len > 13) return 0;
    for (int i = 0; i < len; i++)
        if (!isdigit(ph[i])) return 0;
    return 1;
}

// 간이 이메일 유효성
int is_valid_email(const char *email) {
    const char *at = strchr(email, '@');
    const char *dot = strrchr(email, '.');
    return at && dot && (at < dot) && strlen(email) >= 5;
}
