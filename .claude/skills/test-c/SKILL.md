---
name: test-c
description: Use when changing RetrOS-32's standard C library (libc/), TinyCC (apps/tcc), or the kernel code programs depend on (memory, files, the terminal), to run C programs on RetrOS in QEMU and compare their output with the same programs on Linux.
---

# Testing C programs on RetrOS against Linux

`tests/c/` runs small C programs on RetrOS and compares what they print with what the same programs print on Linux. It checks the standard C library (`libc/`), TCC's output (`apps/tcc`), and the kernel underneath: loading programs, memory, files and the terminal.
- **Each program is built three ways:**
  - for Linux, with gcc and glibc in the Docker builder, where it runs to give the expected output;
  - for RetrOS with gcc on RetrOS's C library (variant `gcc`);
  - for RetrOS with TCC's cross compiler (variant `tcc`).
- **TinyCC's own tests** (`apps/tcc/tinycc/tests/tests2`, 143 programs) run too, as `tests2.NAME`.
  - `tests2.sh` builds them with the cross compiler, as `tests2.conf` says: skips with reasons, flags, extra files and arguments.
  - Each is compared with its `.expect` file, which has the compiler's messages first, then the program's output. As in TCC's own Makefile, runs of spaces count as one, and the test directory (`/t/2/` here) is taken out of the output.
  - A skipped test shows as `skip:` and its reason, and counts as expected.
- **The RetrOS programs run in QEMU,** through the `run-in-qemu` skill's `qctl.py`, so the human's OK to boot QEMU covers this too. A run takes a few seconds per program, plus a build.

## Run it

```
tests/c/run.py                 # build everything, then run every case
tests/c/run.py 'str*'          # cases whose names match a glob
tests/c/run.py -v --no-build   # skip the build, and print each failure's difference
```

- **What it builds,** in one builder container: `make img`, then `make -C tests/c`.
  - `tests/c/Makefile` builds `out/linux/`, `out/expected/` and `out/retros/`.
  - It puts the RetrOS programs and the case sources into `/t` on a copy of the image, `out/test.img`.
- **Then on the host:** `qctl.py` boots `out/test.img` (`qctl.py start --image ...`). For each program, it types `exec /t/NAME-VARIANT` in `/t` and waits for the kernel's `exec_cmd(): Finished exec` log line. It then takes the program's `term[/t/NAME-VARIANT]:` lines from the serial log.
- **A hang or a panic** restarts QEMU, and the run carries on. Each QEMU session's serial log is kept, as `out/serial-1.log` and so on.

## Reading the results

Each line is a case and variant, e.g. `memory.tcc`, then `pass`, or `fail:` and why:

| Reason | Means |
|---|---|
| `output` | RetrOS printed something different. `out/diff/CASE.VARIANT` has a unified diff: `-` lines are Linux's, `+` lines RetrOS's |
| `crashed: ...` | the log had a CPU exception or a page fault while it ran. Read the log with the `debug-kernel-panic` skill |
| `didn't finish within 60 s` | no `Finished exec` line came. QEMU was restarted |

- **Changed results** are marked `<- was ...` against `tests/c/expected.txt`, and the run exits non-zero.
- **When a change fixes or breaks a case on purpose,** update its line in `expected.txt` in the same commit.
- **To look at a failure in QEMU:** `qctl.py start --image tests/c/out/test.img`, then `qctl.py shell "cd /t"` and `qctl.py shell "exec /t/NAME-gcc"`.

## Writing a case

A case is `tests/c/cases/NAME.c`, a program that prints what it finds, the same on Linux and RetrOS.
- **Print what you check, not just "ok":** values, sizes, return codes, `errno` by name. A difference then shows what went wrong.
- **Lines:** RetrOS's log has a line at its newline, splits lines over 127 characters, and gets an unfinished last line when the program exits. `run.py` allows for all three.
- **Stay deterministic:** no addresses, times, process IDs or the current directory's name. It runs in an empty directory on Linux, and in `/t` on RetrOS.
- **Stay within RetrOS's limits:**
  - a process has at most 4 MB of heap, from about 3.5 MB of pages all processes share;
  - `stdin` is at its end (#113);
  - there are no exit codes, signals or `fork`.
- **`harness.h`** prints with `write()`, for cases that shouldn't depend on stdio.
- **Native mode:** the cases' sources are in `/t` too, for compiling on RetrOS itself.
