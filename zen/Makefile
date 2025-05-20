CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=c17 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lcurl

SRC = main.c cJSON.c
OBJ = $(SRC:.c=.o)
BIN = zen_updater.out

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)
