
# #!/bin/bash
# set -e

# echo "[*] Compilando..."
# mkdir -p bin

# gcc -o bin/p2-server \
#     src/p2-server.c src/hash.c src/csv_reader.c src/common.c \
#     -lpthread -lm

# gcc -o bin/p2-client \
#     src/p2-client.c src/csv_reader.c src/common.c src/utils_ui.c \
#     -lm

# echo "[+] Compilacion exitosa."

# echo "[*] Iniciando servidor..."
# ./bin/p2-server &
# SERVER_PID=$!
# echo "[+] Servidor PID: $SERVER_PID (puerto 8080)"

# sleep 1

# ./bin/p2-client 127.0.0.1

# echo "[*] Deteniendo servidor..."
# kill $SERVER_PID 2>/dev/null
# wait $SERVER_PID 2>/dev/null
# echo "[+] Listo."

CC=gcc
CFLAGS=-lpthread -lm

all: server client

server:
	mkdir -p bin
	$(CC) -o bin/p2-server \
		src/p2-server.c src/hash.c src/csv_reader.c src/common.c \
		$(CFLAGS)

client:
	mkdir -p bin
	$(CC) -o bin/p2-client \
		src/p2-client.c src/csv_reader.c src/common.c src/utils_ui.c \
		-lm

run: all
	-pkill -f p2-server 2>/dev/null; sleep 1
	./bin/p2-server & \
	SERVER_PID=$$!; \
	echo "Servidor PID: $$SERVER_PID"; 
#	sleep 1; \
# 	./bin/p2-client 127.0.0.1; \
# 	kill $$SERVER_PID

clean:
	$(pkill) $$SERVER_PID
	rm -rf bin
