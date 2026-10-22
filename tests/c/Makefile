# The C harness's build half (tests/c/run.py runs it in the Docker builder, then runs the programs in QEMU).
#
# For each program in cases/:
# - out/linux/NAME, built with gcc and glibc for Linux, and out/expected/NAME.out, what it prints there;
# - out/retros/NAME-gcc, built with gcc on RetrOS's C library (libc/program.mk);
# - out/retros/NAME-tcc, built with TCC's cross compiler (apps/tcc).
# out/test.img is a copy of RetrOS-32-debug.img with the RetrOS programs and the sources in /t.

ROOT := $(abspath ../..)
include $(ROOT)/libc/program.mk

TCC := $(ROOT)/apps/tcc/bin/i386-retros-tcc
OUT := out
CASES := $(basename $(notdir $(wildcard cases/*.c)))
HEADERS := $(wildcard cases/*.h)

LINUX := $(CASES:%=$(OUT)/linux/%)
EXPECTED := $(CASES:%=$(OUT)/expected/%.out)
RETROS := $(CASES:%=$(OUT)/retros/%-gcc) $(CASES:%=$(OUT)/retros/%-tcc)

.PHONY: all clean
all: $(EXPECTED) $(OUT)/test.img

# Keep the Linux builds, which make would otherwise delete as intermediate files
.SECONDARY: $(LINUX)

$(OUT)/linux/%: cases/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	@gcc -m32 -O2 -w -o $@ $< -lm

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

$(OUT)/test.img: $(ROOT)/RetrOS-32-debug.img $(RETROS) $(wildcard cases/*)
	@cp $(ROOT)/RetrOS-32-debug.img $@.tmp
	@mmd -i $@.tmp ::/t
	@mcopy -i $@.tmp $(RETROS) $(wildcard cases/*) ::/t/
	@mv $@.tmp $@
	@echo "[c harness] built $(words $(CASES)) programs into $@"

clean:
	@rm -rf $(OUT)
