# ===== User Programs =====
CC = gcc
CFLAGS = -pthread

TARGETS = test_heavy test_double_exec test_nonmain_exec new_binary

obj-m := exec_hook.o
KVERSION := $(shell uname -r)

all: clean user module

user: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

module:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	rm -f $(TARGETS)
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

.PHONY: all user module clean