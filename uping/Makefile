CC      = cc
CFLAGS  = -O2 -Wall -Wextra -std=c11
LDFLAGS = -lm
UNAME   := $(shell uname)

.PHONY: all clean install uninstall

all: uping

uping: uping.c
	$(CC) $(CFLAGS) -o uping uping.c $(LDFLAGS)

install: uping
	install -m 0755 uping /usr/local/bin/uping
ifeq ($(UNAME),Linux)
	# Grant CAP_NET_RAW so uping works without sudo at runtime.
	# Requires libcap2-bin (apt install libcap2-bin / dnf install libcap).
	setcap cap_net_raw+ep /usr/local/bin/uping
endif

uninstall:
	rm -f /usr/local/bin/uping

clean:
	rm -f uping
