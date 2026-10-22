# RetrOS-32's standard C library (libc/README.md), built with the builder's gcc.
#
# libc.a is musl's portable code plus RetrOS's own (src/): startup, system calls, errno support and malloc. arch/
# replaces two of musl's i386 headers by coming first in the include path, so musl's files stay unmodified.
#
# It installs what programs need where they'll be on RetrOS's disk: headers in rootfs/usr/include, and libc.a and the
# crt files in rootfs/usr/lib. The cross compiler (apps/tcc) finds them there, and program.mk builds programs with gcc.

ROOT := $(abspath ..)
include $(ROOT)/make/common.mk

MUSL := musl
BUILD_DIR := bin
OBJ := $(BUILD_DIR)/obj
GEN := $(BUILD_DIR)/include
USR_INCLUDE := $(ROOT)/rootfs/usr/include
USR_LIB := $(ROOT)/rootfs/usr/lib

# musl's flags, from its configure script, less -ffunction-sections and -fdata-sections, which only help GNU ld drop
# unused code: TCC would make every function a section of its own
LIBC_CFLAGS := -m32 -march=i486 -std=c99 -ffreestanding -nostdinc -fno-pie -fno-stack-protector \
	-fexcess-precision=standard -frounding-math -fno-strict-aliasing -fno-tree-loop-distribute-patterns \
	-O2 -fomit-frame-pointer -fno-unwind-tables -fno-asynchronous-unwind-tables -Wa,--noexecstack \
	-D_XOPEN_SOURCE=700 -Werror=implicit-function-declaration -Werror=implicit-int -Werror=pointer-sign \
	-Werror=pointer-arith -Werror=int-conversion -Werror=incompatible-pointer-types
LIBC_INCLUDES := -I arch -I $(MUSL)/arch/i386 -I $(MUSL)/arch/generic -I $(OBJ)/internal -I $(MUSL)/src/include \
	-I $(MUSL)/src/internal -I $(GEN) -I $(MUSL)/include
# RetrOS's own code also sees the kernel's headers for system call numbers and error codes, after musl's
# (-Wno-unused-value: musl's internal headers trip it with arch/syscall_arch.h's constant results)
RETROS_CFLAGS := -idirafter $(ROOT)/include -DERRORS_CODES_ONLY -Wall -Wextra -Wno-unused-parameter -Wno-unused-value

# stdio without what RetrOS can't do: popen and pclose (no processes to start), and temporary files (no random names
# yet)
STDIO_LEFT_OUT := popen.c pclose.c tempnam.c tmpfile.c tmpnam.c

