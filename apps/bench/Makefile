# Benchmarks for repeatable performance tests, in standard C on RetrOS's C library (libc/program.mk), installed as
# /bin/cpubench.o and /bin/iobench.o. Each does the same work every run and prints how long it took.

ROOT := $(abspath ../..)
include $(ROOT)/make/common.mk
include $(ROOT)/libc/program.mk

BUILD_DIR := bin
PROGRAMS := cpubench.o iobench.o

.PHONY: all install clean
all: $(PROGRAMS) install

$(PROGRAMS): %.o: %.c $(STDC_USR)/lib/libc.a
	$(call make_dir,$(BUILD_DIR))
	$(QUIET)$(STDC_CC) $(STDC_CFLAGS) -O2 -Wall -c $< -o $(BUILD_DIR)/$@
	$(QUIET)$(LD) $(STDC_LDFLAGS) -o $@ $(STDC_START) $(BUILD_DIR)/$@ $(STDC_LIBS)
	$(QUIET)echo "[bench] built $@"

install: $(PROGRAMS)
	$(call make_dir,$(ROOT)/rootfs/bin)
	$(QUIET)cp $(PROGRAMS) $(ROOT)/rootfs/bin/

clean:
	$(QUIET)$(RM_RF) $(BUILD_DIR)
	$(QUIET)$(RM_F) $(PROGRAMS)
