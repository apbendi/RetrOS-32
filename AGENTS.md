# Notes for agents working on RetrOS-32

RetrOS-32 is a 32-bit x86 hobby OS: kernel, window server, drivers, networking, a FAT16 filesystem, and C/C++ userspace apps. This file covers how to build, run, check and debug it, and the parts of the codebase that have been easy to get wrong.

This repo is a fork, `apbendi/RetrOS-32`, of `joexbayer/RetrOS-32`. Work happens on the `development` branch, and issues are tracked on the fork (`gh issue list --repo apbendi/RetrOS-32`).

`.claude/skills/` has step-by-step procedures for recurring tasks:
- `debug-kernel-panic`: working out a crash from the serial log.
- `check-change`: checking a change without booting. Its `check.sh` runs the clang check described below.
- `file-issue`: this fork's issue format and the `gh` steps.

## Layout

| Path | What's there |
|---|---|
| `kernel/` | Kernel core: scheduler, PCBs, memory (`kmem.c`, `vmem.c`, `memory.c`), interrupts (`arch/`, `irs_entry.s`), syscalls, symbols (`ksyms.c`) |
| `kernel/kthreads/` | Kernel threads with windows: `shell`, `taskbar` (Start menu), `kclock`, `sysinf`, `about`, … |
| `graphics/` | Window server (`windowserver.c`), window manager (`windowmanager.c`), windows (`window.c`), drawing (`gfxlib.c`), the gfx syscall (`api.c`) |
| `drivers/` | VBE, keyboard, mouse, ATA, e1000, … |
| `lib/` | Userspace libraries linked into apps (`graphics.c`, `libc.c` with the sin/cos tables, …) |
| `apps/` | Userspace apps. Each has a `Makefile` that includes `apps/app.mk`. `apps/utils/Graphics.hpp` is the C++ `Window` class |
| `include/` | Headers for everything above |
| `rootfs/` | Files copied into the disk image. `rootfs/sysutil/startup.sh` is run at boot. `rootfs/bin/` (installed apps) and `rootfs/sysutil/symbols.map` are generated |
| `tests/` | Host-side tests (`make test`). Not exercised recently; the CI workflow has them commented out |
| `apps/C-Compiler` | Git submodule |

## Build

Build inside the Docker image (Debian 12 with `gcc-multilib`):

```
docker compose up
```

- **What it runs:** `make img` in a container, with the repo mounted at `/RetrOS-32`. It compiles the kernel, libraries and apps, generates `rootfs/sysutil/symbols.map`, builds `RetrOS-32-debug.img`, and copies `rootfs/` into it.
- **Incremental builds are reliable:**
  - Objects track header dependencies (`-MMD -MP`), and the kernel always relinks.
  - The app static libraries (`apps/lib*.a`) are re-archived on every build, so every app relinks too. That matters when you change a struct shared between `lib/` and the kernel.
  - You don't need `make clean`.
- **Toolchain:** the host (macOS) has no i386 cross toolchain. Use the container for real builds, and clang for quick checks (below).
- **GCC catches more than clang:** the build uses GCC with `-Wall -Wextra -Wpedantic`, and GCC warns about things clang doesn't. One example from this repo: a `?:` mixing an enum value with a non-enum operand.

## Run

Run on the host. This needs `qemu-system-i386`.

```
make qemu                      # boots RetrOS-32-debug.img
HOSTFWD_PORT=8081 make qemu    # if port 8080 is taken (the default forward to the guest's port 80)
```

- **The serial log** goes to the terminal (`-serial stdio`). Every `dbgprintf`/`warningf` line appears there as `function(): message`. It's the main debugging tool.
- **Booting is interactive** and needs a display. The maintainer usually runs it and pastes the serial log. Ask before booting it yourself. When asked whether a build worked, check the build output and artifacts; don't boot it.
- **Starting apps:** from the shell, `exec -d /bin/<app>.o` starts an app without waiting for it; without `-d`, the shell waits until it exits. Kernel threads start by name, e.g. `exec kclock`. The Start menu entries are defined in `kernel/kthreads/taskbar.c`.
- **Resolution varies:** the screen has been both 640×480 and 1024×768. Don't hard-code a resolution; use `vbe_info->width`/`height`.

