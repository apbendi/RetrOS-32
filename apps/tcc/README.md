# TinyCC for RetrOS-32

`tinycc/` is [TinyCC](https://bellard.org/tcc/) (TCC), the C compiler installed as `/bin/tcc.o`. It's a copy of the whole upstream tree, not a submodule.

| | |
|---|---|
| Source | <https://github.com/TinyCC/tinycc>, a mirror of `git://repo.or.cz/tinycc.git` (branch `mob`) |
| Commit | [`e23dabed154a8495f3ac1ff3cc9d04fa1c207893`](https://github.com/TinyCC/tinycc/tree/e23dabed154a8495f3ac1ff3cc9d04fa1c207893), 2026-10-16, version `0.9.28rc` |
| License | GNU LGPL version 2.1 (`tinycc/COPYING`). Its runtime library, `tinycc/lib/libtcc1.c`, is GPL with a linking exception, so linking it into a program puts no conditions on the program. `tinycc/RELICENSING` lists contributors who also allow their code under other licenses |

## What's built

Only the i386 compiler: `tcc.c` with `ONE_SOURCE`, which includes the shared sources and `i386-*.c`. The other targets' files, `win32/` and `tests/` are in the tree so that upgrading is a copy, and RetrOS's image gets only what's built. The test programs in `tinycc/tests/tests2` are run by RetrOS's C harness.

## RetrOS's changes

The files in `tinycc/` are upstream's, unmodified, except for the patches listed here. Each is marked with a `RetrOS` comment in the code. RetrOS's own build files and settings live next to it in `apps/tcc/`:
- **`config.h`** takes the place of the one TCC's configure script writes: the i386 target, the RetrOS target OS, paths, and the default switches that make programs flat binaries at `0x1000000`.
- **`Makefile`** builds the cross compiler and `libtcc1.a` (below).

**Patches:**
1. **A RetrOS target OS** (`tcc.h`, `tccpp.c`, `tcc.c`, `libtcc.c`). With `TARGETOS_RetrOS`, TCC predefines `__retros__` instead of `__linux__`, `__linux`, `__unix__` and `__unix`. In `tcc.h`, it keeps TCC from defaulting to `TARGETOS_Linux`. `tcc -v` says "i386 RetrOS", and `-dumpmachine` says `i386-retros`.
2. **Code first** (`tccelf.c`, `sort_sections`). RetrOS starts a program at its first byte, but TCC puts read-only data before code. For RetrOS, executable sections sort first, so `crt1.o`'s `_start`, linked first, is at `0x1000000`.
3. **`.bss` in flat binaries** (`tccelf.c`, `tcc_output_binary`). TCC leaves `SHT_NOBITS` sections out of a flat binary, and RetrOS's loader maps only what's in the file. For RetrOS, they're written as zeros, placed by address.

## The cross compiler

`make -C apps/tcc`, which the normal build runs, makes `apps/tcc/bin/i386-retros-tcc`, a compiler that runs in the Docker builder and makes RetrOS programs. By default they're linked against RetrOS's C library (`libc/`), with its headers, as the compiler on RetrOS will link them: `crt1.o`, `libc.a` and `libtcc1.a`. It's built from the same sources and `config.h`, with its paths pointing into the repo's `rootfs/`, where the disk's files come from: TCC's headers and `libtcc1.a` go to `rootfs/lib/tcc`. `libtcc1.a`, TCC's runtime library (64-bit arithmetic and the like), is built with it from `tinycc/lib`, minus the Linux-only pieces.

## On RetrOS

`tcc.o`, installed as `/bin/tcc.o`, is TCC built with the builder's gcc on RetrOS's C library (`libc/program.mk`), from the same sources and `config.h`, with the paths as they are on RetrOS's disk. In the shell:

```
cd "/my projects"
exec /bin/tcc.o main.c -o main
exec main
```

**Testing:** the C harness (`tests/c`, the `test-c` skill) runs the harness's own programs and TCC's tests built with it on RetrOS.

## Upgrading

1. Replace `tinycc/` with the new upstream tree: `git -C <clone> archive <commit> | tar -x -C tinycc`.
2. Reapply the patches listed above, and update the commit in this file.
3. Run the C harness, including TCC's own tests, and check the result in QEMU.
