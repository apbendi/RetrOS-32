# Building a program on RetrOS's standard C library with the builder's gcc. A Makefile sets ROOT to the repo's
# top directory, includes this, and links with:
#
#   $(LD) $(STDC_LDFLAGS) -o program $(STDC_START) objects... $(STDC_LIBS)
#
# It uses the library as installed in rootfs/usr, so `make -C libc` comes first.

STDC_USR := $(ROOT)/rootfs/usr
STDC_CC ?= gcc

# __retros__ is what TCC predefines for RetrOS (apps/tcc/config.h)
STDC_CFLAGS := -m32 -march=i486 -nostdinc -isystem $(STDC_USR)/include -fno-pie -fno-stack-protector \
	-fno-asynchronous-unwind-tables -D__retros__
STDC_LDFLAGS := -m elf_i386 -T $(ROOT)/libc/retros.ld
STDC_START := $(STDC_USR)/lib/crt1.o $(STDC_USR)/lib/crti.o
# libgcc.a for the helpers GCC calls, like 64-bit division
STDC_LIBS := $(STDC_USR)/lib/libc.a $(shell $(STDC_CC) -m32 -print-libgcc-file-name) $(STDC_USR)/lib/crtn.o
