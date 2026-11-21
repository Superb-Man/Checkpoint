CC = gcc
CFLAGS = -g3 -O0

.PHONY: all run clean

all: libcheckpoint.so test restore

libcheckpoint.so: cp.c
	$(CC) $(CFLAGS) -fPIC -shared -o libcheckpoint.so cp.c

test: test.c
	$(CC) $(CFLAGS) -o test test.c

restore: restart.c
	$(CC) $(CFLAGS) -static -fno-stack-protector -W -o restore restart.c

run: all
	LD_PRELOAD=$(CURDIR)/libcheckpoint.so ./test

clean:
	rm -f libcheckpoint.so test restore myckpt
