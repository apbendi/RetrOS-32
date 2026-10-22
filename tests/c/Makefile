# The C harness's build half (tests/c/run.py runs it in the Docker builder, then runs the programs in QEMU).
#
# For each program in cases/:
# - out/linux/NAME, built with gcc and glibc for Linux, and out/expected/NAME.out, what it prints there;
# - out/retros/NAME-gcc, built with gcc on RetrOS's C library (libc/program.mk);
# - out/retros/NAME-tcc, built with TCC's cross compiler (apps/tcc).
# TinyCC's own tests are built by tests2.sh into out/tests2 (see tests2.conf).
# out/test.img is a copy of RetrOS-32-debug.img with the RetrOS programs and the sources in /t, and TinyCC's tests,
# built and as sources, in /t/tests2 (TinyCC's 18_include needs that name), for compiling on RetrOS (run.py --native).

ROOT := $(abspath ../..)
include $(ROOT)/libc/program.mk

TCC := $(ROOT)/apps/tcc/bin/i386-retros-tcc
TESTS2 := $(ROOT)/apps/tcc/tinycc/tests/tests2
OUT := out
CASES := $(basename $(notdir $(wildcard cases/*.c)))
HEADERS := $(wildcard cases/*.h)

LINUX := $(CASES:%=$(OUT)/linux/%)
EXPECTED := $(CASES:%=$(OUT)/expected/%.out)
RETROS := $(CASES:%=$(OUT)/retros/%-gcc) $(CASES:%=$(OUT)/retros/%-tcc)

.PHONY: all clean tests2
all: $(EXPECTED) $(OUT)/test.img

# Keep the Linux builds, which make would otherwise delete as intermediate files
.SECONDARY: $(LINUX)

# 64-bit time_t and off_t, as musl has them on 32-bit x86
$(OUT)/linux/%: cases/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	@gcc -m32 -O2 -w -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64 -o $@ $< -lm

# Run in an empty directory, as the program runs in /t on RetrOS. stderr goes with stdout, as on RetrOS's terminal.
$(OUT)/expected/%.out: $(OUT)/linux/%
	@mkdir -p $(dir $@)
	@dir=$$(mktemp -d) && (cd $$dir && timeout 60 $(abspath $<) > $(abspath $@) 2>&1; true) && rm -rf $$dir

$(OUT)/retros/%-gcc: cases/%.c $(HEADERS) $(STDC_USR)/lib/libc.a
	@mkdir -p $(dir $@)
	@gcc $(STDC_CFLAGS) -O2 -w -c $< -o $@.o
	@$(LD) $(STDC_LDFLAGS) -o $@ $(STDC_START) $@.o $(STDC_LIBS)

$(OUT)/retros/%-tcc: cases/%.c $(HEADERS) $(TCC) $(STDC_USR)/lib/libc.a
	@mkdir -p $(dir $@)
	@$(TCC) -w -o $@ $<

# Quick to redo, and it depends on the cross compiler, the C library and tests2.conf, so it always runs
tests2:
	@sh tests2.sh

$(OUT)/test.img: $(ROOT)/RetrOS-32-debug.img $(RETROS) $(wildcard cases/*) tests2
	@cp $(ROOT)/RetrOS-32-debug.img $@.tmp
	@mmd -i $@.tmp ::/t ::/t/tests2
	@mcopy -i $@.tmp $(RETROS) $(wildcard cases/*) ::/t/
	@cd $(OUT)/tests2 && mcopy -i ../../$@.tmp $$(ls | grep -v '\.compile$$\|\.expect$$\|\.c$$') ::/t/tests2/
	@mcopy -i $@.tmp $(TESTS2)/*.c $(TESTS2)/*.h ::/t/tests2/
	@mv $@.tmp $@
	@echo "[c harness] built $(words $(CASES)) programs into $@"

clean:
	@rm -rf $(OUT)
