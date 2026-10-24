---
name: run-in-qemu
description: Use to boot RetrOS-32 in QEMU without a display and check a change at runtime, once the human has said you may boot it. Starts the OS, types shell commands, clicks the Start menu and windows, reads the serial log, takes screenshots, and stops in the kernel with lldb.
---

# Running RetrOS-32 in QEMU yourself

`qctl.py` in this directory boots RetrOS-32 in a QEMU with no window and drives it through QEMU's monitor. You type, click and take screenshots, and read the serial log in between. AGENTS.md still says to ask before booting, so only do this when the human has asked you to test at runtime or has said you may.

## Before you start

- **Build first:** `docker compose up` makes `RetrOS-32-debug.img`. Check the build output before booting.
- **Your files:** `qctl.py start` boots a *copy* of the image, `.qemu/disk.img`, so the real image is never locked or changed. The serial log, QEMU's own debug log and screenshots also go in `.qemu/` (git-ignored).
- **One at a time:** the script uses fixed local ports: monitor 45454, gdb 1234, and the guest's port 80 at 18080. If one is taken, set `RETROS_QEMU_MONITOR_PORT`, `RETROS_QEMU_GDB_PORT` or `RETROS_QEMU_HTTP_PORT`. It doesn't conflict with the human's `make qemu`.
- **Stop when you're done:** `qctl.py stop`.

## A session

```
Q=.claude/skills/run-in-qemu/qctl.py
$Q start                              # boot at 1024x768; returns when the shell is up (about a second)
$Q shell "exec -d /bin/graphs.o"      # bring the shell to the front, type the command, press Enter
$Q wait "Created new window for /bin/graphs" --timeout 10
$Q log                                # serial lines since that last action, without window-focus noise
$Q shot                               # saves .qemu/shot-HHMMSS.png and prints the path; Read it to look
$Q click 30 9                         # open the Start menu
$Q click 50 49                        # ...and pick Finder
$Q stop
```

`$Q --help` lists every command.
- **`start` options:** `--res 640|800|1024|text` picks the resolution. `--rtc 2026-10-20T10:05:00` starts the clock at a given time, e.g. to test the taskbar clock at a two-digit hour. `--image IMG` boots a copy of another image instead, e.g. the C harness's `tests/c/out/test.img` (the `test-c` skill). `--mem 16` sets the memory in MB; the default is 32, like `make qemu`. `--disk-bus virtio` boots with no disk the kernel can see: the BIOS boots from a virtio disk, which the kernel's IDE driver doesn't find, as on a machine booted from USB. `start` then waits for a shell that never comes, so run it in the background and drive the login window and the "No disk attached" prompt yourself.
- **`type TEXT`** types text; `\n` is Enter.
- **`key NAME...`** presses keys by QEMU's names: `ret`, `tab`, `esc`, `f10`, `backspace`, `shift-a`, `ctrl-c`.
- **`windows`** lists the windows front to back, with each one's position, size and owner, and where to click to bring it to the front. It presses F12, which makes the window server log the list.
- **`focus NAME`** brings the window titled NAME to the front.
- **`move X Y`** and **`drag X1 Y1 X2 Y2`** move the mouse; dragging a title bar moves the window.
- **`text`** prints the VGA text screen, for the boot menu and text mode.
- **`mon CMD`** runs any QEMU monitor command.

## How it behaves

- **The boot menu waits for a key,** and a key pressed before the menu is on screen is lost. `start` and `pick` read the menu from video memory, press the key once it's up, and press it again if the kernel doesn't start.
- **Keyboard input goes to the front window.** A new window opens in front, so after `exec -d` the app has the keyboard, not the shell.
  - **`shell`** brings the shell back first. The shell isn't always in front after boot, because kernel threads open their windows in no fixed order.
  - **`focus`** lists the windows (F12), then clicks a part of NAME's title bar that no window in front covers, and checks the log for `Pushing window <name> to front`. That takes about half a second.
  - If the title bar is covered everywhere, `focus` falls back to Alt+Tab, which sends the front window to the back. It presses Alt+Tab until the window order repeats, then stops with NAME in front. That takes a couple of seconds.
  - **F12 and the shell's `windows` command** list the windows. F12 is handled by the window server, so it works whatever has focus. Its lines look like `wm_log_windows(): 1 "Clock" x=450 y=50 w=126 h=156 pid=6`.
- **A shell command is done** when the log says `exec_cmd(): Finished <command>`. For `exec` without `-d`, that's when the program has exited.
- **Keys typed while a command runs wait** in the shell window's event queue, which holds 10 and drops the oldest. So wait for `Finished` before typing more than a few keys ahead.
- **A program that reads `stdin`,** started with `exec` without `-d`, takes what you `type`, a line at a time. `key ctrl-d` on an empty line ends its input.
- **Terminal output is in the serial log,** one line at a time, as `term[<writer>]: <text>`. That includes the shell's echo of each command after its prompt (`term[shell]: kernel:/> exec ...`, with the shell's current directory), kernel commands' output, and apps' `printf`. So `$Q wait "term\[shell\]: .*File too large"` checks a command's result without a screenshot.
  - A line is logged when it ends with a newline, so a program's question without one waits for the next newline.
  - The shell's own prompt and what's typed after it aren't logged until Enter. Ctrl+C at the prompt logs the line with `^C` after it.
  - Lines over 127 characters are split across log lines.