## Checking a change without a full build

These were fast and caught real problems. `.claude/skills/check-change/check.sh` automates the first two: the clang check, compared with HEAD, and the runtime-library check for apps.

**Syntax-check kernel C with clang**, using the kernel's flags:

```
clang --target=i386-elf -m32 -ffreestanding -nostdinc -fno-builtin -fno-omit-frame-pointer \
  -std=gnu11 -O1 -D__KERNEL -I include \
  -Wall -Wextra -Wpedantic -Wno-pointer-arith -Wno-unused-parameter \
  -fsyntax-only graphics/window.c
```

- **Userspace C** (`lib/`, C apps): drop `-D__KERNEL`.
- **C++ apps:** use `clang++` and add `-fno-exceptions -fno-rtti -O2 -I include -I apps -I apps/<app>`.
- **Compare against the code before your change:** the tree has existing warnings. Filter to the file's own warnings, strip line numbers, then compare with the result after `git stash`, and `git stash pop` again.
- **Watch out in zsh:** it doesn't word-split `$FLAGS`, so `clang $FLAGS` passes one big argument. Use `${=FLAGS}` or write the flags inline.

**Check apps for runtime-library calls.** Apps are freestanding and don't link libgcc. Compile the app to an object and run `nm -u` on it. It should only list `gfx_*`, libc and syscall symbols. Watch for helpers like `__udivdi3` or `__moddi3`, which 64-bit division and modulo generate. Floating point is fine; it compiles to inline x87.

**Unit-test the real code on the host.** Most kernel code is hard to compile on the host, but individual functions are easy to test:

- **Kernel C:** use a short Python script to cut the function's source text out of the real file and paste it into a C harness with stub types and a stub `putpixel`. Surround the buffer with guard bytes to catch overflows. Running the same harness against `git show HEAD:<file>` shows the bug before the fix.
- **C++ apps:** put a stub `utils/Graphics.hpp` in a scratch directory that records draw calls into a vector. Include the app's `.cpp` after `#define private public`, with `main` renamed. Then test geometry and event handling.

**Things these checks can't tell you:** how it looks, timing, and anything involving real hardware or QEMU. Say so when handing work back, and give the human a short test script.

## Debugging crashes

