# Notes for agents working on RetrOS-32

RetrOS-32 is a 32-bit x86 hobby OS: kernel, window server, drivers, networking, a FAT file system on FatFs, C/C++ userspace apps, and TinyCC, a C compiler that runs on RetrOS, with a standard C library built from musl. This file covers how to build, run, check and debug it, and the parts of the codebase that have been easy to get wrong.

This repo is a fork, `apbendi/RetrOS-32`, of `joexbayer/RetrOS-32`. Work happens on the `development` branch, and issues are tracked on the fork (`gh issue list --repo apbendi/RetrOS-32`).

`.claude/skills/` has step-by-step procedures for recurring tasks:
- `debug-kernel-panic`: working out a crash from the serial log.
- `check-change`: checking a change without booting. Its `check.sh` runs the clang check described below.
- `file-issue`: this fork's issue format and the `gh` steps.
- `run-in-qemu`: booting RetrOS-32 in QEMU without a display and driving it, once the human says you may: shell commands, clicks, screenshots, the serial log and lldb. Its `qctl.py` does the work.
- `test-filesystem`: checking the file system code against mtools and `fsck.fat` on disk images, without booting.
- `test-c`: running C programs on RetrOS in QEMU and comparing their output with the same programs on Linux, for the C library, TinyCC and the kernel under them. It runs TinyCC's own test suite too.

## Layout

