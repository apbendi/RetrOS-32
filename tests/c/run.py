#!/usr/bin/env python3
"""
The C harness: runs C programs on RetrOS and compares their output with the same programs on Linux.

1. In the Docker builder: builds the image (make img) and the harness's programs (tests/c/Makefile). Each program in
   cases/ is built for Linux with gcc and glibc, and run there, which gives the expected output. It's also built for
   RetrOS twice: with gcc on RetrOS's C library (variant "gcc") and with TCC's cross compiler (variant "tcc").
2. On this machine: boots a copy of out/test.img in QEMU with the run-in-qemu skill's qctl.py, runs each RetrOS
   program from the shell, in /t, and collects what it printed from the serial log.
3. Compares the two, and compares the results with expected.txt, like tests/fs.

Usage: tests/c/run.py [-v] [--no-build] [CASE-GLOB ...]
"""
import argparse, fnmatch, os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(HERE, "out")
sys.path.insert(0, os.path.join(ROOT, ".claude", "skills", "run-in-qemu"))
import qctl  # noqa: E402

VARIANTS = ["gcc", "tcc"]
TIMEOUT = 60
# The longest piece of a line RetrOS's terminal copies to the log at once (TERMINAL_SERIAL_LINE_SIZE - 1)
LOG_LINE = 127


def log_lines(text):
    """What RetrOS's log shows for text a program prints: kernel/terminal.c's __terminal_copy_to_serial, then
    qctl's clean(). A line is logged at its newline, or in pieces of 127 characters; an unfinished last line isn't."""
    lines, line = [], ""
    for c in text:
        if c != "\n":
            line += c
            if len(line) < LOG_LINE:
                continue
        lines.append(qctl.clean(line))
        line = ""
    return lines


def program_output(lines, program):
    rx = re.compile(r"^term\[" + re.escape(program) + r"\]: ?(.*)$")
    return [m.group(1) for m in (rx.match(l) for l in lines) if m]


def crash(lines):
    for l in lines:
        if re.search(r"\[exception\]|Page Fault Address|KERNEL PANIC", l):
            return l
    return None


class RetrOS:
    """QEMU running the harness's image, restarted after a program hangs or the kernel panics"""

    def __init__(self):
        self.running = False

    def start(self):
        if qctl.qemu_pid():
            qctl.stop(None)
        args = argparse.Namespace(res="1024", gdb=False, rtc=None, mem=32, disk_bus="ide",
                                  image=os.path.join(OUT, "test.img"))
        qctl.start(args)
        self.command("cd /t")
        self.running = True

    def stop(self):
        if qctl.qemu_pid():
            qctl.stop(None)
        self.running = False

    def command(self, line, timeout=TIMEOUT):
        """Types a shell command and waits for it to finish. Returns the log lines since, and whether it finished."""
        if not self.running and line != "cd /t":
            self.start()
        qctl.set_mark()
        since = qctl.get_mark()
        qctl.shell(line)
        word = line.split()[0]
        found, _ = qctl.wait_for(r"exec_cmd\(\): Finished " + re.escape(word) + r"$|KERNEL PANIC", timeout, since)
        lines = qctl.serial_lines(since)
        finished = found is not None and "KERNEL PANIC" not in found
        if not finished:
            self.stop()
        return lines, finished


def run_case(retros, case, variant, verbose):
    """The result for one program: "pass", or "fail:" and why"""
    expected_path = os.path.join(OUT, "expected", case + ".out")
    expected = log_lines(open(expected_path, encoding="latin-1").read())

    program = f"/t/{case}-{variant}"
    lines, finished = retros.command(f"exec {program}")
    got = program_output(lines, program)

    problems = []
    if not finished:
        problems.append(f"didn't finish within {TIMEOUT} s")
    crashed = crash(lines)
    if crashed:
        problems.append("crashed: " + crashed.strip())
    if got != expected:
        problems.append("output")
        detail = os.path.join(OUT, "diff", f"{case}.{variant}")
        os.makedirs(os.path.dirname(detail), exist_ok=True)
        with open(detail, "w") as f:
            f.write("--- Linux\n+++ RetrOS\n")
            import difflib
            f.writelines(l + "\n" for l in difflib.unified_diff(expected, got, lineterm="", n=1))
        if verbose:
            print(open(detail).read())
    return "pass" if not problems else "fail: " + "; ".join(problems)


def read_expected():
    results = {}
    path = os.path.join(HERE, "expected.txt")
    if os.path.exists(path):
        for line in open(path):
            if line.strip() and not line.startswith("#"):
                name, _, result = line.rstrip("\n").partition(" ")
                results[name] = result
    return results


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-v", action="store_true", help="print each failure's output difference")
    p.add_argument("--no-build", action="store_true", help="skip the builder: use out/ as it is")
    p.add_argument("cases", nargs="*", help="globs of case names, e.g. 'str*' (default: all)")
    args = p.parse_args()

    if not args.no_build:
        build = subprocess.run(["docker", "compose", "run", "--rm", "--entrypoint", "sh", "builder", "-c",
                                "make img > /dev/null && make -C tests/c"], cwd=ROOT)
        if build.returncode != 0:
            sys.exit("The build failed")

    cases = sorted(f[:-2] for f in os.listdir(os.path.join(HERE, "cases")) if f.endswith(".c"))
    if args.cases:
        cases = [c for c in cases if any(fnmatch.fnmatch(c, g) for g in args.cases)]

    expected = read_expected()
    retros = RetrOS()
    results, changed = {}, 0
    try:
        retros.start()
        for case in cases:
            for variant in VARIANTS:
                name = f"{case}.{variant}"
                result = run_case(retros, case, variant, args.v)
                results[name] = result
                was = expected.get(name)
                mark = "" if was == result else f"  <- was {was or 'not run'}"
                changed += was != result
                print(f"{name:<36} {result}{mark}", flush=True)
    finally:
        retros.stop()

    with open(os.path.join(OUT, "results.txt"), "w") as f:
        f.writelines(f"{n} {r}\n" for n, r in results.items())
    passed = sum(r == "pass" for r in results.values())
    print(f"\n{passed} passed, {len(results) - passed} failed, {changed} changed from tests/c/expected.txt. "
          f"Differences: tests/c/out/diff/")
    sys.exit(1 if changed else 0)


if __name__ == "__main__":
    main()
