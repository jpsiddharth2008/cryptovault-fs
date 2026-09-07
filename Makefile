CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude $(shell pkg-config fuse3 --cflags) $(shell pkg-config libsodium --cflags)
LDFLAGS = $(shell pkg-config fuse3 --libs) $(shell pkg-config libsodium --libs)

SRCS = src/main.c src/fuse_ops.c src/crypto.c src/path_utils.c
OBJS = $(SRCS:.c=.o)

encfs: $(OBJS)
	$(CC) $(CFLAGS) -o encfs $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Unit test: crypto.o + test harness, no FUSE needed
test-crypto: src/crypto.o tests/test_crypto.c
	$(CC) $(CFLAGS) -o test_crypto src/crypto.o tests/test_crypto.c $(shell pkg-config libsodium --libs)
	./test_crypto

# Integration test: needs the full binary built first
test-fs: encfs
	bash tests/test_fs.sh

test: test-crypto test-fs

clean:
	rm -f encfs test_crypto $(OBJS) src/crypto.o

.PHONY: clean test test-crypto test-fs
