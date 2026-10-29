---
name: check-change
description: Use after editing RetrOS-32 kernel, library or app code, before handing the change back or drafting a commit, to check it as far as possible without a full Docker build or booting QEMU.
---

# Checking a RetrOS-32 change without booting

The real build runs GCC in Docker, and testing means booting QEMU. Both are slow, and booting needs the human's OK; with it, the `run-in-qemu` skill can boot it for you. Do these steps first, then tell the human exactly what's left for them to test.

## 1. Syntax and warnings: always

```
.claude/skills/check-change/check.sh            # every .c/.cpp/.s changed since HEAD
.claude/skills/check-change/check.sh <file>...  # or specific files
```

- **What it does:** it compiles each file with clang, using the flags the build uses for that kind of file (kernel, `lib/`, or app). It reports only diagnostics that HEAD's version of the same file doesn't have, because many files already have warnings.
- **For apps,** it also lists the runtime-library (libgcc) helpers the object needs. Apps don't link libgcc, so any helper there is a link error waiting to happen. It's usually 64-bit `/` or `%`.
- **Generated headers:** kernel sources that include `images.h` need `bin/gen/images.h`, which the build writes from `images/`. Run a build first, or they stop at that include.
- **What it can't catch:** clang isn't GCC, and GCC warns about some things clang doesn't (e.g. `?:` mixing an enum with a non-enum operand). It also doesn't link the kernel.
- **Fix every new diagnostic,** or explain why it's expected.

## 2. Host test the logic: when the change has logic worth testing

Use this for bounds checks, geometry, buffer copies, event handling and state machines. Test the **real** code, not a copy you rewrote:

- **A kernel or library C function:**
  1. Use a short Python script to cut the function's source text out of the file, plus any small helpers or macros it needs.
  2. Paste it into a C harness in your scratch directory, with stub types (e.g. a `struct window` with just the fields used) and a stub `putpixel`.
  3. Put guard bytes around buffers and check that they're untouched.
  4. Run the same cases against `git show HEAD:<file>` to show the bug before the fix.
- **A C++ app:**
  1. In a scratch directory, write a stub `utils/Graphics.hpp` with a do-nothing `Window` class and `gfx_*` functions that record their calls. Also add an empty `libc.h` and a `colors.h` with the constants used.
  2. Include the app's `.cpp` after `#define private public`, with its `main` renamed so the test can have its own.
  3. Build it with the host `c++` and `-I <scratch dir>`.

Keep harnesses in a scratch directory, not in the repo.

## 3. Check protocol and build changes

- **A struct shared between `lib/` and the kernel:** apps need relinking. A normal `docker compose up` does that, so no clean is needed. Old app binaries would send garbage in the new field, so mention it.
- **A `Makefile` change:** reason through what each target now links, and note it for the human to verify in the build output.

## 4. Hand back

- **State what you checked and how,** e.g. "syntax-checked with clang, no new warnings; host-tested 6 cases; not built with GCC or booted".
- **Give a short test script** for the human: what to open, what to do, and what they should see, including any regressions worth checking.
- **When they report a crash,** ask for the full serial log (see the `debug-kernel-panic` skill).