- **Text mode has no windows,** so use `type` rather than `shell` or `focus` there. `text` prints its screen.
- **`log` and `wait` start from your last action:** `type`, `key`, `shell`, `focus`, `click`, `move` or `drag`.
  - So a `wait` can't match a line from before the command you just ran.
  - Use `--all` for everything since boot.
  - `log --raw` keeps the lines `log` normally hides: window focus changes, mouse events and the line logged for every file opened.
  - `log --grep RE` filters.
- **Mouse coordinates are screen pixels,** and they're exact.
  - QEMU's PS/2 mouse only moves relatively, so every mouse command first pushes the cursor into the top-left corner, then moves from there.
  - The click lands on the cursor's arrow tip (`VESA_CURSOR_HOTSPOT_X`/`_Y`), and the script allows for it.
  - The taskbar logs `taskbar(): Mouse event: X Y` for its clicks, which is a quick way to check where one landed.
- **Places to click.** These are fixed and the same at every resolution:

  | What | X, Y |
  |---|---|
  | Start | 30, 9 |
  | Start menu items | x 50; y 33 Terminal, 49 Finder, 65 SysInfo, 81 Editor, 97 Cube, 113 Colors, 129 Clock, 145 Calc, 161 Users, 177 Wolfstein |

  For windows, use `windows`: it prints each window's rectangle and a point to click.
  - `x`, `y`, `w` and `h` are the window's outer rectangle. Its title bar starts 4 px above `y` and is 12 px tall.
  - The maximize, minimize and close buttons take the title bar from `x+w-40` rightwards.
  - The taskbar is a transparent window across the top of the screen. Only its bar, the top 20 or so pixels, takes clicks.
- **Each command takes a few hundred milliseconds.** That's fine for checking behaviour, but it can't judge smoothness, flicker or how something feels. Leave that to the human.

## Repeating an action until something fails

To fill memory or hammer a code path, loop in the shell and stop on the first failure, e.g.:

```
for i in $(seq 1 40); do
  $Q shell "exec -d /bin/graphs.o"
  $Q wait "Created new window for /bin/graphs|Couldn't start|Not enough memory|PANIC" --timeout 5 || break
  $Q log --grep "Couldn't start|Not enough memory|PANIC" | grep . && break
done
```

## Crashes

- **`start` reports a crash during boot.** It stops waiting at `KERNEL PANIC`, `[exception]` or a page fault, and points you to `log --all`. Read the log with the `debug-kernel-panic` skill.
- **A triple fault stops the VM** instead of rebooting it (`-no-reboot -no-shutdown`), so `$Q mon "info registers"` still shows the CPU state.
- **`.qemu/qemu-debug.log`** has QEMU's register dump at every CPU reset, including the first one at power-on, and any guest errors QEMU noticed.
- **Monitor commands worth knowing:**
  - `info registers`;
  - `xp /16xw 0xADDR` reads physical memory, and the kernel's own memory is identity mapped;
  - `x /16xw 0xADDR` reads through the current page tables;
  - `info mem` and `info tlb`.

## Debugging with lldb

`start --gdb` starts QEMU paused before the BIOS, waiting for a debugger on port 1234. The boot menu still needs a key, and lldb waits at `process continue`, so run lldb in the background with its commands up front, then pick the resolution:

```
$Q start --gdb
lldb --batch -o "target create --arch i386 bin/symbols" -o "settings set target.source-map /RetrOS-32 $PWD" \
     -o "gdb-remote localhost:1234" -o "breakpoint set -f cmds.c -l 130" -o "process continue" \
     -o "frame variable" -o "p *fs" -o "process detach" > .qemu/lldb.out 2>&1 &
$Q pick 1024          # boots; it also notices if lldb stops the VM during boot
$Q shell "ls"         # runs the line with the breakpoint
cat .qemu/lldb.out
```

- **`bin/symbols` has debug info:** the kernel's C is compiled with `-g`, so lldb knows source lines, types and variables. Apps have none.
  - **`source-map`:** the build runs in the container, where the repo is `/RetrOS-32`. This setting points lldb at your checkout, so it can show the source.
  - **Breakpoints:** by function (`-n kernel`), address (`-a 0x10000` is the kernel's first instruction) or source line (`-f cmds.c -l 130`).
  - **At a stop:** `frame variable` lists the arguments and locals, and `p *fs` or `p fs->ops->list` follow pointers. Locals the optimizer dropped show as `<unavailable>`.
  - To stop at a moment you choose after boot, break on `wm_log_windows` and press F12 (`$Q key f12`).
- **Globals:** a global's address in `bin/symbols` is its address in the running kernel. `target variable netd` prints the whole struct, and `p netd.if_count` is 2 once `eth0` and `lo0` are registered.
  - `$kernel`, `$process` and `$services` have a `$` in their names, which lldb keeps for its own variables. `target variable -r '^.process$'` prints the pointer. Cast the address it prints to follow it, e.g. `p ((struct process*)0x4c61c)->current->name`.
  - Macros aren't available.
- **`bt` stops at frame #0 in kernel threads:**
  - **Why:** each thread's stack starts one byte below its top (`kernel/pcb.c`), so every frame is misaligned, and lldb's i386 unwinder rejects misaligned frames.
  - **Where it works:** on the boot stack, e.g. at `kernel()`, where frames above `_start` are leftover stack, not callers.
  - **Walk the frames yourself instead:**
    1. `memory read -f x -s 4 -c 2 $ebp` prints the caller's `ebp` and the return address.
    2. `image lookup -a <return address - 1>` names the caller and the line of the call.
    3. Repeat with the caller's `ebp`.
- **`process detach`** lets the OS keep running; `kill` stops QEMU.
