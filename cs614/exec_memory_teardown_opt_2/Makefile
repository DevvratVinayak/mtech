obj-m += time.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# List of test source files (without .c extension)
TESTS := test test1 test2 test3 test4 test5 test6 test7 test8 test9

all: module

module:
	make -C $(KDIR) M=$(PWD) modules

tests: $(addsuffix _bin,$(TESTS))

%_bin: %.c
	gcc -o $@ $<

benchmark: tests
	@echo "Run: sudo ./benchmark.sh [iterations]"

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f $(addsuffix _bin,$(TESTS))

.PHONY: all module tests benchmark clean
