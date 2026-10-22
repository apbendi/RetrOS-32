/**
 * RetrOS-32's settings for TinyCC, in place of the config.h that TCC's configure script writes.
 *
 * Both compilers use it: the cross compiler that runs in the builder (bin/i386-retros-tcc) and the one that runs on
 * RetrOS (/bin/tcc.o). The cross compiler's Makefile sets CONFIG_SYSROOT and CONFIG_TCCDIR to the repo's rootfs/, so
 * it finds headers and libraries where RetrOS will.
 */
#define TCC_VERSION "0.9.28rc"

#define CC_NAME CC_gcc
#define GCC_MAJOR 12
#define GCC_MINOR 2

/* The target: 32-bit x86, running RetrOS. TARGETOS_RetrOS predefines __retros__ instead of __linux__ and __unix__ */
#ifndef TCC_TARGET_I386
# define TCC_TARGET_I386 1
#endif
#define TARGETOS_RetrOS 1

/* tccdefs.h is compiled in, as tccdefs_.h, rather than read at run time */
#define CONFIG_TCC_PREDEFS 1

/* Off: stack backtraces and bounds checking need signals and the like; semaphores need pthreads */
#define CONFIG_TCC_BACKTRACE 0
#define CONFIG_TCC_BCHECK 0
#define CONFIG_TCC_SEMLOCK 0

/* No dynamic linking: programs are static, and tcc -run can't load libraries */
#define CONFIG_TCC_STATIC 1

/* Where things are on RetrOS's disk. {B} is CONFIG_TCCDIR. */
#ifndef CONFIG_SYSROOT
# define CONFIG_SYSROOT ""
#endif
#ifndef CONFIG_TCCDIR
# define CONFIG_TCCDIR "/lib/tcc"
#endif
#define CONFIG_TCC_SYSINCLUDEPATHS "{B}/include" PATHSEP CONFIG_SYSROOT "/usr/include"
#define CONFIG_TCC_LIBPATHS "{B}" PATHSEP CONFIG_SYSROOT "/usr/lib"
#define CONFIG_TCC_CRTPREFIX CONFIG_SYSROOT "/usr/lib"
#define CONFIG_TCC_ELFINTERP "-"

/**
 * Programs are flat binaries at 0x1000000, where RetrOS loads them, linked statically. Object files (-c) stay ELF.
 * A program starts at its first byte, so crt1.o's _start has to come first.
 */
#define CONFIG_TCC_SWITCHES "-static -Wl,--oformat=binary -Wl,-Ttext=0x1000000"
