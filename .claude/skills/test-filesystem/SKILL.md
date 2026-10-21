---
name: test-filesystem
description: Use when changing RetrOS-32's file system code (fs/, the FAT16 layer and fs.c) or investigating a file system bug, to check RetrOS against mtools and fsck.fat on disk images, without booting.
---

# Testing the file system against mtools and fsck.fat

`tests/fs/` runs RetrOS's own FAT16 code on disk image files and compares what it does with mtools, the standard FAT tools, and `fsck.fat`.
- **It tests what ships:** `fstool` links `bin/fs.o` (the file layer) and `bin/fatfs.o` (FatFs and RetrOS's code around it), the objects the kernel links. `tests/fs/host.c` stands in for the kernel functions they call.
- **It runs in the Docker builder,** which has mtools, `fsck.fat`, `mkfs.fat` and `gcc -m32`.
- **A full run takes about 30 seconds.** It needs no QEMU and no OK from the human.

## Run it

```
docker compose run --rm --entrypoint tests/fs/run.sh builder              # every case
docker compose run --rm --entrypoint tests/fs/run.sh builder 'write.*'    # cases whose names match a glob
docker compose run --rm --entrypoint tests/fs/run.sh builder -v           # with every failure's details
```

- **It rebuilds `fs/` and `fstool` first,** so it always tests the current source.
- **Some cases use `RetrOS-32-debug.img`:** `read.real-image`, `read.dotdot` and `write.real-image`. They fail with `setup` if there's no image. Run `docker compose up` first.

## Reading the results

Each case starts two copies of one image:
- **R,** which RetrOS's code changes through `fstool`;
- **M,** which mtools changes the same way.

Each line of output is a case's name, then `pass`, or `fail:` and the checks that failed:

| Check | Fails when |
|---|---|
| `fsck` | `fsck.fat` finds problems in R that weren't there before RetrOS changed it |
| `same` | mtools reads R and M differently: a path, a size or a file's content |
| `reads` | RetrOS reads R differently from how mtools reads M |
| `tool` | RetrOS's code read or wrote outside the disk, or wrote past the end of a kernel allocation |
| `lock` | RetrOS released a lock it didn't hold. It's kept apart because the old FAT16 code did it for every new cluster (#38) |
| `op` | a RetrOS operation failed, or succeeded when it should have failed |
| `mount` | RetrOS didn't mount the image |
| `path`, `setup`, `ref` | case-specific checks. `ref` means mtools itself failed, which is a harness problem |

- **Changed results:** `<- was ...` marks a result that differs from `tests/fs/expected.txt`, and the run exits non-zero. `-v` isn't needed to see why: changed results always print their details.
- **What a failing case leaves:** `tests/fs/out/CASE/` has:
  - `notes.txt`;
  - the trees that were compared (`tree-M-mtools`, `tree-R-mtools`, `tree-R-retros`);
  - fsck's findings from before.

  `-k` keeps every case's files, plus its `R.img` and `M.img`.
- **How to read a tree:** lines are `D /PATH` and `F /PATH SIZE SHA`, with uppercase 8.3 names. In the notes, `<` lines are what M has and `>` lines what R has.

## After changing fs/

1. **Run every case.**
2. **Cases that changed on purpose:** if a case now passes, or fails differently, as intended, update its line in `expected.txt` in the same commit. Name the cases that changed in the commit message.
3. **Any other change** is a regression, or a harness problem. Start with `out/CASE/notes.txt`.
4. **Still check in QEMU** (the `run-in-qemu` skill) before calling it done. The harness doesn't cover:
   - syscalls, the shell or apps;
   - the kernel's disk drivers;
   - several threads at once.

## Adding a case

Add a function to `tests/fs/cases.sh`, and an `add` line naming it:

```
write_grow() {
    begin fresh || return      # R and M start as copies of a base image
    put /A.TXT 100             # the same change to both: fstool on R, mtools on M
    put /A.TXT 3000
    check                      # fsck, same and reads
}
add write.grow "RetrOS replaces a file with a longer one" write_grow
```

- **Base images** (`base_image` in `run.sh`):

  | Name | What it is |
  |---|---|
  | `real` | `RetrOS-32-debug.img` |
  | `fresh` | 32 MB, formatted by RetrOS |
  | `small` | 3 MB, formatted by RetrOS |
  | `virtual` | 1 MB, formatted by RetrOS, like `login`'s virtual disk |
  | `standard` | `mkfs.fat` defaults |
  | `standard-oem` | `mkfs.fat` defaults, with RetrOS's name in the boot sector |

- **Setting both up before RetrOS acts:** `seed MTOOLS-COMMAND ARGS`, e.g. `seed mmd ::/SUB`. Then call `ready`, which records fsck's starting findings.
- **Making the same change to both:**
  - `put PATH N`, `put_chunks PATH N CHUNK`, `put_at PATH OFFSET N`, `create PATH`;
  - for anything else, `rfs ARGS` on R and `ref MTOOLS-COMMAND ARGS` on M.
- **An operation RetrOS should refuse:** `refused ARGS`.
- **Checking:** `check` runs `check_fsck`, `check_same` and `check_reads`. Call them one by one when a case needs only some.
- **Test data:** `data N TAG` makes an N-byte file that never repeats, so a misplaced piece shows.
- **Naming:** name a case by area (`format`, `read`, `write`, `limits`, `ops`), and put the issue number in its description when there is one.
- **New cases** show `<- new case` until they're in `expected.txt`.

## Using fstool by hand

`tests/fs/bin/fstool IMAGE OPERATION ...` runs one operation. Its usage is at the top of `tests/fs/fstool.c`. In the builder:

```
docker compose run --rm --entrypoint bash builder
make -s -C fs && make -s -C tests/fs                         # if run.sh hasn't built it
truncate -s 32M /tmp/d.img && tests/fs/bin/fstool /tmp/d.img format
echo hello | tests/fs/bin/fstool /tmp/d.img write /A.TXT
FSTOOL_DEBUG=1 tests/fs/bin/fstool /tmp/d.img cat /A.TXT    # with the file system's own dbgprintf log
MTOOLS_SKIP_CHECK=1 mdir -i /tmp/d.img ::/
fsck.fat -n /tmp/d.img
```

- **Exit status:** 0 done, 1 the operation failed, 3 the image didn't mount, 4 the operation worked but a problem happened.
- **Problems** are printed as `fstool: problem: ...` on stderr, whatever the exit status.
- **The clock is fixed** at 2 January 2026, 03:04:06, so the same steps write the same bytes.

## Things that have tripped this up

- **mtools refuses RetrOS images** without `MTOOLS_SKIP_CHECK=1`, because RetrOS writes 0 heads and sectors per track. `run.sh` sets it.
- **Long names:**
  - `MTOOLS_NO_VFAT=1` stops mtools *writing* long names, but `mdir` still *lists* them. So `run.sh` takes short names from `mdir`'s first 12 columns.
  - In mtools 4.0.32, `mshortname` drops the dot before a one-letter extension (`add.c` gives `ADDC`), so it can't supply them.
- **The harness compares `fsck.fat`'s findings before and after a case, never its exit status.** The old FAT16 code's images always had findings (#101), and a case can start from a damaged image.
- **`fsck.fat` gives up early on some damage,** such as a FAT too small for the volume's clusters (#99). There, finding nothing new proves little, so a case like `limits.full-disk` checks `tool` too.
- **The builder has Linux's `vfat` driver but not `msdos`,** so mounting can't show short names either.
- **There's no Python in the builder,** which is why the runner is bash.
