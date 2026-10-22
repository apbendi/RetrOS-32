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

The files in `tinycc/` are upstream's, unmodified, except for the patches listed here. RetrOS's own build files and settings live next to it in `apps/tcc/`.

None yet.

## Upgrading

1. Replace `tinycc/` with the new upstream tree: `git -C <clone> archive <commit> | tar -x -C tinycc`.
2. Reapply the patches listed above, and update the commit in this file.
3. Run the C harness, including TCC's own tests, and check the result in QEMU.
