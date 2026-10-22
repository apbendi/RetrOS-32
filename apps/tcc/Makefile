# TinyCC for RetrOS-32 (apps/tcc/README.md).
#
# bin/i386-retros-tcc is the cross compiler: it runs in the builder and makes RetrOS programs. It's built with the
# builder's own gcc, from tinycc/ with config.h, and finds headers, libraries and crt files under the repo's rootfs/,
# where they'll be on RetrOS's disk. TCC's runtime library, libtcc1.a, is built with it, and installed with TCC's
# own headers in rootfs/lib/tcc.
#
# tcc.o is TCC itself, as a RetrOS program: built with gcc on RetrOS's C library (libc/program.mk), with config.h's
# paths as they are on RetrOS's disk, and installed as /bin/tcc.o.

ROOT := $(abspath ../..)
include $(ROOT)/make/common.mk
include $(ROOT)/libc/program.mk

SRC := tinycc
BUILD_DIR := bin
HOST_CC ?= gcc

CROSS := $(BUILD_DIR)/i386-retros-tcc
TCC_DIR := $(ROOT)/rootfs/lib/tcc

# The settings in config.h, with paths pointing into rootfs/
CROSS_DEFINES := -DONE_SOURCE=1 -DCONFIG_SYSROOT=\"$(ROOT)/rootfs\" -DCONFIG_TCCDIR=\"$(TCC_DIR)\"
TCC_INCLUDES := -I . -I $(BUILD_DIR) -I $(SRC)

# libtcc1.a for i386, as tinycc/lib/Makefile builds it, without the Linux-only pieces
LIBTCC1_SRCS := libtcc1.c stdatomic.c builtin.c alloca.S alloca-bt.S atomic.S
LIBTCC1_OBJS := $(patsubst %,$(BUILD_DIR)/lib/%.o,$(basename $(LIBTCC1_SRCS)))

# TCC's own flags, from its Makefile, without its warnings
NATIVE_CFLAGS := $(STDC_CFLAGS) -O2 -fno-strict-aliasing -w -DONE_SOURCE=1 $(TCC_INCLUDES)

.PHONY: all install clean
all: $(CROSS) $(BUILD_DIR)/libtcc1.a tcc.o install

# tccdefs.h, converted to C strings that are compiled in
$(BUILD_DIR)/c2str: $(SRC)/conftest.c
	$(call make_dir,$(BUILD_DIR))
	$(QUIET)$(HOST_CC) -DC2STR $< -o $@

$(BUILD_DIR)/tccdefs_.h: $(SRC)/include/tccdefs.h $(BUILD_DIR)/c2str
	$(QUIET)$(BUILD_DIR)/c2str $< $@

$(CROSS): $(wildcard $(SRC)/*.c $(SRC)/*.h) config.h $(BUILD_DIR)/tccdefs_.h
	$(QUIET)$(HOST_CC) -O2 -Wno-unused-result $(CROSS_DEFINES) $(TCC_INCLUDES) $(SRC)/tcc.c -o $@ -lm
	$(QUIET)echo "[tcc] built the cross compiler, $@"

$(BUILD_DIR)/lib/%.o: $(SRC)/lib/%.c $(CROSS)
	$(call make_dir,$(BUILD_DIR)/lib)
	$(QUIET)$(CROSS) -c $< -o $@ -B$(SRC) -I$(SRC)

$(BUILD_DIR)/lib/%.o: $(SRC)/lib/%.S $(CROSS)
	$(call make_dir,$(BUILD_DIR)/lib)
	$(QUIET)$(CROSS) -c $< -o $@ -B$(SRC) -I$(SRC)

$(BUILD_DIR)/tcc-native.o: $(wildcard $(SRC)/*.c $(SRC)/*.h) config.h $(BUILD_DIR)/tccdefs_.h $(STDC_USR)/lib/libc.a
	$(QUIET)$(STDC_CC) $(NATIVE_CFLAGS) -c $(SRC)/tcc.c -o $@

tcc.o: $(BUILD_DIR)/tcc-native.o $(STDC_USR)/lib/libc.a
	$(QUIET)$(LD) $(STDC_LDFLAGS) -o $@ $(STDC_START) $< $(STDC_LIBS)
	$(QUIET)echo "[tcc] built tcc.o, $$(wc -c < $@) bytes"

$(BUILD_DIR)/libtcc1.a: $(LIBTCC1_OBJS)
	$(QUIET)$(CROSS) -ar rcs $@ $^
	$(QUIET)echo "[tcc] built libtcc1.a"

# What RetrOS's disk gets in /lib/tcc
install: $(BUILD_DIR)/libtcc1.a tcc.o
	$(call make_dir,$(ROOT)/rootfs/bin)
	$(QUIET)cp tcc.o $(ROOT)/rootfs/bin/
	$(call make_dir,$(TCC_DIR)/include)
	$(QUIET)cp $(SRC)/include/*.h $(TCC_DIR)/include/
	$(QUIET)cp $(BUILD_DIR)/libtcc1.a $(TCC_DIR)/

clean:
	$(QUIET)$(RM_RF) $(BUILD_DIR) $(TCC_DIR) tcc.o
