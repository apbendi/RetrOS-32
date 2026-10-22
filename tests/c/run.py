#!/usr/bin/env python3
"""
The C harness: runs C programs on RetrOS and compares their output with the same programs on Linux.

1. In the Docker builder: builds the image (make img) and the harness's programs (tests/c/Makefile). Each program in
   cases/ is built for Linux with gcc and glibc, and run there, which gives the expected output. It's also built for
   RetrOS twice: with gcc on RetrOS's C library (variant "gcc") and with TCC's cross compiler (variant "tcc").
2. On this machine: boots a copy of out/test.img in QEMU with the run-in-qemu skill's qctl.py, runs each RetrOS
   program from the shell, in /t, and collects what it printed from the serial log.
3. Compares the two, and compares the results with expected.txt, like tests/fs.

It also runs TinyCC's own tests (apps/tcc/tinycc/tests/tests2), as "tests2.NAME": built with the cross compiler as
tests2.conf says, run from /t/2, and compared with each test's .expect, which has the compiler's messages first.

Usage: tests/c/run.py [-v] [--no-build] [CASE-GLOB ...]
"""
import argparse, fnmatch, os, re, shutil, subprocess, sys, time

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
    qctl's clean(). A line is logged at its newline, or in pieces of 127 characters, and an unfinished last line when
    the program exits."""
    lines, line = [], ""
    for c in text:
        if c != "\n":
            line += c
            if len(line) < LOG_LINE:
                continue
        lines.append(qctl.clean(line))
        line = ""
    if line:
        lines.append(qctl.clean(line))
    return lines


def program_output(lines, program):
    rx = re.compile(r"^term\[" + re.escape(writer(program)) + r"\]: ?(.*)$")
    return [m.group(1) for m in (rx.match(l) for l in lines) if m]


# The longest name a process gets (PCB_MAX_NAME_LENGTH - 1): a longer path keeps its end, and the log shows that
NAME_MAX = 24


def writer(program):
    return program[-NAME_MAX:]


def crash(lines):
    for l in lines:
        if re.search(r"\[exception\]|Page Fault Address|KERNEL PANIC", l):
            return l
    return None


class RetrOS:
    """QEMU running the harness's image, restarted after a program hangs or the kernel panics"""

    def __init__(self):
        self.running = False
        self.sessions = 0

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
        # Keep each session's log: a restart replaces qctl's
        if os.path.exists(qctl.SERIAL):
            self.sessions += 1
            shutil.copyfile(qctl.SERIAL, os.path.join(OUT, f"serial-{self.sessions}.log"))

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
        write_diff(f"{case}.{variant}", expected, got, verbose)
    return "pass" if not problems else "fail: " + "; ".join(problems)


def read_tests2_conf():
    """tests2.conf's skips (name: reason) and arguments (name: the rest of the line, typed as it is)"""
    skips, args = {}, {}
    for line in open(os.path.join(HERE, "tests2.conf")):
        parts = line.split(None, 2)
        if len(parts) < 2 or line.startswith("#"):
            continue
        rest = parts[2].strip() if len(parts) > 2 else ""
        if parts[0] == "skip":
            skips[parts[1]] = rest
        elif parts[0] == "args":
            args[parts[1]] = rest
    return skips, args


def squeeze(lines):
    """Lines compared as the tests' Makefile compares them, with diff -b: runs of spaces count as one, and spaces at
    the ends of lines don't count"""
    return [re.sub(r"[ \t]+", " ", l).rstrip() for l in lines]


def run_tests2(retros, name, args, verbose):
    """The result for one of TinyCC's tests"""
    built = os.path.join(OUT, "tests2", name)
    expected = squeeze(log_lines(open(built + ".expect", encoding="latin-1").read()))
    got = squeeze(log_lines(open(built + ".compile", encoding="latin-1").read()))

    problems = []
    if os.path.exists(built):
        program = f"/t/2/{name}"
        lines, finished = retros.command(f"exec {program} {args}".rstrip())
        # The tests' Makefile takes their directory out of what they print, and theirs is /t/2 here
        got += squeeze(l.replace("/t/2/", "") for l in program_output(lines, program))
        if not finished:
            problems.append(f"didn't finish within {TIMEOUT} s")
        crashed = crash(lines)
        if crashed:
            problems.append("crashed: " + crashed.strip())
    if got != expected:
        problems.append("output")
        write_diff(f"tests2.{name}", expected, got, verbose)
    return "pass" if not problems else "fail: " + "; ".join(problems)


def write_diff(name, expected, got, verbose):
    import difflib
    detail = os.path.join(OUT, "diff", name)
    os.makedirs(os.path.dirname(detail), exist_ok=True)
    with open(detail, "w") as f:
        f.write("--- expected\n+++ RetrOS\n")
        f.writelines(l + "\n" for l in difflib.unified_diff(expected, got, lineterm="", n=1))
    if verbose:
        print(open(detail).read())


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

    def wanted(name):
        return not args.cases or any(fnmatch.fnmatch(name, g) for g in args.cases)

    cases = [c for c in sorted(f[:-2] for f in os.listdir(os.path.join(HERE, "cases")) if f.endswith(".c")) if wanted(c)]
    skips, tests2_args = read_tests2_conf()
    tests2_dir = os.path.join(ROOT, "apps", "tcc", "tinycc", "tests", "tests2")
    tests2 = sorted(f[:-2] for f in os.listdir(tests2_dir) if re.match(r"^[0-9]{2,3}_.*\.c$", f))
    tests2 = [t for t in tests2 if wanted("tests2." + t)]

    expected = read_expected()
    diffs = os.path.join(OUT, "diff")
    if os.path.isdir(diffs):
        for name in os.listdir(diffs):
            os.remove(os.path.join(diffs, name))
    for name in os.listdir(OUT):
        if name.startswith("serial-"):
            os.remove(os.path.join(OUT, name))
    retros = RetrOS()
    results, changed = {}, 0
    try:
        retros.start()
        def record(name, result):
            nonlocal changed
            results[name] = result
            was = expected.get(name)
            mark = "" if was == result else f"  <- was {was or 'not run'}"
            changed += was != result
            print(f"{name:<36} {result}{mark}", flush=True)

        for case in cases:
            for variant in VARIANTS:
                record(f"{case}.{variant}", run_case(retros, case, variant, args.v))
        for test in tests2:
            if test in skips:
                record(f"tests2.{test}", "skip: " + skips[test])
            else:
                record(f"tests2.{test}", run_tests2(retros, test, tests2_args.get(test, ""), args.v))
    finally:
        retros.stop()

    with open(os.path.join(OUT, "results.txt"), "w") as f:
        f.writelines(f"{n} {r}\n" for n, r in results.items())
    passed = sum(r == "pass" for r in results.values())
    skipped = sum(r.startswith("skip") for r in results.values())
    print(f"\n{passed} passed, {len(results) - passed - skipped} failed, {skipped} skipped, {changed} changed from tests/c/expected.txt. "
          f"Differences: tests/c/out/diff/")
    sys.exit(1 if changed else 0)


if __name__ == "__main__":
    main()