- **Read the whole serial log,** not just the panic. The lines before it usually show what was happening, e.g. a resize or a window close.
- **CPU exceptions** (since #5) print `[exception] N <name> (<thread>)`. The output also includes `eip`, the error code, `cs`, `eflags`, and the general registers. The backtrace's first line is the faulting function, then its callers.
- **Page faults use a different path** (#8): `_page_fault_entry` → `page_fault_interrupt`. It prints CR2, the faulting address, but not `eip`.
  - Its backtrace starts at `_page_fault_entry`. The next entry is the return address *in the faulting function's caller*, so the faulting function itself is missing.
  - To find it, disassemble around that return address and see which call it follows.
- **Mapping addresses to code:**
  - `symbols.map` and `bin/symbols` (ELF) match the booted `bin/kernelout` for code addresses. String and data addresses differ by a few KB between the two links.
  - Disassemble with the host's `objdump`, e.g. `objdump -d --no-show-raw-insn --start-address=0x2fcf0 --stop-address=0x2fd30 bin/symbols`. Data symbols are listed by `nm -n -S bin/symbols`.
- **The PCB dump isn't the crash location.** `entry point` is the thread's entry, and `esp`/`ebp` are from its last context switch.
- **Memory corruption usually shows up somewhere else.** An overflow in one subsystem crashes another.
  - The kernel heap is first fit, in boot order. At boot, netd's stack sits right before the window manager's screen buffer, so drawing above the top of the screen used to crash *netd*.
  - When you find a corrupted pointer, read its bytes as data. `20 1c 04 1c` turned out to look like text pixels in the terminal's color `0x1c`, which pointed at a drawing overflow.
- **To test crash output,** temporarily add a fault to a kernel thread that only starts on demand, e.g. the first line of `about()` in `kernel/kthreads/about.c`, which runs when you open About. Remove it afterwards.
  - Invalid opcode: `asm volatile("ud2");`
  - An exception with an error code: `asm volatile("movw $0x1234, %%ax\n\tmovw %%ax, %%ds" ::: "eax");`, which raises #GP with error code `0x1234`.

## Graphics system

**Window geometry** (`include/gfx/window.h`, `graphics/window.c`):
- **Frame:** the title bar and borders are drawn `GFX_WINDOW_HEADER_OVERHANG` (4 px) above `window->y`.
- **Content:** the content starts at `x+8, y+8`, and `inner_width`/`inner_height` are the outer size minus 16. A window's buffer is `inner`, with stride `pitch`, which equals `inner_width`.
- **Size limits:** `gfx_window_clamp_size` enforces a minimum of `GFX_WINDOW_MIN_WIDTH`×`GFX_WINDOW_MIN_HEIGHT` (128×48) and keeps the window on screen. The resize outline and `gfx_window_resize` both use it, so they agree. When there isn't enough memory, `gfx_window_resize` keeps the old size.
- **Resizing:** dragging the corner only moves an outline, drawn from `pending_size`. The window keeps its size and buffer until the release, when `gfx_default_mouse_up` calls `gfx_window_resize` once (#10). Maximize and restore resize right away.
- **Moving:** `is_moving.x`/`y` hold where the title bar was grabbed, relative to the window's position. `gfx_default_hover` keeps that point under the cursor, clamping each axis to the screen on its own, so a window pushed against an edge slides along it. The whole window has to stay on screen because drawing isn't clipped (#4). #18 covers letting windows hang off the edge.

**Drawing:**
- **The `vesa_*` helpers aren't clipped** (`include/vbe.h`, `drivers/vbe.c`; #4). Anything the window server draws off screen lands outside the screen buffer, in the kernel heap.
- **The window drawing functions** in `gfxlib.c` (`kernel_gfx_*`) check against the window's content size. Lines and circles clip every pixel.

**Coordinates:**
- `kernel_gfx_draw_line` treats `x` as the **row** and `y` as the column. Kernel callers pass `(row, column)`.
- The userspace `gfx_draw_line` in `lib/graphics.c` swaps the coordinates first, so apps pass normal `(x, y)`. Don't swap them again in an app.

**Colors** are either a VGA palette index or RGB332, depending on the call:

| Call | Takes |
|---|---|
| `gfx_draw_rectangle`, `gfx_draw_line` | VGA (`COLOR_VGA_*`) |
| `gfx_draw_rectangle_rgb`, `gfx_draw_line_rgb` | RGB332 |
| `gfx_draw_pixel` | RGB332, for icons |
| circles | VGA, passed through unconverted |

The `COLOR_*` constants without `VGA_` in `colors.h` are meant as RGB332, but not all of them are accurate.

**Events:**
- **Reading:** `gfx_get_event(&e, GFX_EVENT_NONBLOCKING)` returns 0 if it got an event and -1 if there was none. On -1 it leaves `e` untouched.
- **Blocking:** `GFX_EVENT_BLOCKING` blocks the thread until an event arrives, so nothing else in its loop runs meanwhile. A thread with timed work, like the taskbar's clock, reads with `GFX_EVENT_NONBLOCKING` and calls `kernel_sleep(ms)`. Events don't wake a sleeping thread, so the sleep is also the longest it takes to respond.
- **The queue holds 10 events.** When it's full, the oldest is dropped.
- **Drain all events every frame.** An app that reads one event per frame falls behind its input, and once the queue is full, events get dropped.
- **On a resize,** apply only the latest size, and redraw everything. A window gets one `GFX_EVENT_RESOLUTION` per resize: when a corner drag is released (#10), or on maximize or restore. The kernel keeps the old content that still fits (#14), and the rest is black.

**Redraws:** the window server only redraws the screen when the mouse moves or a window is marked changed. Every gfx drawing syscall marks the app's window changed. Kernel threads that draw with `w->draw->*` must call `gfx_commit()` themselves, or their updates only appear when the mouse moves.

**Concurrency:** the window server thread replaces a window's buffer on a resize, maximize, restore or F10 fullscreen, while the window's owner may be drawing into it on its own thread. `w->buffer_lock` keeps the two apart (#9):
- **Drawing:** code that writes a window's pixels holds the lock from its bounds check through its last pixel, as the `kernel_gfx_*` functions in `gfxlib.c` do. Code that replaces `inner` holds it for the swap.
- **No nesting:** the lock isn't reentrant, so don't call a function that takes it while holding it.
- **Interrupts:** `spin_lock` waits by yielding to other threads (`WAIT` → `kernel_yield`), so don't take it inside `ENTER_CRITICAL`.
- **Compositing** reads buffers without the lock. That's safe only because every swap also runs on the window server thread. Moving a swap to another thread would mean the compositor has to take the lock too.
- **The event queue** has its own lock, `w->spinlock`.

Syscalls run with interrupts enabled, so they can be preempted.

**Mouse:** the window manager remembers which window got the press (`pressed_window`). A release always goes to that window, but it only counts as a click if the cursor is still over it. Clicks are handled at the cursor's arrow tip (`VESA_CURSOR_HOTSPOT_X`/`_Y`).

## Kernel facts that have mattered

- **The kernel heap is small, and `kalloc` panics when full.**
  - It's 4 MB (`memory_map_init` only counts RAM below 16 MB; #11), first fit, in 256-byte blocks.
  - `kalloc` panics with "Out of memory!" rather than returning NULL.
  - Use `kalloc_try` for large allocations that are allowed to fail.
  - Every window's buffer comes from this heap, apps' included. At 1024×768 a maximized window's buffer is ~715 KB.
- **The time of day:** `get_current_time` (`drivers/rtc.c`) returns exactly what the CMOS real-time clock holds; the OS has no time zones. The Makefile starts QEMU with `-rtc base=localtime`, so the clock shows the host's local time (#19). #20 covers keeping the RTC in UTC with an offset setting.
- **Timers and sleeping:** the timer runs at 1000 Hz, so `sleep(ms)` and ticks are milliseconds. PCB sleep deadlines are `uint32_t`; a 16-bit field used to overflow after ~65 s.
- **Fixed offsets:** `struct pcb` fields above the "DO NOT CHANGE ABOVE" marker are read by assembly at fixed offsets. The ISR stubs in `kernel/irs_entry.s` build exactly the frame described by `struct registers`.
- **Freeing pages:** a process exit currently double-frees a stack page and leaks another (#12), which logs `[VMEM] Double free or invalid free`.

## Writing an app

1. **Create the app:** add `apps/<name>/<name>.cpp` and a `Makefile` (`APP_NAME`, `APP_OUTPUT := <name>.o`, `APP_LIBS := -lcore -lgraphic`, `include ../app.mk`). Then add the name to `APPS` in `apps/Makefile`.
2. **Write the window:** subclass `Window` from `apps/utils/Graphics.hpp`. A frame loop should:
   - drain events;
   - handle `GFX_EVENT_EXIT` and `GFX_EVENT_RESOLUTION`;
   - draw;
   - `sleep(ms)`.
   `apps/breakout`, `apps/bcalc` and `apps/cube` follow this pattern.
3. **Math:** there's no libm. `sin_60`/`cos_60` in `lib/libc.c` are 6° tables, offset by 90°: `cos_60[i]` is sin(6i°), and `sin_60[i]` is −cos(6i°). Avoid 64-bit integer division.
4. **Build and launch:** `docker compose up` installs the app into `rootfs/bin`. Launch it with `exec -d /bin/<name>.o`.

## Workflow conventions

- **Commit only when asked.** Show a draft commit message first.
- **Commit messages:**
  - an imperative subject line;
  - a body that explains the problem and why the fix works, wrapped at ~72 columns;
  - `Fixes #N.` at the end when it closes an issue. Pushing to `development`, the fork's default branch, closes the issue.
- **Issues** follow this outline: Summary, Where, Proposed fix, How to verify.
  - **Where** has line links pinned to a pushed commit SHA.
  - Say what's confirmed and what's only from reading the code.
  - When issues reference each other, create them first, then fill in the numbers with `gh issue edit --body-file`.
- **Report results plainly.** Say what you checked, how, and what you didn't check, e.g. "syntax-checked with clang, host-tested; not built with GCC or booted".
