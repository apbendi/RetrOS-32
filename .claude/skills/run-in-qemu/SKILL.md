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
- **`start` options:** `--res 640|800|1024|text` picks the resolution. `--rtc 2026-10-20T10:05:00` starts the clock at a given time, e.g. to test the taskbar clock at a two-digit hour.
- **`type TEXT`** types text; `\n` is Enter.
- **`key NAME...`** presses keys by QEMU's names: `ret`, `tab`, `esc`, `f10`, `backspace`, `shift-a`, `ctrl-c`.
- **`focus NAME`** brings the window titled NAME to the front.
- **`move X Y`** and **`drag X1 Y1 X2 Y2`** move the mouse; dragging a title bar moves the window.
- **`text`** prints the VGA text screen, for the boot menu and text mode.
- **`mon CMD`** runs any QEMU monitor command.

## How it behaves

- **The boot menu waits for a key,** and a key pressed before the menu is on screen is lost. `start` and `pick` read the menu from video memory, press the key once it's up, and press it again if the kernel doesn't start.
- **Keyboard input goes to the front window.** A new window opens in front, so after `exec -d` the app has the keyboard, not the shell.
  - **`shell`** brings the shell back first.
  - **`focus`** works by pressing Tab, which sends the front window to the back and logs `Pushing window <name> to back`. It presses Tab until the window order repeats, then stops with NAME in front. That takes up to two rounds of Tabs, a couple of seconds.
- **Terminal output is in the serial log,** one line at a time, as `term[<writer>]: <text>`. That includes the shell's echo of each command (`term[shell]: kernel> exec ...`), kernel commands' output, and apps' `printf`. So `$Q wait "term\[shell\]: .*File too large"` checks a command's result without a screenshot.
  - A line is logged when it ends with a newline, so a prompt without one waits for the next newline.
  - Lines over 127 characters are split across log lines.
- **Text mode has no windows,** so use `type` rather than `shell` or `focus` there. `text` prints its screen.
- **`log` and `wait` start from your last action:** `type`, `key`, `shell`, `focus`, `click`, `move` or `drag`.
  - So a `wait` can't match a line from before the command you just ran.
  - Use `--all` for everything since boot.
  - `log --raw` keeps the lines `log` normally hides: window focus changes, mouse events and FAT16's field-by-field dumps.
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

  Windows open near the top left. A window's title bar is its top 12 or so pixels; check a screenshot for exact positions.
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
lldb --batch -o "target create --arch i386 bin/symbols" -o "gdb-remote localhost:1234" \
     -o "breakpoint set -n kernel" -o "process continue" -o "bt" -o "register read" \
     -o "process detach" > .qemu/lldb.out 2>&1 &
$Q pick 1024          # notices when lldb has stopped the VM
cat .qemu/lldb.out
```

- **Breakpoints and backtraces work:** set them by function name or by address (`breakpoint set -a 0x10000` stops at the kernel's first instruction).
  - Frames above `_start` are leftover stack, not callers.
  - `bin/symbols` has no debug info, so you get function names, not source lines.
- **Don't trust lldb for global variables.** `bin/symbols` has the same code addresses as the booted `bin/kernelout`, but its data addresses are off, by 0x1000 in recent builds.
  - To find a global's real address, disassemble a function that uses it and read the address from the instruction.
  - Or read it from `bin/kernelout`'s bytes, the way the `debug-kernel-panic` skill does.
- **`process detach`** lets the OS keep running; `kill` stops QEMU.
