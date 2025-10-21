CC     = gcc
CFLAGS = -Wall -g
MYSQL  = `mysql_config --cflags --libs` -lssl -lcrypto

# --- 서버 오브젝트 파일 목록 ---
SRV_OBJS = server.o handler.o room.o chat_util.o db.o global.o \
           account.o password_utils.o

# --- 클라이언트 오브젝트 파일 목록 ---
CLI_OBJS = client.o global.o

# --- 서버용 빌드 타겟 ---
chat_server: $(SRV_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread $(MYSQL)

# --- 테스트용 클라이언트 빌드 (ELF 바이너리) ---
.PHONY: client
client: $(CLI_OBJS)
	$(CC) $(CFLAGS) -o client.bin client.o global.o -lpthread -lreadline -lncursesw

# --- 배포용 빌드 + 단일 스크립트 생성 ---
.PHONY: run
run: chat_server client
	@echo "▶ Generating single-file 'client' wrapper (no chmod needed)…"
	@printf '%s\n' \
	  '#!/usr/bin/env bash' \
	  '' \
	  '# 필수 패키지 libncursesw5-dev가 설치되어 있지 않으면 아래 명령어를 실행하세요:' \
	  '# sudo apt-get install -y libncursesw5-dev' \
	  '' \
	  '# === extract & exec embedded payload ===' \
	  'awk '\''/^__PAYLOAD_BELOW__$$/{start=1;next} start{print}'\'' "$$0" | base64 -d > /tmp/client_exec' \
	  'chmod +x /tmp/client_exec' \
	  'exec /tmp/client_exec "$$@"' \
	  '' \
	  '__PAYLOAD_BELOW__' \
	> client
	@base64 client.bin >> client
	@chmod +x client
	@rm -f client.bin
	@echo "✅ Done: distribute './client' only."
	@echo "▶ Client runs via: bash client <IP> <PORT>"

# --- 소스 컴파일 규칙 ---
server.o: server.c handler.h db.h member.h
	$(CC) $(CFLAGS) -c $<
handler.o: handler.c handler.h db.h member.h room.h chat_util.h
	$(CC) $(CFLAGS) -c $<
room.o: room.c room.h chat_util.h member.h
	$(CC) $(CFLAGS) -c $<
chat_util.o: chat_util.c chat_util.h member.h
	$(CC) $(CFLAGS) -c $<
db.o: db.c db.h member.h
	$(CC) $(CFLAGS) -c $<
account.o: account.c account.h password_utils.h
	$(CC) $(CFLAGS) -c $<
password_utils.o: password_utils.c password_utils.h
	$(CC) $(CFLAGS) -c $<
global.o: global.c global.h
	$(CC) $(CFLAGS) -c $<
client.o: client.c global.h
	$(CC) $(CFLAGS) -c $<

# --- 정리 타겟 ---
.PHONY: clean
clean:
	rm -f *.o chat_server client client.bin
