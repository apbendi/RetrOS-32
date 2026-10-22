# RetrOS-32's C library

The standard C library for RetrOS programs: the ones TCC compiles on RetrOS, TCC itself, and anything else written in standard C. It's built from [musl](https://musl.libc.org/), with RetrOS's own startup code, system calls, `errno` and `malloc` in place of musl's Linux ones.

It's separate from `lib/libc.c` and `include/libc.h`, the kernel's and the existing apps' library, which isn't standard C.

## musl

`musl/` is musl's whole release tree, so that upgrading is a copy and more of musl can be built later (#112). Only the part RetrOS needs is built; nothing builds it yet.

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

## Upgrading musl

1. Unpack the new release over `musl/`, after removing the old tree.
2. Apply the patches in `patches/` that the new release doesn't already include, and drop the rest.
3. Update the version and checksum above, rebuild, and run the C harness.
