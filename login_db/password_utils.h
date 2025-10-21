// password_utils.h
#ifndef PASSWORD_UTILS_H
#define PASSWORD_UTILS_H

int is_valid_password(const char *pw);
void hash_password_sha256(const char *pw, char *hash_buf);
int is_valid_phone(const char *ph);      
int is_valid_email(const char *email);   

#endif
