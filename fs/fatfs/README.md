# FatFs

[FatFs](https://elm-chan.org/fsw/ff/) by ChaN is the FAT file system code RetrOS-32 uses.

- **Version:** R0.16 (22 July 2025), with the author's patch-1 (13 September 2025) and patch-2 (10 July 2026) applied to `ff.c`, from the [patches page](https://elm-chan.org/fsw/ff/patches.html). Patch-2 fixes CVE-2026-6682: a boot sector with a broken FAT size could wreck the files or crash the system.
- **License:** the FatFs license in `LICENSE.txt`. It's BSD-style, with one condition: keep the copyright notice in the source.

## What's here

| File | From |
|---|---|
| `ff.c` | Upstream, with patch-1 and patch-2 |
| `ff.h`, `diskio.h`, `ffunicode.c`, `LICENSE.txt` | Upstream, unchanged |
| `ffconf.h` | Upstream's settings file, with RetrOS's settings below |
| `string.h` | RetrOS's: maps `ff.c`'s `<string.h>` to the kernel's `libc.h` |
| `README.md` | This file |

Line endings are converted from upstream's CRLF to LF. Upstream's sample `diskio.c` and `ffsystem.c` aren't included: RetrOS provides the disk, clock, memory and lock functions FatFs calls.

## Settings

Changed from upstream's defaults in `ffconf.h`:

| Setting | Value | Why |
|---|---|---|
| `FF_USE_MKFS` | 1 | `f_mkfs` formats the virtual disk and the file system harness's images |
| `FF_USE_LABEL` | 1 | `f_setlabel` and `f_getlabel`, for the volume label |
| `FF_CODE_PAGE` | 437 | The IBM PC character set, which VGA text mode also uses. Short names are stored in it, and long names are converted to it |
| `FF_USE_LFN` | 3 | Long file names, with the name buffer taken from the kernel heap (`ff_memalloc`) |
| `FF_FS_REENTRANT` | 1 | Several threads use the file system at once. FatFs locks the volume with `ff_mutex_*` |

Everything else is upstream's default. In particular:
- **Relative paths are off** (`FF_FS_RPATH` 0): the kernel resolves `.`, `..` and the current directory itself.
- **exFAT is off.**
- **File locking is off** (`FF_FS_LOCK` 0): the Editor keeps a file open while the compiler reads it. #102 has the plan to revisit it.

## Upgrading

1. Download the release's zip, and convert the line endings of the files you need: `tr -d '\r' < in > out`.
2. Apply the patches listed on the patches page to `ff.c`, converted to LF too: `patch -p0 ff.c < patch.diff`.
3. Copy `ff.c`, `ff.h`, `diskio.h`, `ffunicode.c` and `LICENSE.txt` over these.
4. Copy the new `ffconf.h` over this one, then redo the settings above. Diff it against the old one first: new releases add options.
5. Run the file system harness (`tests/fs/run.sh`, see the `test-filesystem` skill) and boot it in QEMU.
