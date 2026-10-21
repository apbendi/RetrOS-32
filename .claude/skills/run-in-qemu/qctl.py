#!/usr/bin/env python3
"""Boot RetrOS-32 in a headless QEMU and drive it from the command line.

QEMU runs in the background with no window. Its serial log, a copy of the disk
image and screenshots go in .qemu/ at the repo root. The script talks to QEMU's
monitor over TCP to press keys, move the mouse and take screenshots.

  start [--res 640|800|1024|text] [--gdb] [--rtc TIME]
                          boot a copy of RetrOS-32-debug.img, pick the resolution
                          (default 1024) and wait for the desktop
  stop                    quit QEMU
  status                  is it running, and where are its files
  pick RES                at the boot menu, press the key for RES (after start --gdb)

  log [--all] [--raw] [--grep RE] [--tail N]
                          serial log since the last action (type, key, click, ...)
  wait RE [--timeout S] [--all]
                          wait for a serial line matching RE since the last action
  shot [FILE]             save a screenshot (PNG) and print its path
  text                    print the VGA text screen (boot menu, text mode)

  type TEXT               type TEXT; \\n is Enter
  key NAME...             press keys by QEMU name: ret tab esc f10 ctrl-c shift-a ...
  windows                 list the windows front to back, with where to click each (presses F12)
  focus NAME              bring the window titled NAME to the front
  shell CMD               focus the shell, then type CMD and Enter
  click X Y [--right]     click at screen pixel X,Y
  move X Y                move the mouse to screen pixel X,Y
  drag X1 Y1 X2 Y2        press at X1,Y1, move to X2,Y2, release

  mon CMD                 run a QEMU monitor command (info registers, xp, ...)

Environment: RETROS_QEMU_MONITOR_PORT (default 45454), RETROS_QEMU_GDB_PORT
(1234), RETROS_QEMU_HTTP_PORT (18080, forwarded to the guest's port 80).
"""
import argparse, os, re, shutil, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
STATE = os.path.join(REPO, ".qemu")
SERIAL = os.path.join(STATE, "serial.log")
PIDFILE = os.path.join(STATE, "qemu.pid")
MARK = os.path.join(STATE, "mark")
DISK = os.path.join(STATE, "disk.img")
IMAGE = os.path.join(REPO, "RetrOS-32-debug.img")

MONITOR_PORT = int(os.environ.get("RETROS_QEMU_MONITOR_PORT", "45454"))
GDB_PORT = int(os.environ.get("RETROS_QEMU_GDB_PORT", "1234"))
HTTP_PORT = int(os.environ.get("RETROS_QEMU_HTTP_PORT", "18080"))

RESOLUTION_KEYS = {"640": "1", "800": "2", "1024": "3", "text": "esc"}

# Characters that need a QEMU key name other than themselves
KEYS = {" ": "spc", "\n": "ret", "\t": "tab", "-": "minus", "=": "equal", "[": "bracket_left",
        "]": "bracket_right", "\\": "backslash", ";": "semicolon", "'": "apostrophe",
        "`": "grave_accent", ",": "comma", ".": "dot", "/": "slash",
        "!": "shift-1", "@": "shift-2", "#": "shift-3", "$": "shift-4", "%": "shift-5",
        "^": "shift-6", "&": "shift-7", "*": "shift-8", "(": "shift-9", ")": "shift-0",
        "_": "shift-minus", "+": "shift-equal", "{": "shift-bracket_left",
        "}": "shift-bracket_right", "|": "shift-backslash", ":": "shift-semicolon",
        '"': "shift-apostrophe", "~": "shift-grave_accent", "<": "shift-comma",
        ">": "shift-dot", "?": "shift-slash"}

# Serial lines left out of `log` unless --raw: focus changes, mouse events and FAT16's per-field dumps
NOISE = re.compile(r"Pushing window .* to (back|front)|Removing (front )?window|Switching focus"
                   r"|\(\): Mouse event|^fat16_\w+\(\):\s{2,}|^fat16_read\(\): Entry:|^fs_open\(\): fs_open:")

