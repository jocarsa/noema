CC=gcc
CFLAGS=-std=c11 -O2 -Wall -Wextra -Wpedantic

SRC=src/main.c src/noema.c src/lexer.c src/parser.c src/runtime.c src/diag.c
OUT=noema

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

clean:
	rm -f $(OUT)

