CC = gcc
CFLAGS = -Wall -Wextra -I./src
LDFLAGS = -pthread -lrt

SRC = src/server.c src/teller.c src/client.c
BIN = bin/server bin/client

all: bin $(BIN)

bin:
	mkdir -p bin

bin/server: src/server.c src/teller.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

bin/client: src/client.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -rf bin AdaBank.bankLog  # Remove entire bin directory

.PHONY: all clean