#ifndef ACCOUNT_H
#define ACCOUNT_H


#include <stddef.h>


void handle_modify_info(const char *id, const char *args);
void handle_delete_account(const char *id, int sock);
void handle_admin_menu(const char *id, int sock);
void handle_view_all_members(int sock);
void handle_change_admin_pw(const char* id, int sock);
void handle_restore_member(int sock);
void handle_delete_member(int sock);
void handle_view_notes(const char *id, int sock);
void handle_offline_note(const char *sender, const char *args);
void handle_whisper(const char *sender, const char *args);
void notify_offline_note_count(const char *id, int sock);
void handle_modify_pw(const char *id, const char *new_pw, int sock);
void handle_modify_phone(const char *id, const char *new_phone, int sock);
void handle_modify_email(const char *id, const char *new_email, int sock);
void handle_admin_modify_user(const char *admin_id, int sock, const char *args);


#endif