# The parts of musl that are built
MUSL_SRCS := \
	$(filter-out $(addprefix $(MUSL)/src/stdio/,$(STDIO_LEFT_OUT)),$(wildcard $(MUSL)/src/stdio/*.c)) \
	$(wildcard $(MUSL)/src/multibyte/*.c) \
	$(wildcard $(MUSL)/src/string/*.c) \
	$(wildcard $(MUSL)/src/ctype/*.c) \
	$(addprefix $(MUSL)/src/stdlib/,abs.c atoi.c atol.c atoll.c bsearch.c div.c imaxabs.c imaxdiv.c labs.c ldiv.c \
		llabs.c lldiv.c atof.c strtod.c strtol.c wcstod.c wcstol.c qsort.c qsort_nr.c ecvt.c fcvt.c gcvt.c) \
	$(addprefix $(MUSL)/src/internal/,shgetc.c intscan.c floatscan.c) \
	$(addprefix $(MUSL)/src/env/,getenv.c setenv.c unsetenv.c putenv.c clearenv.c secure_getenv.c) \
	$(addprefix $(MUSL)/src/time/,__map_file.c __month_to_secs.c __secs_to_tm.c __tm_to_secs.c __tz.c __utc.c \
		__year_to_secs.c asctime.c asctime_r.c ctime.c ctime_r.c difftime.c gmtime.c gmtime_r.c localtime.c \
		localtime_r.c mktime.c strftime.c timegm.c) \
	$(addprefix $(MUSL)/src/mman/,mmap.c munmap.c) \
	$(addprefix $(MUSL)/src/exit/,exit.c atexit.c) \
	$(addprefix $(MUSL)/src/errno/,__errno_location.c strerror.c) \
	$(MUSL)/src/env/__environ.c \
	$(MUSL)/src/internal/libc.c \
	$(MUSL)/src/internal/syscall_ret.c \
	$(addprefix $(MUSL)/src/locale/,__lctrans.c c_locale.c langinfo.c) \
	$(addprefix $(MUSL)/src/math/,__fpclassifyl.c __signbitl.c frexpl.c scalbn.c scalbnl.c ldexpl.c copysignl.c \
		fabsl.c fmodl.c) \
	$(MUSL)/src/thread/__lock.c

RETROS_SRCS := $(wildcard src/*.c)

# musl's assembly: setjmp and longjmp
MUSL_ASM := $(MUSL)/src/setjmp/i386/setjmp.s $(MUSL)/src/setjmp/i386/longjmp.s

MUSL_OBJS := $(patsubst $(MUSL)/src/%.c,$(OBJ)/musl/%.o,$(MUSL_SRCS)) $(patsubst $(MUSL)/src/%.s,$(OBJ)/musl/%.o,$(MUSL_ASM))
RETROS_OBJS := $(patsubst src/%.c,$(OBJ)/retros/%.o,$(RETROS_SRCS))
CRT_OBJS := $(BUILD_DIR)/crt1.o $(BUILD_DIR)/crti.o $(BUILD_DIR)/crtn.o
GENH := $(GEN)/bits/alltypes.h $(GEN)/bits/syscall.h $(OBJ)/internal/version.h

.PHONY: all install clean
all: $(BUILD_DIR)/libc.a $(CRT_OBJS) install

# Headers musl's build generates
$(GEN)/bits/alltypes.h: $(MUSL)/arch/i386/bits/alltypes.h.in $(MUSL)/include/alltypes.h.in $(MUSL)/tools/mkalltypes.sed
	$(call make_dir,$(GEN)/bits)
	$(QUIET)sed -f $(MUSL)/tools/mkalltypes.sed $(MUSL)/arch/i386/bits/alltypes.h.in $(MUSL)/include/alltypes.h.in > $@

$(GEN)/bits/syscall.h: $(MUSL)/arch/i386/bits/syscall.h.in
	$(call make_dir,$(GEN)/bits)
	$(QUIET)cp $< $@
	$(QUIET)sed -n -e s/__NR_/SYS_/p < $< >> $@

$(OBJ)/internal/version.h: $(MUSL)/VERSION
	$(call make_dir,$(OBJ)/internal)
	$(QUIET)printf '#define VERSION "%s"\n' "$$(cat $<)" > $@

$(OBJ)/musl/%.o: $(MUSL)/src/%.c $(GENH)
	$(call make_dir,$(dir $@))
	$(QUIET)$(CC) $(LIBC_CFLAGS) $(LIBC_INCLUDES) -c $< -o $@

$(OBJ)/musl/%.o: $(MUSL)/src/%.s
	$(call make_dir,$(dir $@))
	$(QUIET)$(CC) -m32 -c $< -o $@

$(OBJ)/retros/%.o: src/%.c $(GENH)
	$(call make_dir,$(dir $@))
	$(QUIET)$(CC) $(LIBC_CFLAGS) $(LIBC_INCLUDES) $(RETROS_CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: crt/%.s
	$(call make_dir,$(BUILD_DIR))
	$(QUIET)$(CC) -m32 -c $< -o $@

$(BUILD_DIR)/libc.a: $(MUSL_OBJS) $(RETROS_OBJS)
	$(QUIET)$(RM_F) $@
	$(QUIET)$(AR) rcs $@ $^
	$(QUIET)echo "[libc] built libc.a"

-include $(RETROS_OBJS:.o=.d)

# What RetrOS's disk gets in /usr/include and /usr/lib, laid out as musl's install-headers lays it out
install: $(BUILD_DIR)/libc.a $(CRT_OBJS)
	$(call make_dir,$(USR_INCLUDE)/bits)
	$(call make_dir,$(USR_LIB))
	$(QUIET)cp -Rp $(MUSL)/include/. $(USR_INCLUDE)/
	$(QUIET)$(RM_F) $(USR_INCLUDE)/alltypes.h.in
	$(QUIET)cp -p $(MUSL)/arch/generic/bits/*.h $(USR_INCLUDE)/bits/
	$(QUIET)cp -p $(MUSL)/arch/i386/bits/*.h $(USR_INCLUDE)/bits/
	$(QUIET)cp -p $(GEN)/bits/alltypes.h $(GEN)/bits/syscall.h $(USR_INCLUDE)/bits/
	$(QUIET)cp -p $(BUILD_DIR)/libc.a $(CRT_OBJS) $(USR_LIB)/

clean:
	$(QUIET)$(RM_RF) $(BUILD_DIR) $(USR_INCLUDE) $(USR_LIB)