| Path | What's there |
|---|---|
| `kernel/` | Kernel core: scheduler, PCBs, memory (`kmem.c`, `vmem.c`, `memory.c`), interrupts (`arch/`, `irs_entry.s`), syscalls, symbols (`ksyms.c`), the terminal (`terminal.c`) |
| `kernel/kthreads/` | Kernel threads with windows: `shell`, `taskbar` (Start menu), `kclock`, `sysinf`, `about`, … |
| `graphics/` | Window server (`windowserver.c`), window manager (`windowmanager.c`), windows (`window.c`), drawing (`gfxlib.c`), the gfx syscall (`api.c`) |
| `drivers/` | VBE, keyboard, mouse, ATA, e1000, … |
| `fs/` | The file system: `fs.c` (paths, file descriptors, current directories), `fatfs_iface.c` (FAT on FatFs), `fatfs_port.c` (FatFs's hooks), and `fatfs/`, FatFs itself. See "File system" below |
| `lib/` | Userspace libraries linked into apps (`graphics.c`, `libc.c` with the sin/cos tables, …) |
| `apps/` | Userspace apps. Each has a `Makefile` that includes `apps/app.mk`. `apps/utils/Graphics.hpp` is the C++ `Window` class |
| `include/` | Headers for everything above |
| `rootfs/` | Files copied into the disk image. `rootfs/sysutil/startup.sh` is run at boot. `rootfs/bin/` (installed apps), `rootfs/usr/` (the C library), `rootfs/lib/tcc/` (TinyCC's headers and runtime) and `rootfs/sysutil/symbols.map` are generated |
| `tests/` | `tests/fs/`: the file system harness (the `test-filesystem` skill). `tests/c/`: the C harness (the `test-c` skill). The rest is host-side tests (`make test`), not exercised recently; the CI workflow has them commented out |
| `apps/tcc` | TinyCC (TCC), the C compiler, installed as `/bin/tcc.o`. `tinycc/` is upstream's whole tree, with a few patches; `README.md` lists them. See "C programs" below |
| `libc/` | The standard C library for C programs: `musl/`, a whole musl release, built in part, with RetrOS's own startup, system calls and `malloc` in `src/`. See "C programs" below |

## Architecture

- **Kernel threads do most of the work.** The window server (`wind`), networking (`netd`, `dhcpd`, `tcpd`), the work queue (`workd`) and the windowed tools in `kernel/kthreads/` are all kernel threads. They run in ring 0 in the kernel's address space, allocate from the kernel heap and call kernel functions directly. The tools draw with `w->draw->*`.
- **Apps are processes.** `pcb_create_process` loads a flat binary at `0x1000000` into its own page directory and runs it in ring 3. Apps reach the kernel only through `int $48` (`lib/syscall.c`), drawing included.
- **Any thread can be preempted.** The timer interrupt runs at 1000 Hz and calls `kernel_yield()` on every tick. There's one CPU, so `ENTER_CRITICAL()`, which turns interrupts off, is what keeps other threads out.
- **Boot:** `boot/bootloader.s` → `kernel/kcrt0.s` → `kernel()` in `kernel/kernel.c`.
  - **Interrupts stay off** from the bootloader until `kernel()` has loaded its interrupt table (`init_interrupts()`). An interrupt taken before then triple-faults (#41).
  - `kernel()` initializes each subsystem, starts `idled`, `workd`, `wind` and, if `default.cfg` enables it, `netd`, then starts the timer.
  - `wind` starts `login`, which runs `rootfs/sysutil/startup.sh`.
  - **The kernel image** is a flat binary loaded at `0x10000`, and `.bss` is the last part of it (`kernel/linker.ld`). The bootloader reads exactly the image's sectors, a count `tools/build.c` stores in the boot sector (#32), and `kcrt0` clears `.bss` before calling `kernel()`.
- **Files register themselves.** The macros below add a constructor to the `.kctor_table` section, and `init_kctors()` runs them all at boot. There's no central list; grep for a macro to find everything it registers.
  - `EXPORT_KTHREAD(fn)`: a kernel thread, launched by `start("fn", …)` or the shell's `exec fn`.
  - `EXPORT_SYSCALL(index, fn)`: a syscall. The core syscalls are added by hand in `kernel()`.
  - `EXPORT_KSYMBOL(fn)`: a shell command.
- **Shell commands:** `exec_cmd` (`kernel/kutils.c`) splits a line into `argc`/`argv`, looks up the first word with `ksyms_resolve_symbol`, and calls the result on the caller's thread. `sh` scripts and the `system` syscall go through it too.
  - **Splitting:** spaces, tabs and newlines separate words, except between double quotes, which are dropped: `cd "my projects"` passes one argument, and `""` an empty one. There's no limit on the number of words or their length (#51). `argv` ends with NULL.
  - **Errors:** `exec_cmd` prints its own, "Unknown command: <name>" or "Missing closing quote.". A blank line does nothing.
  - **Finished:** it logs `exec_cmd(): Finished <command>` when the command returns. For `exec` without `-d`, that's when the program has exited, which is how the `test-c` harness waits for one.
  - The lookup tries `EXPORT_KSYMBOL` names first, then every function in `symbols.map`. So any kernel function's name runs as a command, with `(argc, argv)` as its arguments (#27), e.g. `exec_cmd` itself.
  - **Passing arguments on:** a kernel thread gets copies of its arguments. A process gets `argv` and its strings copied into its memory, which have to fit in one 4 KB page, or it fails with "Arguments too long.".
- **Where data comes in:**
  - **Input:** the keyboard and mouse interrupts fill buffers, and `wind` polls them every frame.
  - **Network:** the e1000 interrupt queues each packet for `netd`, which hands it to `workd` to run up the stack in `net/`. Apps use the socket syscalls in `net/api.c`.
  - **Files:** `fs/fs.c` is the file layer, and FAT through FatFs is the only file system behind it. `kernel()` mounts the disk at boot. See "File system" below.
    - **With no disk,** as on a machine booted from USB, where the IDE driver doesn't see the boot disk, `login` offers to create a 1 MB virtual disk in the kernel heap (`drivers/virtualdisk.c`), formats it and mounts it. Nothing else formats a disk.
- **Globals** (`include/kernel.h`, `include/pcb.h`):
  - `$kernel` holds the graphics context and window server.
  - `$services` holds the scheduler, networking, user manager and event log.
  - `$process->current` is the running PCB.

## Build

Build inside the Docker image (Debian 12 with `gcc-multilib`):

```
docker compose up
```

- **What it runs:** `make img` in a container, with the repo mounted at `/RetrOS-32`. It compiles the kernel, libraries and apps, generates `rootfs/sysutil/symbols.map`, builds `RetrOS-32-debug.img` with `tools/build.c`, and copies `rootfs/` into it through a Linux `vfat` mount, long names included.
  - **The C library and TinyCC:** `make img` builds `libc/` first, then `apps/`, which builds `apps/tcc`: TinyCC's cross compiler, which runs in the container, and `/bin/tcc.o`, both linked against the C library.
- **Incremental builds are reliable:**
  - Objects track header dependencies (`-MMD -MP`), and the kernel always relinks.
  - `fs/` and `net/` have their own Makefiles. The top-level Makefile runs them on every build, through the phony targets `fs_objects` and `net_objects`, so their header tracking works too. A rule for a subdirectory's objects without a prerequisite like that only ever builds them once. That's how `list` crashed after #33 changed `struct terminal`: `fs/` still had the old layout compiled in.
  - The app static libraries (`apps/lib*.a`) are re-archived on every build, so every app relinks too. That matters when you change a struct shared between `lib/` and the kernel.
  - You don't need `make clean`, except after changing compile flags, which make doesn't track: run `docker compose run --rm builder clean`, then `docker compose up`.
- **Toolchain:** the host (macOS) has no i386 cross toolchain. Use the container for real builds, and clang for quick checks (below).
- **A first build is slow:** the host is arm64 and the container `linux/amd64`, so gcc runs emulated, one file at a time. The C library alone is ~550 files and takes a few minutes. Once they're built, later builds skip them.
- **`bin/build` must be the container's:** it's a host tool (`HOST_CC`), and the repo is shared with the container, so a `make tools`, `make img` or `make create_fs` run on the Mac leaves a macOS binary there. The container's make sees it's newer than `tools/build.c`, tries to run it, and fails with `create_fs] Error 127`. `rm bin/build`, then `docker compose up` again.
- **GCC catches more than clang:** the build uses GCC with `-Wall -Wextra -Wpedantic`, and GCC warns about things clang doesn't. One example from this repo: a `?:` mixing an enum value with a non-enum operand.

## Run

Run on the host. This needs `qemu-system-i386`.

```
make qemu                      # boots RetrOS-32-debug.img
HOSTFWD_PORT=8081 make qemu    # if port 8080 is taken (the default forward to the guest's port 80)
```

- **The serial log** goes to the terminal (`-serial stdio`). Every `dbgprintf`/`warningf` line appears there as `function(): message`. It's the main debugging tool.
  - Terminal output appears there too, one line at a time, as `term[<writer>]: <text>`. That covers the shell's echo of each command, the commands' output and apps' `printf`.
- **`make qemu` is interactive** and opens a window. The maintainer usually runs it and pastes the serial log. Ask before booting it yourself. When asked whether a build worked, check the build output and artifacts; don't boot it.
- **The window on a Mac:** since QEMU 11.0, the Cocoa display draws one guest pixel per screen pixel, so on a Retina display the window is half the width and height it used to be. Zoom-to-fit enlarges it, but stretches the strips QEMU redraws when only part of the screen changes, such as the rows around the cursor.
  - **The fix:** `brew install apbendi/tap/qemu-hidpi` installs a patched QEMU with both fixed: `-display cocoa,scale=N`, and redraws that match a full redraw. It's keg-only, so Homebrew's own `qemu` stays the default everywhere else. The source is the `v11.1.2-hidpi` tag of github.com/apbendi/qemu. QEMU doesn't accept AI-written code, so the patches can't go upstream.
  - **What `make qemu` does:** on macOS, it runs that QEMU when it's installed, with `scale=1`, the window size from before 11.0. On stock QEMU 11 or later, it prints a note suggesting the install. Elsewhere it runs `qemu-system-i386` as before. Stock QEMU rejects `scale` ("Parameter 'scale' is unexpected"), so the Makefile passes it only to a QEMU whose `-h` lists it.
  - **Overrides:** `QEMU=<path>` picks the binary. `QEMU_DISPLAY="-display cocoa,zoom-to-fit=on"` gives a window you can resize, and `QEMU_DISPLAY=` gives QEMU's default.
- **Without a display:** when the human says you may boot it, `.claude/skills/run-in-qemu/qctl.py` boots a copy of the image in a headless QEMU and drives it. It picks the resolution, types into the shell, clicks, saves screenshots you can look at, and reads the serial log. See the `run-in-qemu` skill.
- **Starting apps:** from the shell, `exec -d /bin/<app>.o` starts an app without waiting for it; without `-d`, the shell waits until it exits. Kernel threads start by name, e.g. `exec kclock`. The Start menu entries are defined in `kernel/kthreads/taskbar.c`.
  - A program starts in the shell's current directory, so relative paths work: `cd "/my projects"`, then `exec -d /bin/edit.o main.c`, `exec /bin/tcc.o main.c -o main` and `exec main`, or `exec /bin/tcc.o -run main.c`.
- **Resolution varies:** the screen has been both 640×480 and 1024×768. Don't hard-code a resolution; use `vbe_info->width`/`height`.

## Checking a change without a full build

These were fast and caught real problems. `.claude/skills/check-change/check.sh` automates the first two: the clang check, compared with HEAD, and the runtime-library check for apps.

For the C library, TinyCC, and the kernel code programs depend on (loading, memory, files, the terminal), the `test-c` skill's harness runs C programs on RetrOS in QEMU and compares them with Linux. It found most of the bugs in that code so far.

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

**Things these checks can't tell you:** how it looks, timing, and anything involving real hardware or QEMU. Say so when handing work back, and give the human a short test script. With the human's OK, the `run-in-qemu` skill covers behaviour in QEMU, but not how it feels in use.

## Debugging crashes

- **Read the whole serial log,** not just the panic. The lines before it usually show what was happening, e.g. a resize or a window close.
- **CPU exceptions** (since #5) print `[exception] N <name> (<thread>)`. The output also includes `eip`, the error code, `cs`, `eflags`, and the general registers. The backtrace's first line is the faulting function, then its callers.
- **Page faults** (exception 14) print the same, then `Page Fault Address:` with CR2, the address the instruction touched, and whether the page was present, whether it was a write, and whether it came from user mode. The page table entries for the address follow. Before #8 they took a separate path that printed no `eip`.
- **Mapping addresses to code:**
  - `bin/kernelout`, the flat image that boots, is copied out of the ELF `bin/symbols` with `objcopy -O binary`. So every address in `bin/symbols` and `symbols.map` is the booted kernel's, data included.
  - **Source lines and types:** the kernel's C is compiled with `-g` (`KERNEL_DEBUG_CFLAGS` in `make/common.mk`), so `bin/symbols` has debug info.
    - `objcopy` leaves the debug info out of `kernelout`, so the kernel that boots is the same either way.
    - lldb reads it without booting: `lldb --batch -o "target create --arch i386 bin/symbols" -o "image lookup -a 0x1b036"` prints `list + 255 at cmds.c:130:3`.
    - **For a return address,** which is every backtrace entry except an exception's first, look up the address minus 1, which is inside the call.
    - `image lookup -t 'struct terminal'` lists a struct's fields with their types.
  - Disassemble with the host's `objdump`, e.g. `objdump -d -l --no-show-raw-insn --start-address=0x2fcf0 --stop-address=0x2fd30 bin/symbols`, where `-l` marks the source lines. Data symbols are listed by `nm -n -S bin/symbols`.
- **The PCB dump isn't the crash location.** `entry point` is the thread's entry, and `esp`/`ebp` are from its last context switch.
- **Memory corruption usually shows up somewhere else.** An overflow in one subsystem crashes another.
  - The kernel heap is first fit, in boot order. At boot, netd's stack sits right before the window manager's screen buffer, so drawing above the top of the screen used to crash *netd*.
  - When you find a corrupted pointer, read its bytes as data. `20 1c 04 1c` turned out to look like text pixels in the terminal's color `0x1c`, which pointed at a drawing overflow.
  - A global that's already wrong before its thread first runs can come from the disk image, not the heap. `netd` crashed at boot on an interface pointer of `0xffff00a3`: two FAT entries, which the bootloader had loaded over the end of `.bss` (fixed in 2f52aa1, and the bootloader now reads only the kernel's sectors, #32).
- **Temporary logging in `kernel/vmem.c`:** it turns `dbgprintf` off at the top, so use `warningf` there.
- **To test crash output,** temporarily add a fault to a kernel thread that only starts on demand, e.g. the first line of `about()` in `kernel/kthreads/about.c`, which runs when you open About. Remove it afterwards.
  - Invalid opcode: `asm volatile("ud2");`
  - An exception with an error code: `asm volatile("movw $0x1234, %%ax\n\tmovw %%ax, %%ds" ::: "eax");`, which raises #GP with error code `0x1234`.
  - A page fault: `*(volatile int*)0x1c041c20 = 0;`, a write to an address that isn't mapped.

## Graphics system

**Window geometry** (`include/gfx/window.h`, `graphics/window.c`):
- **Frame:** the title bar and borders are drawn `GFX_WINDOW_HEADER_OVERHANG` (4 px) above `window->y`.
- **Content:** the content starts at `x+8, y+8`, and `inner_width`/`inner_height` are the outer size minus 16. A window's buffer is `inner`, with stride `pitch`, which equals `inner_width`.
- **Size limits:** `gfx_window_clamp_size` enforces a minimum of `GFX_WINDOW_MIN_WIDTH`×`GFX_WINDOW_MIN_HEIGHT` (128×48) and keeps the window on screen. The resize outline and `gfx_window_resize` both use it, so they agree. When there isn't enough memory, `gfx_window_resize` keeps the old size.
- **Creating:** `gfx_new_window` returns NULL when there isn't enough memory for the buffer. Kernel threads check for it and return. For apps, `gfx_create_window` in `lib/syscall.c` prints a message and exits, since an app without a window can't be seen or closed.
- **Titles:** a window's title and header hold 19 characters (`GFX_MAX_WINDOW_NAME_SIZE`, less the terminating 0). `gfx_set_title` refuses a longer one and leaves the old title up, so shorten it first, as the Editor does with a file's name.
- **Resizing:** dragging the corner only moves an outline, drawn from `pending_size`. The window keeps its size and buffer until the release, when `gfx_default_mouse_up` calls `gfx_window_resize` once (#10). Maximize and restore resize right away.
- **Fullscreen (F10):** the window's `inner` becomes the window server's composition buffer, at screen size, and the window server stops compositing.
  - **What's recorded:** the window server remembers which window it is (`ws->_fullscreen`) and the fields it changed (`ws->_saved`). F10 restores exactly those on that window, even if another window has come to the front since.
  - **No resizing:** while `is_fullscreen` is set, `gfx_window_resize` refuses, since it would free the composition buffer.
  - **Closing it:** `ws_remove` leaves fullscreen before the window's buffer is freed.
  - **The taskbar** (`GFX_IS_IMMUATABLE`) doesn't go fullscreen.
- **Moving:** `is_moving.x`/`y` hold where the title bar was grabbed, relative to the window's position. `gfx_default_hover` keeps that point under the cursor, clamping each axis to the screen on its own, so a window pushed against an edge slides along it. The whole window stays on screen. Drawing is clipped (#4), so that's for usability, not safety. #18 covers letting windows hang off the edge.

**Drawing:**
- **The `vesa_*` helpers are clipped to the screen** (`include/vbe.h`, `drivers/vbe.c`; #4). They draw only into screen-sized buffers: the composition buffer, the framebuffer and the window server's background. Whatever falls off screen is skipped.
  - **Lines and filled rectangles** clamp their range before looping. **Characters, icons and single pixels** check each pixel with `vesa_on_screen`.
  - **`gfx_draw_window`** copies only the on-screen part of a window's content.
  - **`putpixel` isn't clipped,** because it also draws into windows' own buffers. Use it only with a size check of your own, as the `kernel_gfx_*` functions do.
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
- **Key codes** (`include/keyboard.h`): a `GFX_EVENT_KEYBOARD`'s `data` is the key's character, or for Ctrl+key, 128 plus the key's character without Shift, e.g. `CTRLC`, 227.
  - **Keys without a character** have codes no Ctrl+key can be: the arrows (251–254), F1–F10 (193–202), `KEY_HOME`, `KEY_END`, `KEY_PAGE_UP`, `KEY_PAGE_DOWN` and `KEY_DELETE` (203–207), and F12 (134).
  - **Apps** use the `KEY_*` names in `include/gfx/events.h`. Line editors ignore codes above 126.
  - **Modifiers:** the event's `flags` has the modifier keys held when the key was pressed, `KEY_MOD_SHIFT`, `KEY_MOD_CTRL` and `KEY_MOD_ALT`. So Shift+Left is `KEY_LEFT` with `KEY_MOD_SHIFT`, and Ctrl+Shift+C is `CTRLC` with both. The driver queues each key with them (`struct kb_key`), and `kb_get_modifiers()` gives the ones held now.
  - **Fake Shifts:** a real keyboard sends Shift presses and releases prefixed with `0xE0` around the gray arrows, Home, End and the like. The driver ignores them. QEMU doesn't send them.

**Redraws:** the window server only redraws the screen when the mouse moves or a window is marked changed. Every gfx drawing syscall marks the app's window changed. Kernel threads that draw with `w->draw->*` must call `gfx_commit()` themselves, or their updates only appear when the mouse moves.

**Concurrency:** the window server thread replaces a window's buffer on a resize, maximize, restore or F10 fullscreen, while the window's owner may be drawing into it on its own thread. `w->buffer_lock` keeps the two apart (#9):
- **Drawing:** code that writes a window's pixels holds the lock from its bounds check through its last pixel, as the `kernel_gfx_*` functions in `gfxlib.c` do. Code that replaces `inner` holds it for the swap.
- **No nesting:** the lock isn't reentrant, so don't call a function that takes it while holding it.
- **Interrupts:** `spin_lock` waits by yielding to other threads (`WAIT` → `kernel_yield`), so don't take it inside `ENTER_CRITICAL`.
- **Compositing** reads buffers without the lock. That's safe only because every swap also runs on the window server thread. Moving a swap to another thread would mean the compositor has to take the lock too.
  - So it can copy a window halfway through a redraw, which then shows on screen until the next composite. `gfx_draw_window` clears `changed` before copying, not after, so a commit made during the copy still gets the finished frame composited.
- **The event queue** has its own lock, `w->spinlock`.

**Window lifetime:** when a thread or process with a window exits, the cleanup worker calls `gfx_destroy_window`. The window server may be in the middle of a frame that uses that window, so the window isn't freed right away (#16).
- **Removing it:** `ws_remove` sets `w->removed` and takes the window out of the lists, the active one or another workspace's.
- **Freeing it:** the window server frees the window and its buffer at the start of its next frame (`__ws_free_windows`), when it holds no window pointers.
- **Rules for window manager and window server code:**
  - Don't keep a window pointer from one frame to the next. Anything that does, like `pressed_window` or `ws->_fullscreen`, has to be cleared in `ws_remove`.
  - Don't put a window with `removed` set back into a list, or store it anywhere. `add`, `push_front` and `push_back` check it under `wm->spinlock`.
  - **Every change to the window lists holds `wm->spinlock`:** `add`, `remove`, `push_front`, `push_back` and the workspace switch.
  - The walks in `wm_default_draw`, `wm_default_mouse_event` and `wm_default_changes` don't take the lock. That's safe for two reasons:
    - a removed window keeps its `next` pointer until it's freed;
    - a window's `next` is set before it's published at the front, with `COMPILER_BARRIER()` between the two stores.

Syscalls run with interrupts enabled, so they can be preempted.

**Mouse:** the window manager remembers which window got the press (`pressed_window`). A release always goes to that window, but it only counts as a click if the cursor is still over it. Clicks are handled at the cursor's arrow tip (`VESA_CURSOR_HOTSPOT_X`/`_Y`).
- **Clicks:** `GFX_EVENT_MOUSE` is a click, at `data`, `data2` in the window's content. It's sent on the release.
- **Dragging:** a press on a window's content, not one that grabs its title bar or resize corner, starts a drag (`wm->dragging`). The window gets:
  - `GFX_EVENT_MOUSE_DOWN` where the press was;
  - `GFX_EVENT_MOUSE_MOVE` each frame the mouse has moved while the button is held, wherever the mouse is;
  - `GFX_EVENT_MOUSE_UP` where the button comes up, then the click if that's over the window.
- **Coordinates** are relative to the content, so a move or release outside the window is negative or past its size: read `data` and `data2` as `int16_t`.
- **Modifiers:** `flags` has the modifier keys held, as on a key, so Shift+click is a press with `KEY_MOD_SHIFT`.
- **The queue:** a move replaces a move at the end of the window's queue (`gfx_push_event`), so a drag can't push other events out.
- **Destroyed windows:** `ws_remove` clears `pressed_window` and the drag, so nothing more is sent to a window that's going.

**Listing windows:** F12 logs the current workspace's windows to the serial log, front to back, with each one's rectangle, owner and flags (`wm_log_windows`). The shell's `windows` command prints the same list. Both copy the list under `wm->spinlock` with `wm_get_windows`, then print after releasing it.

## Terminal

**The pieces** (`kernel/terminal.c`, `include/terminal.h`, `kernel/kthreads/shell.c`):
- **A terminal,** `struct terminal`, keeps the text written to it in `textbuffer` and draws it into its window, `screen`.
  - **Writing:** kernel code writes with `twritef`, and programs with `SYSCALL_TERM_WRITE` (`terminal_write`).
  - **The log:** each line written is also copied to the serial log, as `term[<writer>]: <text>`.
- **The input line** is the line being edited after the text: the shell's prompt and command, or what a program reading a line has typed.
  - **Showing it:** `terminal_set_input` shows it, continuing the text's last line, and `terminal_hide_input` takes it away.
  - **Not part of the text:** it's never logged, and text written meanwhile goes before it.
- **The GUI shell** is a kernel thread per Terminal window, `shell()`.
  - **Its state:** a kalloc'd `struct shell`. Its PCB's `local` points to it, so commands running on its thread, like `reset` and `exit`, find it. `local_free` frees it however the thread exits.
  - **A command line:** the prompt, `kernel:<directory>> `, and what's typed are the input line. Enter hides it, writes the same text into the output, runs the command with `exec_cmd`, and shows a new prompt after the output.
  - **An unfinished last line,** a program's or `cat`'s, gets a newline before the prompt (`terminal_end_line`).
  - **Keys:** backspace, Up and Down through the last 32 commands (`SHELL_HISTORY_MAX`), and Ctrl+C, which leaves the line in the output with `^C` after it. A line holds `SHELL_LINE_MAX`, 1,088 bytes.
- **Text mode** (`textshell`) uses a terminal with the `terminal_ops_textmode` ops. It edits its line on its frame's bottom row, and shows no input line. It also ends an unfinished last line before a command's echo.

**Layout:** a graphics terminal lays out its text and input line at the window's size when it draws. It wraps lines at the window's width, and shows as much of the end as fits.
- **What's kept:** as many lines as a screen-sized window shows (`__terminal_max_lines`), and at most twice the characters it holds (`__terminal_max_chars`).
- So a window made bigger shows earlier lines again. There's no scrollback beyond that.

**Drawing only what changed** (#118): the terminal remembers what's on screen, in its `drawn_*` fields.
- **A commit** clears the input line, draws the text added since, scrolling the window's pixels up when it reaches the bottom, and draws the input line after it.
- **Full redraws:**
  - the first draw, a resize and a color change;
  - text taken away: `clear`, `reset`, or an overlong line cut short;
  - an input line that ends higher than it did, once the text has scrolled.
  - **After each command,** the shell asks for one with `terminal_invalidate`, since a command may draw into the window itself.
- **Who draws:** whichever thread commits.
  - **Other threads:** a thread writing to a terminal whose window isn't its own, like a program, draws at each newline it writes, or after each `terminal_write`.
  - **The window's own thread,** like the shell, draws when it commits.

**Threads:**
- **Sharing:** threads, programs and kernel threads started from a shell share its terminal.
  - **References:** each holds one, taken when its PCB is created (`terminal_get`) and released in `pcb_cleanup_routine` (`terminal_put`). The last frees the terminal.
  - **Outliving the shell:** a program started with `exec -d` can. Cleanup first takes the closing window away from the terminal (`terminal_release_window`), so the program's text goes only to the log.
- **The lock:** `terminal_lock` is held while the text or the input line changes, or the terminal draws.
  - **Taking it again:** the thread holding it can, since the ops call each other.
  - **Order:** take it before a window's `buffer_lock`, never after, and never inside `ENTER_CRITICAL`.

**Reading** (#113): `SYSCALL_TERM_READ` (`terminal_read`) reads a line typed at the caller's terminal. The C library's `stdin` uses it.
- **Who may read:** only the program its shell is waiting for. `exec` without `-d` makes it the terminal's `reader` before it can run. Any other caller gets end of input at once, since the shell is taking the keys.
- **Editing:** the line is edited on the input line, after what the program printed, with the keys typed in the shell's window. Enter adds it to the text, so it's shown and logged as the program's.
- **End of input:** Ctrl+D on an empty line; Ctrl+C, which throws the line away, since RetrOS has no signals; or the window closing.
- **The kernel's own `scan`,** which `tcp` and the text shell's login use, reads the same way.

## Selection and the clipboard

**The clipboard** (`kernel/clipboard.c`, `include/clipboard.h`) is one text, of up to `CLIPBOARD_MAX`, 64 KB, without a terminating 0. Every program and kernel thread shares it.
- **Setting it:** `clipboard_set(data, size)` copies the text in, replacing the old one, and returns `size`. Too much, or no memory for the copy, returns a negative error code and leaves the old text.
- **Reading it:** `clipboard_get(data, size)` copies out as much as fits, and returns the whole text's size. So a size of 0 asks how big it is.
- **Who calls them:** kernel code directly; apps through `SYSCALL_CLIPBOARD_SET` and `SYSCALL_CLIPBOARD_GET`, with wrappers of the same names in `lib/syscall.c`.
- **The shell's `clip`** prints the clipboard as it is, or with arguments sets it to them, joined by single spaces as `echo` joins them, without a newline. It's a quick way to check what an app copied, from the serial log.

**In the Editor** (`apps/editor/edit.cpp`), the selection runs from `m_selAnchor`, -1 for none, to the cursor.
- **Keys:** Shift with an arrow, Home, End, Page Up or Page Down selects; Ctrl+A selects all; Esc clears. Ctrl+C, Ctrl+X and Ctrl+V copy, cut and paste through the clipboard. Typing, Enter, Tab, Backspace and Delete replace or delete the selection.
- **Pasting** inserts the text exactly as it is, without the `}` typing a `{` adds. What doesn't fit in the 64 KB buffer is left out, and the status line says so.
- **Editing ranges:** `ropeDeleteRange` and `ropeInsertText` change the rope a node at a time. Each node is a `malloc`, so a page of the app's heap, and rebuilding the rope for each edit would need twice the pages.
- **Drawing:** `drawSelection` draws the selection's background before the text, laying rows out as `reDraw` does.

## File system

**Layers** (`fs/`):
- **`fs.c`** is what the kernel's commands and the file syscalls call: `fs_open`, `fs_read`, `fs_write`, `fs_seek`, `fs_stat`, `fs_readdir`, `fs_mkdir`, `fs_unlink`, `fs_rmdir`, `fs_rename`, `fs_touch`, `fs_chdir` and `fs_getcwd`.
  - **Paths:** it normalizes every path before the file system sees it (`fs_path_normalize`): absolute, without empty, `.` or `..` parts, and without a trailing slash. A relative path starts from the current process's directory. A path is at most `FS_PATH_MAX`, 512 bytes.
  - **Errors:** everything returns a negative code from `include/errors.h`, never a bare -1: e.g. `ERROR_FILE_NOT_FOUND`, `ERROR_PATH_NOT_FOUND` (a directory in the path), `ERROR_IS_DIRECTORY`, `ERROR_NO_SPACE`. `error_get_string` gives the message.
- **`fatfs_iface.c`** implements `struct filesystem_ops` on FatFs, and **`fatfs_port.c`** gives FatFs its disk, clock, memory and lock hooks.
- **`fatfs/`** is FatFs R0.16, vendored with its two official patches. `ffconf.h` turns on long names (with a heap buffer), code page 437, `f_mkfs`, labels, `f_utime` and reentrancy (one mutex per volume). `FF_FS_RPATH` is 0, since `fs.c` resolves relative paths itself.
  - **`FF_FS_LOCK` is 0,** so nothing stops one process deleting, renaming or writing a file another has open, which can corrupt the disk (#109).
- **Mounting:** any FAT12, FAT16 or FAT32 volume, at sector 0 or in the partition table's first entry.

**Files and directories:**
- **Descriptors** are per process: 32 each, from 3, kept in `fs.c` by PID because `struct pcb` is packed. A process's threads share its table. Exiting closes what's left open (`fs_close_files_of`). Files opened before any process runs use `fs_boot_files`.
- **Reads and writes** go to the file's position, which they advance and `fs_seek` moves (from the start, the current position or the end). `fs_read` returns 0 at the end.
  - `FS_FILE_FLAG_TRUNCATE` empties a file as it's opened, and `FS_FILE_FLAG_APPEND` writes at its end.
  - A write can't start past the end of the file. Each write is synced to the disk as it's made. A short write means the disk is full.
- **Directories** are opened read-only and listed with `fs_readdir`, which gives each entry's long name, size, attributes and last write time (`struct fs_dirent`), and leaves out `.` and `..`. Reading a directory like a file fails with `ERROR_IS_DIRECTORY`, and opening one for writing is refused (#86).
- **Names** are case-insensitive, and keep the case they were made with. A long name has an 8.3 alias that FatFs makes up; `fs_stat` and `fs_readdir` report the long name as stored. `fs_same_entry` tells whether two paths are one file or directory, however each spells it, by where it is on the disk.
- **Moving:** `fs_rename` also moves between directories. It refuses an existing destination, and a directory moved inside itself, which FatFs would do, losing the directory. Renaming to the same name in another case works, which is how `mv docs Docs` changes only the case.
- **The current directory** is per process, kept in `fs.c` by PID. A new process or kernel thread starts in its creator's, and a thread shares its process's. Each shell window is a kernel thread with its own.

**In the shell:** `ls`, `cd`, `pwd`, `mkdir`, `rm`, `rmdir`, `mv`, `cp`, `find`, `touch`, `cat` and `xxd`, in `kernel/cmds.c`. `touch` dates a file or directory now, and creates an empty file where there's nothing. `cat` prints files exactly as they are, one after another, and `xxd` prints a file's bytes in hex. `mv` and `cp` put the source inside a destination that's a directory, as on Unix; `cp` copies files, not directories. Quote names with spaces. The prompt shows the current directory, `kernel:/my projects> `. It follows the output, and each command stays on screen after it, as in the log.

**In apps:** `open`, `read`, `write`, `seek`, `fclose`, `stat`, `readdir`, `mkdir`, `unlink`, `rmdir`, `rename`, `chdir` and `getcwd` in `lib/syscall.c`. Finder and the tree view (`apps/utils/TreeView.hpp`) list with `readdir`.

**The disk image** (`tools/build.c`): `mkfs.fat` makes a 32 MB FAT16 volume whose reserved sectors hold the bootloader and the kernel.
- **Sectors 0–3** are the bootloader, around the boot sector's FAT fields, with a partition entry covering the disk.
- **The kernel** starts at sector 4. The reserved sector count (boot sector offset 14) is the kernel's sectors plus 4, which is how the bootloader knows how much to read.
- **`make img`'s `sync`** then copies `rootfs/` onto the volume.

**The virtual disk:** `login` formats it with `fs_format`, which runs `f_mkfs`. At 1 MB that's FAT12.

**Testing:** the `test-filesystem` skill. `tests/fs/run.sh` runs the real `fs.o` and `fatfs.o` on disk images through `tests/fs/fstool`, and compares the result with mtools and `fsck.fat`, in the Docker builder, without booting. Then check syscalls, the shell and apps in QEMU.

## C programs

A program in standard C is built on RetrOS's C library (`libc/`), with TinyCC on RetrOS or in the builder, or with the builder's gcc. RetrOS's own apps (`apps/`, "Writing an app") use `libcore` and `libgraphic` instead, which aren't standard C.

**The C library** (`libc/`, its `README.md`):
- **What it is:** musl, a whole release vendored in `libc/musl/` with its three security patches, of which `libc/Makefile` builds a part: strings, stdio, the stdlib, math, time, the environment, `setjmp`. RetrOS's own code in `libc/src/` does the startup, the POSIX calls on RetrOS's system calls, `errno` and `malloc`.
- **How it fits RetrOS:**
  - `libc/arch/` replaces two of musl's i386 headers by coming first in the include path, so musl's files stay unmodified:
    - **The current thread:** musl finds it through `%gs`, which RetrOS doesn't set up, so it's one static structure, shared by a program's threads.
    - **Linux system calls:** the ones musl's own code makes, like stdio's `writev`, go to `libc/src/linux.c`, which does them with the library's functions; the rest return `ENOSYS`.
  - **The kernel's numbers:** system call numbers, error codes and file flags come from the kernel's own headers, which the library includes next to standard ones: `syscall_helper.h`, `errors.h` with `ERRORS_CODES_ONLY`, and `fs/fs_types.h`.
- **Where it's installed:** headers in `/usr/include`, and `libc.a` and `crt1.o` in `/usr/lib`, with empty `libm.a` and friends as musl has them. In the repo, `rootfs/usr/`.
- **How programs behave:**
  - descriptors 0 to 2 are the terminal. `stdout` is line-buffered. `stdin` reads lines typed at the terminal, if the program is the one its shell is waiting for; otherwise it's at its end;
  - RetrOS's errors become `errno` values;
  - `rename` replaces an existing file;
  - the clock has whole seconds of local time;
  - at most 4 MB of heap and 64 KB of stack.
- **Not yet:** thread safety, `dup`, `exec`, signals, exit codes and windows (#117).

**Building a program:**
- **TinyCC on RetrOS:** `exec /bin/tcc.o main.c -o main`, then `exec main`, or `exec /bin/tcc.o -run main.c`.
- **TinyCC in the builder:** `apps/tcc/bin/i386-retros-tcc main.c -o main`, with the same defaults, finding the library under `rootfs/`.
- **gcc in the builder:** include `libc/program.mk` and link with its `STDC_*` variables (`libc/retros.ld`).
- **Either way, a program** is a flat binary at `0x1000000`. RetrOS starts it at its first byte, so `crt1.o`'s `_start` comes first, and `.bss` is written into the file as zeros, because RetrOS maps only what's in the file.

**TinyCC** (`apps/tcc`, its `README.md`):
- **Patches:** upstream's tree in `tinycc/`, with a few, each marked `RetrOS` in the code:
  - a RetrOS target, which predefines `__retros__`, not `__linux__`;
  - code sorted first;
  - `.bss` in flat binaries;
  - `-run` using TCC's own C library through a generated symbol table, since the program would have no set-up library of its own.
- **Settings:** `apps/tcc/config.h`, in place of what TCC's configure script writes.
- **Its own files** are in `/lib/tcc`: headers, `libtcc1.a` (64-bit arithmetic and the like) and `runmain.o` (for `-run`).

**Testing:** the `test-c` skill's harness (`tests/c/run.py`) runs C programs and TinyCC's own tests on RetrOS, compiled with gcc and the cross compiler, and with `--native`, with `/bin/tcc.o` on RetrOS.

## Kernel facts that have mattered

- **How memory is divided:** `memory_map_init` (`kernel/memory.c`) divides the memory between 1 MB and 16 MB. With `-m 32m` that's 15 MB:
  - **1–11 MB, the kernel heap (`kalloc`):** whatever the other regions leave, 10 MB (#11).
  - **11–12 MB, permanent memory (`palloc`, never freed):** a fixed 1 MB. It holds the wallpaper buffer, the network card's buffers and the heap's bitmap, about 834 KB at 1024×768.
  - **12–16 MB, pages:** a quarter of the memory. The first 256 KB is the page-table pool for the kernel's own tables (`vmem_manager`, 6 pages used); the rest is pages for processes (`vmem_default`: page tables, code, stacks, `malloc`).
  - **The memory above 16 MB isn't used.** The kernel's 1:1 mapping ends at 16 MB, where process code is mapped (`VMEM_DATA`). #56 covers moving it and using all of memory.
  - The sizes are in `include/memory.h`. The shell's `meminfo` shows each region's use.
- **Pages come from `vmem_alloc` as their last user left them.** Clear any page used as a page table or directory, as `vmem_init_process` does: one that isn't cleared maps whatever its page held.
- **A process's `malloc` system call** (`vmem_stack_alloc`) allocates from regions of pages in its heap, one page table at `VMEM_HEAP`, so 4 MB at most (`VMEM_HEAP_SIZE`).
  - **New regions** go in the first gap between regions that fits, or after the last.
  - **Freeing a region** unmaps its pages, and `vmem_unmap` flushes each from the TLB.
  - **The C library** asks for blocks of 128 KB or more and divides them itself (`libc/src/malloc.c`); `libcore` apps make a system call per allocation.
- **Programs** are up to `PROGRAM_MAX_SIZE`, 2 MB, mapped at `VMEM_DATA` through one page table.
- **`kalloc` panics when the heap is full.**
  - The heap is first fit, in 256-byte blocks.
  - `kalloc` panics with "Out of memory!" rather than returning NULL.
  - Use `kalloc_try` for large allocations that are allowed to fail.
  - Starting a program or thread uses `kalloc_try` for the load buffer, the kernel stack and the window buffer. Running out returns `-ERROR_ALLOC`, which `exec` and the Start menu report. Smaller allocations along the way still use `kalloc`. Keeping a reserve, so a launch can't take the last free memory, is #31.
  - Every window's buffer comes from this heap, apps' included. At 1024×768 a maximized window's buffer is ~715 KB.
- **The time of day:** `get_current_time` (`drivers/rtc.c`) returns exactly what the CMOS real-time clock holds; the OS has no time zones. The Makefile starts QEMU with `-rtc base=localtime`, so the clock shows the host's local time (#19). #20 covers keeping the RTC in UTC with an offset setting.
  - It reads the RTC with interrupts off. Selecting a register (port `0x70`) and reading it (`0x71`) are separate accesses, and a thread switch between them lets another thread change which register gets read.
  - `struct time`'s `year` has two digits. `get_current_time` sets `centuary` to 20, so the full year is `centuary * 100 + year`.
- **Showing a time or date:** use `time_format_time` and `time_format_date` (`include/timefmt.h`), not your own format string. The taskbar, the Clock window and `ls` all use them.
  - **Settings:** they follow the `[time]` section of `rootfs/sysutil/default.cfg`:
    - `clock=12h` or `24h`;
    - `date=mdy`, `dmy` or `ymd`.
    The defaults are 12h and mdy.
  - **Changing them:** the settings are read on every call, so `conf load sysutil/default.cfg` applies them right away. The taskbar redraws its clock once a minute.
  - **Screen space:** size buffers and screen space with the `TIME_FORMAT_*_MAX` lengths, which cover every setting. The kernel's printf has no `%02d`, so the formatters pad the numbers themselves.
- **File dates:** FAT keeps the time of each file's last write. FatFs sets it from `get_fattime` (`fs/fatfs_port.c`), which reads the RTC, as does `fs_touch`, which the shell's `touch` uses. `fs_stat` and `fs_readdir` report it as a `struct time` in `struct fs_dirent`'s `written`, with a `centuary` of 0 for none.
  - Files `make img` copies in get the build container's clock, which is UTC. So they look a few hours off from files created in RetrOS, which use the host's local time.
- **Timers and sleeping:** the timer runs at 1000 Hz, so `sleep(ms)` and ticks are milliseconds. PCB sleep deadlines are `uint32_t`; a 16-bit field used to overflow after ~65 s.
- **The scheduler's queues:** the scheduler runs from the timer interrupt, with interrupts off, and uses the PCB queues. So each queue operation runs with interrupts off (`QUEUE_CRITICAL` in `kernel/pcb_queue.c`), not under a spinlock. Don't guard anything the scheduler uses with `spin_lock`: if the timer preempts a thread holding the lock, the scheduler waits for it on that thread's stack, yielding into itself until the kernel stack overflows.
- **Printing from programs:** `SYSCALL_TERM_WRITE` (`terminal_write`) writes a buffer to the program's terminal in one call, and the terminal draws only what it adds ("Terminal"). A program's unfinished last line goes to the log when it exits.
- **Fixed offsets:** `struct pcb` fields above the "DO NOT CHANGE ABOVE" marker are read by assembly at fixed offsets. The ISR stubs in `kernel/irs_entry.s` build exactly the frame described by `struct registers`.
- **User stacks:** a process's stack, and each of its threads', is `VMEM_STACK_PAGES` pages below `VMEM_STACK`, 64 KB, all mapped when it starts. Exiting frees them. Growing on demand instead is #115.
  - **A double free:** `[VMEM] Double free or invalid free for page N` in the log means something freed a page twice. Every exit logged it until #12 was fixed, so now it points to a new bug.
  - **Checking for a leak:** in lldb, compare `p vmem_default->used_pages`, the pages in use, before and after, e.g. stopping on `meminfo` both times. Opening and closing an app should leave it unchanged.

## Writing an app

1. **Create the app:** add `apps/<name>/<name>.cpp` and a `Makefile` (`APP_NAME`, `APP_OUTPUT := <name>.o`, `APP_LIBS := -lcore -lgraphic`, `include ../app.mk`). Then add the name to `APPS` in `apps/Makefile`.
   - **Any name works:** RetrOS reads long names, so `mandelbrot.o` is opened by that name. (Before the FatFs move, names had to fit 8.3.)
2. **Write the window:** subclass `Window` from `apps/utils/Graphics.hpp`. A frame loop should:
   - drain events;
   - handle `GFX_EVENT_EXIT` and `GFX_EVENT_RESOLUTION`;
   - draw;
   - `sleep(ms)`.
   `apps/breakout`, `apps/bcalc` and `apps/cube` follow this pattern.
3. **Math:** `libcore` has no libm. `sin_60`/`cos_60` in `lib/libc.c` are 6° tables, offset by 90°: `cos_60[i]` is sin(6i°), and `sin_60[i]` is −cos(6i°). Avoid 64-bit integer division.
4. **Files:** the file syscalls are in `lib/syscall.c` and `include/lib/syscall.h`; see "File system" for how they behave. They return negative codes from `include/errors.h`, and `error_get_string` (in `libcore`) gives a code's message.
5. **Build and launch:** `docker compose up` installs the app into `rootfs/bin`. Launch it with `exec -d /bin/<name>.o`.

These apps use RetrOS's own libraries, `libcore` and `libgraphic`, which aren't standard C. A program in standard C uses the C library instead: see "C programs".

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