# The mouse cursor's click point is this far from its top-left corner (VESA_CURSOR_HOTSPOT_X/Y)
HOTSPOT = (1, 2)


def die(message):
    print(message, file=sys.stderr)
    sys.exit(1)


def sleep(seconds):
    time.sleep(seconds)


# --- QEMU process ---------------------------------------------------------

def qemu_pid():
    try:
        pid = int(open(PIDFILE).read())
        os.kill(pid, 0)
        return pid
    except (OSError, ValueError):
        return None


class Monitor:
    """One connection to QEMU's human monitor. The monitor echoes each command
    back as it's typed, so the first line of every reply is dropped."""

    def __init__(self):
        try:
            self.sock = socket.create_connection(("127.0.0.1", MONITOR_PORT), timeout=10)
        except OSError:
            die(f"No QEMU monitor on port {MONITOR_PORT}. Is it running? (qctl.py start)")
        self._read_prompt()

    def _read_prompt(self):
        data = b""
        while not data.rstrip().endswith(b"(qemu)"):
            chunk = self.sock.recv(65536)
            if not chunk:
                break
            data += chunk
        return data

    def run(self, command):
        self.sock.sendall(command.encode() + b"\n")
        text = self._read_prompt().decode(errors="replace")
        text = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", text).replace("\r", "")
        return "\n".join(l for l in text.split("\n")[1:] if l.strip() and l.strip() != "(qemu)")

    def close(self):
        self.sock.close()


def monitor(command):
    m = Monitor()
    try:
        return m.run(command)
    finally:
        m.close()


# --- Serial log -----------------------------------------------------------

def clean(line):
    line = re.sub(r"\x1b\[[0-9;]*m", "", line)
    return "".join(c if 32 <= ord(c) < 127 or c == "\t" else "?" for c in line)


def serial_size():
    return os.path.getsize(SERIAL) if os.path.exists(SERIAL) else 0


def set_mark():
    """Remember where the serial log ends, so log and wait only look at what comes after the next action."""
    os.makedirs(STATE, exist_ok=True)
    open(MARK, "w").write(str(serial_size()))


def get_mark():
    try:
        return int(open(MARK).read())
    except (OSError, ValueError):
        return 0


def serial_lines(since):
    if not os.path.exists(SERIAL):
        return []
    with open(SERIAL, "rb") as f:
        f.seek(since)
        data = f.read().decode("latin-1")
    return [clean(l) for l in data.split("\n") if l.strip()]


def wait_for(pattern, timeout, since):
    rx = re.compile(pattern)
    end = time.time() + timeout
    while True:
        for line in serial_lines(since):
            m = rx.search(line)
            if m:
                return line, m
        if time.time() >= end:
            return None, None
        sleep(0.25)


# --- Boot -----------------------------------------------------------------

def vga_text():
    """The 80x25 VGA text screen, read from guest memory at 0xb8000."""
    out = monitor("xp /4000xb 0xb8000")
    # Each line of xp output starts with an address and a colon; keep only the byte values after it
    data = []
    for line in out.split("\n"):
        if ":" in line:
            data += [int(b, 16) for b in re.findall(r"0x([0-9a-f]{2})", line.split(":", 1)[1])]
    chars = "".join(chr(c) if 32 <= c < 127 else " " for c in data[0::2])
    return [chars[i:i + 80].rstrip() for i in range(0, len(chars), 80)]


def pick(res, timeout=30):
    """Wait for the boot menu, then press the key for res. A key pressed before the menu is up is lost."""
    key = RESOLUTION_KEYS.get(str(res))
    if key is None:
        die(f"Unknown resolution {res}; use one of {', '.join(RESOLUTION_KEYS)}")
    end = time.time() + timeout
    while not any("Choose resolution" in l for l in vga_text()):
        if time.time() > end:
            die("The boot menu didn't appear. Try: qctl.py text, qctl.py shot")
        sleep(0.3)
    for attempt in range(3):
        since = serial_size()
        Monitor().run(f"sendkey {key}")
        # The kernel prints as soon as it starts. A debugger stop counts as started too.
        line, _ = wait_for(r"\S", 4, since)
        if line or "debug" in monitor("info status"):
            print(f"Picked {res} at the boot menu")
            return
    die("Pressed the resolution key 3 times, but the kernel didn't start. Try: qctl.py shot")


