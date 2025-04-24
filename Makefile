CC = gcc
CFLAGS = -Wall -Wextra -I./include
LDFLAGS = -pthread -lrt

SRC = src/server.c src/teller.c src/client.c
BIN = bin/server bin/client

all: $(BIN)

bin/server: src/server.c src/teller.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)  # <-- TAB HERE

bin/client: src/client.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)  # <-- TAB HERE

clean:
	rm -f $(BIN) /tmp/bank_server.fifo /dev/shm/bank_shm

.PHONY: all clean