// [handler.h]
#ifndef HANDLER_H
#define HANDLER_H

void *client_handler(void *arg);
int handle_whisper_or_note(const char *id, const char *target, const char *message, int flag);
int is_user_admin(const char *id);
int prompt_and_read_valid(
    int sock,
    const char *prompt,
    char *out,
    int outsz,
    int (*validator)(const char *),
    const char *errmsg
);

#endif