def start(args):
    if qemu_pid():
        die("QEMU is already running; stop it first (qctl.py stop)")
    if not os.path.exists(IMAGE):
        die(f"{IMAGE} doesn't exist. Build it first: docker compose up")
    os.makedirs(STATE, exist_ok=True)
    # Boot a copy, so the real image is never locked or written
    shutil.copyfile(IMAGE, DISK)
    for path in (SERIAL, MARK):
        if os.path.exists(path):
            os.remove(path)

    cmd = ["qemu-system-i386",
           "-device", "e1000,netdev=net0",
           "-netdev", f"user,id=net0,hostfwd=tcp::{HTTP_PORT}-:80",
           "-m", f"{args.mem}m",
           "-rtc", f"base={args.rtc}" if args.rtc else "base=localtime",
           "-drive", f"file={DISK},format=raw,index=0,media=disk" if args.disk_bus == "ide" else f"file={DISK},format=raw,if=virtio",
           "-display", "none",
           "-serial", f"file:{SERIAL}",
           "-monitor", f"tcp:127.0.0.1:{MONITOR_PORT},server,nowait",
           # Stop instead of rebooting on a triple fault, so registers can still be read
           "-no-reboot", "-no-shutdown",
           "-d", "cpu_reset,guest_errors", "-D", os.path.join(STATE, "qemu-debug.log")]
    if args.gdb:
        cmd += ["-gdb", f"tcp::{GDB_PORT}", "-S"]
    out = open(os.path.join(STATE, "qemu.out"), "w")
    proc = subprocess.Popen(cmd, cwd=REPO, stdin=subprocess.DEVNULL, stdout=out, stderr=subprocess.STDOUT,
                            start_new_session=True)
    open(PIDFILE, "w").write(str(proc.pid))

    end = time.time() + 10
    while True:
        try:
            socket.create_connection(("127.0.0.1", MONITOR_PORT), timeout=1).close()
            break
        except OSError:
            if proc.poll() is not None or time.time() > end:
                die("QEMU didn't start:\n" + open(os.path.join(STATE, "qemu.out")).read())
            sleep(0.2)
    print(f"QEMU running, pid {proc.pid}. Serial log: {SERIAL}")

    if args.gdb:
        print(f"Paused before the BIOS, waiting for a debugger on port {GDB_PORT}:\n"
              f"  lldb bin/symbols -o 'settings set target.source-map /RetrOS-32 {REPO}' -o 'gdb-remote localhost:{GDB_PORT}'\n"
              f"Set breakpoints, continue, then pick a resolution: qctl.py pick 1024")
        return
    pick(args.res)
    if args.res == "text":
        return
    line, _ = wait_for(r"shell: entering event loop|KERNEL PANIC|\[exception\]|print_page_fault_info", 60, 0)
    if line is None:
        die("The desktop didn't come up within 60 s. Try: qctl.py log --all, qctl.py shot")
    if "entering event loop" not in line:
        die("The kernel crashed while booting:\n  " + line + "\nSee: qctl.py log --all")
    set_mark()
    # Kernel threads start their windows in no fixed order, so the shell isn't always in front; shell and focus handle that
    print("Desktop is up")


def stop(_):
    pid = qemu_pid()
    if not pid:
        print("QEMU isn't running")
        return
    try:
        monitor("quit")
    except SystemExit:
        pass
    for _ in range(20):
        if not qemu_pid():
            break
        sleep(0.2)
    if qemu_pid():
        os.kill(pid, signal.SIGTERM)
    print("QEMU stopped")


