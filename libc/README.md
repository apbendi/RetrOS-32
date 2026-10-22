# RetrOS-32's C library

The standard C library for RetrOS programs: the ones TCC compiles on RetrOS, TCC itself, and anything else written in standard C. It's built from [musl](https://musl.libc.org/), with RetrOS's own startup code, system calls, `errno` and `malloc` in place of musl's Linux ones.

It's separate from `lib/libc.c` and `include/libc.h`, the kernel's and the existing apps' library, which isn't standard C.

## musl

`musl/` is musl's whole release tree, so that upgrading is a copy and more of musl can be built later (#112). Only the part RetrOS needs is built: `Makefile`'s `MUSL_SRCS` lists it.

| | |
|---|---|
| Version | 1.2.6, from <https://musl.libc.org/releases/musl-1.2.6.tar.gz> |
| SHA-256 | `d585fd3b613c66151fc3249e8ed44f77020cb5e6c1e635a616d3f9f82460512a` |
| Checked | the SHA-512 matches Alpine Linux's musl 1.2.6 package (`main/musl/APKBUILD`). The release's GPG signature wasn't checked |
| License | MIT (`musl/COPYRIGHT`) |

**Patches:** musl's three published security fixes for 1.2.6, unmodified, in `patches/`. musl's advisory says every 1.2.6 user should apply them:

| Patch | Fixes | Built here |
|---|---|---|
| `qsort-CVE-2026-40200.patch` | [stack corruption in `qsort` on 32-bit systems](https://www.openwall.com/lists/musl/2026/04/10/3) | yes |
| `regcomp-CVE-2026-107891.patch` | [integer overflows in `regcomp`](https://www.openwall.com/lists/musl/2026/10/21/1) | not yet |
| `iconv-CVE-2026-6042.patch` | [slowness and wrong mappings in `iconv`](https://www.openwall.com/lists/oss-security/2026/04/09/19). It adds `src/locale/gb18030utf.h` | not yet |

The tree is otherwise unmodified. RetrOS's own code goes next to it, in `libc/`.

## RetrOS's part

- **`crt/`:** `crt1.s`, where every program starts. RetrOS starts a program at its first byte, with `argc` in `%ebx` and `argv` in `%ecx`, and its threads there too, with the thread's function in `%edx`. `_start` is in a section of its own, `.text.retros_entry`, which TCC and `retros.ld` put first. `crti.s` and `crtn.s` are empty: constructors run from `.init_array`.
- **`src/`:**
  - `start.c`: the thread structure musl needs, the empty environment, constructors, then `main` and `exit`;
  - `unistd.c`, `fcntl.c`, `stat.c`, `time.c`, `uio.c`, `rename.c`: POSIX calls on RetrOS's system calls, with RetrOS's error codes mapped to `errno` (`retros.c`). `rename` replaces an existing file, as POSIX says, which RetrOS doesn't do itself;
  - `linux.c`: the Linux system calls musl's own code makes, like stdio's `writev`, `open` and `ioctl`, done with those functions;
  - `malloc.c`: `malloc` and the rest, dividing large blocks from the kernel's `malloc` system call;
  - `abort.c`, and `libgcc.c` for two 64-bit division helpers TCC's `libtcc1.a` lacks.
- **`arch/`:** two of musl's i386 headers, replaced by coming first in the include path:
  - `pthread_arch.h`: musl finds the current thread through `%gs`, which RetrOS doesn't set up, so it's one static structure;
  - `syscall_arch.h`: musl's Linux system calls go to `src/linux.c`, and any it doesn't do returns `ENOSYS`.
- **The kernel's headers** give system call numbers, error codes and file flags: `syscall_helper.h`, `errors.h` (with `ERRORS_CODES_ONLY`) and `fs/fs_types.h`.

**Descriptors 0 to 2** aren't RetrOS file descriptors, which start at 3. `stdout` and `stderr` write to the terminal in one system call each; `stdin` is always at its end, since programs can't read the terminal yet (#113). The terminal counts as a tty, so `stdout` is line-buffered.

**stdio** is all of musl's, except `popen` and `pclose` (RetrOS can't start a process that way), the temporary files (`tmpfile`, `tmpnam`, `tempnam`), and, until the number scanners come, the `scanf` family. `freopen` with a new path fails with `ENOSYS`, because RetrOS can't duplicate a descriptor.

**Not yet:** `dup`, thread safety (a program's threads share `malloc`, `errno` and the rest without locks), environment variables, exit codes (RetrOS doesn't keep them), and `fork`, signals and the like.

## Building programs

- **On RetrOS, and with the cross compiler:** TCC finds the headers in `/usr/include` and `libc.a` and the crt files in `/usr/lib`. In the repo, they're installed in `rootfs/usr`, where the cross compiler looks.
- **With the builder's gcc:** include `program.mk`, which links with `retros.ld` and `libgcc.a`.

## Upgrading musl

1. Unpack the new release over `musl/`, after removing the old tree.
2. Apply the patches in `patches/` that the new release doesn't already include, and drop the rest.
3. Update the version and checksum above, rebuild, and run the C harness.
