CC      = cc
CFLAGS  = -O2 -Wall -Wextra -std=c11
LDFLAGS = -lm

.PHONY: all clean install uninstall

all: uping

uping: uping.c
	$(CC) $(CFLAGS) -o uping uping.c $(LDFLAGS)

install: uping
	install -m 0755 uping /usr/local/bin/uping

uninstall:
	rm -f /usr/local/bin/uping

clean:
	rm -f uping