def status(_):
    pid = qemu_pid()
    print(f"QEMU: {'running, pid ' + str(pid) if pid else 'not running'}")
    if pid:
        print("VM: " + monitor("info status"))
    print(f"Files: {STATE} (serial.log, qemu-debug.log, disk.img, screenshots)")
    print(f"Ports: monitor {MONITOR_PORT}, gdb {GDB_PORT}, guest port 80 at localhost:{HTTP_PORT}")


# --- Input ----------------------------------------------------------------

def press(m, name):
    m.run(f"sendkey {name}")
    sleep(0.08)


def type_text(text):
    set_mark()
    m = Monitor()
    for ch in text:
        if ch in KEYS:
            press(m, KEYS[ch])
        elif ch.isupper():
            press(m, "shift-" + ch.lower())
        elif ch.isalnum():
            press(m, ch)
        else:
            die(f"Don't know how to type {ch!r}")
    m.close()


def keys(names):
    set_mark()
    m = Monitor()
    for name in names:
        press(m, name)
    m.close()


def mouse_home(m):
    # The cursor stops at the screen's edges, so a big move up and left puts it at 0,0
    for _ in range(4):
        m.run("mouse_move -400 -400")
        sleep(0.02)


def mouse_by(m, dx, dy):
    # Small steps, so QEMU's PS/2 queue doesn't overflow
    steps = max(1, (max(abs(dx), abs(dy)) + 99) // 100)
    for i in range(steps):
        sx = dx * (i + 1) // steps - dx * i // steps
        sy = dy * (i + 1) // steps - dy * i // steps
        m.run(f"mouse_move {sx} {sy}")
        sleep(0.02)


def mouse_to(m, x, y):
    mouse_home(m)
    mouse_by(m, x - HOTSPOT[0], y - HOTSPOT[1])
    sleep(0.1)


def click(x, y, button):
    set_mark()
    m = Monitor()
    mouse_to(m, x, y)
    m.run(f"mouse_button {button}")
    sleep(0.1)
    m.run("mouse_button 0")
    m.close()


def move(x, y):
    set_mark()
    m = Monitor()
    mouse_to(m, x, y)
    m.close()


def drag(x1, y1, x2, y2):
    set_mark()
    m = Monitor()
    mouse_to(m, x1, y1)
    m.run("mouse_button 1")
    sleep(0.1)
    mouse_by(m, x2 - x1, y2 - y1)
    sleep(0.1)
    m.run("mouse_button 0")
    m.close()


WINDOW_LINE = re.compile(r'wm_log_windows\(\): (\d+) "(.*)" x=(\d+) y=(\d+) w=(\d+) h=(\d+) pid=(-?\d+)(.*)')

# The taskbar is a transparent window across the top of the screen. Clicks go through its transparent
# pixels, so only its bar (TASKBAR_HEIGHT in kernel/kthreads/taskbar.c) takes them. This ignores an open Start menu.
TASKBAR_BAR = 22


def list_windows():
    """F12 makes the window server log the current workspace's windows, front to back (wm_log_windows)."""
    since = serial_size()
    m = Monitor()
    press(m, "f12")
    m.close()
    _, match = wait_for(r"wm_log_windows\(\): (\d+) windows in workspace", 3, since)
    if not match:
        die("F12 didn't log a window list. In text mode there are no windows.")
    count = min(int(match.group(1)), 32)
    end = time.time() + 3
    while True:
        found = [WINDOW_LINE.search(l) for l in serial_lines(since)]
        windows = [{"index": int(w.group(1)), "name": w.group(2), "x": int(w.group(3)), "y": int(w.group(4)),
                    "w": int(w.group(5)), "h": int(w.group(6)), "pid": int(w.group(7)), "flags": w.group(8).split()}
                   for w in found if w]
        if len(windows) >= count or time.time() > end:
            return windows
        sleep(0.1)


def takes_click(w, x, y):
    """Whether window w would take a click at screen pixel x,y (see wm_default_mouse_event)."""
    if "transparent" in w["flags"]:
        top, bottom = w["y"], w["y"] + TASKBAR_BAR
    elif "no-title-bar" in w["flags"]:
        top, bottom = w["y"], w["y"] + w["h"]
    elif "minimized" in w["flags"]:
        # Only the title bar is drawn: it starts GFX_WINDOW_HEADER_OVERHANG (4) above y and is 12 tall
        top, bottom = w["y"] - 4, w["y"] + 8
    else:
        top, bottom = w["y"] - 4, w["y"] + w["h"]
    return w["x"] <= x <= w["x"] + w["w"] and top <= y <= bottom


def title_bar_point(windows, target):
    """A point on target's title bar that no window in front of it covers, or None.
    Clicking a title bar brings the window to the front without sending the app a click.
    The maximize, minimize and close buttons take its right end from x+w-40 on (gfx_default_click)."""
    if "no-title-bar" in target["flags"]:
        return None
    y = target["y"] + 2
    for x in range(target["x"] + 8, target["x"] + target["w"] - 42, 4):
        if not any(takes_click(w, x, y) for w in windows[:target["index"]]):
            return x, y
    return None


def windows_cmd():
    """Print the window list, front to back, with where to click to bring each window to the front."""
    set_mark()
    windows = list_windows()
    for w in windows:
        if w["index"] == 0:
            where = "in front"
        elif "no-title-bar" in w["flags"]:
            where = "no title bar"
        else:
            point = title_bar_point(windows, w)
            where = f"click {point[0]} {point[1]} to focus" if point else "title bar covered"
        flags = " " + " ".join(w["flags"]) if w["flags"] else ""
        print(f'{w["index"]:2} {w["name"]!r:22} x={w["x"]} y={w["y"]} w={w["w"]} h={w["h"]} pid={w["pid"]}{flags}  ({where})')


def focus(name, quiet=False):
    """Bring the frontmost window titled NAME to the front by clicking a visible part of its title bar.
    Falls back to Alt+Tab cycling when its title bar is covered."""
    windows = list_windows()
    matches = [w for w in windows if w["name"] == name]
    if not matches:
        die(f"No window titled {name!r}. Windows, front to back: {', '.join(w['name'] for w in windows)}")
    target = matches[0]
    if target["index"] > 0:
        point = title_bar_point(windows, target)
        if point is None:
            focus_by_tab(name, quiet)
            return
        since = serial_size()
        m = Monitor()
        mouse_to(m, *point)
        m.run("mouse_button 1")
        sleep(0.1)
        m.run("mouse_button 0")
        m.close()
        if not wait_for(r"Pushing window " + re.escape(name) + " to front", 2, since)[0]:
            focus_by_tab(name, quiet)
            return
    set_mark()
    if not quiet:
        print(f"{name} is in front")


def focus_by_tab(name, quiet=False):
    """Alt+Tab sends the front window to the back and logs "Pushing window <name> to back".
    Press Alt+Tab until the sequence of names repeats, so the window order is known, then
    stop when NAME is next in line, which means it's in front."""
    seen = []
    m = Monitor()
    for _ in range(80):
        since = serial_size()
        press(m, "alt-tab")
        line, match = wait_for(r"Pushing window (.+) to back", 2, since)
        if not match:
            m.close()
            die("Alt+Tab didn't switch windows (no 'Pushing window' line). Is a window open?")
        seen.append(match.group(1))
        period = cycle_length(seen)
        if period:
            order = seen[-period:]
            if name not in order:
                m.close()
                die(f"No window titled {name!r}. Windows, front to back after the next Alt+Tab: {', '.join(order)}")
            if seen[len(seen) - period] == name:
                m.close()
                set_mark()
                if not quiet:
                    print(f"{name} is in front")
                return
    m.close()
    die("Gave up after 80 Alt+Tabs")


def cycle_length(seen):
    """The window order is a cycle. Return its length once it has been seen to repeat."""
    for n in range(1, len(seen) // 2 + 1):
        if all(seen[i] == seen[i - n] for i in range(n, len(seen))):
            # Unique names repeat after one cycle; duplicate names need two full cycles to be sure
            if len(set(seen[:n])) == n or len(seen) >= 2 * n:
                return n
    return None


def shell(command):
    focus("shell", quiet=True)
    type_text(command + "\n")


# --- Output ---------------------------------------------------------------

def shot(path):
    if path is None:
        path = os.path.join(STATE, time.strftime("shot-%H%M%S.png"))
    path = os.path.abspath(path)
    out = monitor(f"screendump {path} -f png")
    if out:
        die(out)
    print(path)


def log(args):
    since = 0 if args.all else get_mark()
    lines = serial_lines(since)
    if not args.raw:
        lines = [l for l in lines if not NOISE.search(l)]
    if args.grep:
        rx = re.compile(args.grep)
        lines = [l for l in lines if rx.search(l)]
    if args.tail:
        lines = lines[-args.tail:]
    print("\n".join(lines))


def wait(args):
    line, _ = wait_for(args.pattern, args.timeout, 0 if args.all else get_mark())
    if line is None:
        die(f"Timed out after {args.timeout} s waiting for {args.pattern!r}")
    print(line)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="op", required=True)
    s = sub.add_parser("start")
    s.add_argument("--res", default="1024", choices=list(RESOLUTION_KEYS))
    s.add_argument("--gdb", action="store_true", help="wait for a debugger before the BIOS runs")
    s.add_argument("--rtc", help="start the clock at this time, e.g. 2026-10-20T10:05:00")
    s.add_argument("--mem", type=int, default=32, help="memory in MB, like make qemu's -m 32m")
    s.add_argument("--disk-bus", default="ide", choices=["ide", "virtio"],
                   help="virtio: the BIOS boots from the disk, but the kernel's IDE driver doesn't see it, as on a machine booted from USB")
    sub.add_parser("stop")
    sub.add_parser("status")
    s = sub.add_parser("pick"); s.add_argument("res", choices=list(RESOLUTION_KEYS))
    s = sub.add_parser("log")
    s.add_argument("--all", action="store_true"); s.add_argument("--raw", action="store_true")
    s.add_argument("--grep"); s.add_argument("--tail", type=int)
    s = sub.add_parser("wait"); s.add_argument("pattern")
    s.add_argument("--timeout", type=float, default=30); s.add_argument("--all", action="store_true")
    s = sub.add_parser("shot"); s.add_argument("file", nargs="?")
    sub.add_parser("text")
    s = sub.add_parser("type"); s.add_argument("text")
    s = sub.add_parser("key"); s.add_argument("names", nargs="+")
    sub.add_parser("windows")
    s = sub.add_parser("focus"); s.add_argument("name")
    s = sub.add_parser("shell"); s.add_argument("command")
    s = sub.add_parser("click"); s.add_argument("x", type=int); s.add_argument("y", type=int)
    s.add_argument("--right", action="store_true")
    s = sub.add_parser("move"); s.add_argument("x", type=int); s.add_argument("y", type=int)
    s = sub.add_parser("drag")
    for a in ("x1", "y1", "x2", "y2"):
        s.add_argument(a, type=int)
    s = sub.add_parser("mon"); s.add_argument("command")
    a = p.parse_args()

    if a.op == "start": start(a)
    elif a.op == "stop": stop(a)
    elif a.op == "status": status(a)
    elif a.op == "pick": pick(a.res)
    elif a.op == "log": log(a)
    elif a.op == "wait": wait(a)
    elif a.op == "shot": shot(a.file)
    elif a.op == "text": print("\n".join(vga_text()).rstrip("\n"))
    elif a.op == "type": type_text(a.text.encode().decode("unicode_escape"))
    elif a.op == "key": keys(a.names)
    elif a.op == "windows": windows_cmd()
    elif a.op == "focus": focus(a.name)
    elif a.op == "shell": shell(a.command)
    elif a.op == "click": click(a.x, a.y, 2 if a.right else 1)
    elif a.op == "move": move(a.x, a.y)
    elif a.op == "drag": drag(a.x1, a.y1, a.x2, a.y2)
    elif a.op == "mon": print(monitor(a.command))


if __name__ == "__main__":
    main()
