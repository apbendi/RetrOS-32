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
tests2.conf says, run from /t/tests2, and compared with each test's .expect, which has the compiler's messages first.

With --native, everything is also compiled on RetrOS itself, by /bin/tcc.o: each case to a file and run (variant
"native") and with tcc -run (variant "run"), and each of TinyCC's tests as its Makefile does it, with -run or to a
file ("tests2.NAME.native").

Usage: tests/c/run.py [-v] [--no-build] [--native] [CASE-GLOB ...]
"""
import argparse, fnmatch, os, re, shutil, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(HERE, "out")
sys.path.insert(0, os.path.join(ROOT, ".claude", "skills", "run-in-qemu"))
import qctl  # noqa: E402

VARIANTS = ["gcc", "tcc"]
# Compiled on RetrOS, with --native
NATIVE_VARIANTS = ["native", "run"]
TCC = "/bin/tcc.o"
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
        # Where the shell should be, after a restart too
        self.cwd = "/t"

    def start(self):
        if qctl.qemu_pid():
            qctl.stop(None)
        args = argparse.Namespace(res="1024", gdb=False, rtc=None, mem=32, disk_bus="ide",
                                  image=os.path.join(OUT, "test.img"))
        qctl.start(args)
        self.running = True
        self.command(f"cd {self.cwd}")

    def stop(self):
        if qctl.qemu_pid():
            qctl.stop(None)
        # QEMU is this process's child, through qctl: until it's reaped, it lingers, and qctl thinks it's running
        for _ in range(50):
            try:
                if os.waitpid(-1, os.WNOHANG) == (0, 0) and not qctl.qemu_pid():
                    break
            except ChildProcessError:
                break
            time.sleep(0.1)
        self.running = False
        # Keep each session's log: a restart replaces qctl's
        if os.path.exists(qctl.SERIAL):
            self.sessions += 1
            shutil.copyfile(qctl.SERIAL, os.path.join(OUT, f"serial-{self.sessions}.log"))

    def command(self, line, timeout=TIMEOUT):
        """Types a shell command and waits for it to finish. Returns the log lines since, and whether it finished."""
        if line.startswith("cd "):
            self.cwd = line[3:]
        if not self.running:
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

    problems = []
    lines = []
    if variant == "native":
        # Compiled on RetrOS first. -w: Linux's output has no compiler warnings to compare with.
        compiled, finished = retros.command(f"exec {TCC} -w /t/{case}.c -o /t/{case}-native")
        lines += compiled
        if not finished:
            problems.append(f"didn't compile within {TIMEOUT} s")
    if variant == "run":
        program = TCC
        command = f"exec {TCC} -w -run /t/{case}.c"
    else:
        program = f"/t/{case}-{variant}"
        command = f"exec {program}"
    ran, finished = retros.command(command)
    lines += ran
    got = program_output(ran, program)

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
    """tests2.conf's settings: skips (name: reason), arguments, flags and extra files (name: the rest of the line, as
    it is), and the names to compile to a file rather than with -run"""
    skips, args, flags, files, norun = {}, {}, {}, {}, set()
    for line in open(os.path.join(HERE, "tests2.conf")):
        parts = line.split(None, 2)
        if len(parts) < 2 or line.startswith("#"):
            continue
        rest = parts[2].strip() if len(parts) > 2 else ""
        if parts[0] == "skip":
            skips[parts[1]] = rest
        elif parts[0] == "args":
            args[parts[1]] = rest
        elif parts[0] == "flags":
            flags[parts[1]] = rest
        elif parts[0] == "files":
            files[parts[1]] = rest
        elif parts[0] == "norun":
            norun.add(parts[1])
    return {"skip": skips, "args": args, "flags": flags, "files": files, "norun": norun}


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
        program = f"/t/tests2/{name}"
        lines, finished = retros.command(f"exec {program} {args}".rstrip())
        # The tests' Makefile takes their directory out of what they print, and theirs is /t/tests2 here
        got += squeeze(l.replace("/t/tests2/", "") for l in program_output(lines, program))
        if not finished:
            problems.append(f"didn't finish within {TIMEOUT} s")
        crashed = crash(lines)
        if crashed:
            problems.append("crashed: " + crashed.strip())
    if got != expected:
        problems.append("output")
        write_diff(f"tests2.{name}", expected, got, verbose)
    return "pass" if not problems else "fail: " + "; ".join(problems)


def run_tests2_native(retros, name, conf, verbose):
    """One of TinyCC's tests, compiled on RetrOS by /bin/tcc.o as their Makefile does it: with -run, or to a file and
    run, for the tests it doesn't run with -run. In /t/tests2, so messages name the file alone."""
    built = os.path.join(OUT, "tests2", name)
    expected = squeeze(log_lines(open(built + ".expect", encoding="latin-1").read()))
    flags = " ".join(x for x in (conf["flags"].get(name, ""), conf["files"].get(name, "")) if x)
    args = conf["args"].get(name, "")

    problems, lines = [], []
    if name in conf["norun"]:
        compiled, finished = retros.command(f"exec {TCC} {flags} {name}.c -o {name}-native".replace("  ", " "))
        lines += compiled
        got = squeeze(program_output(compiled, TCC))
        if not finished:
            problems.append(f"didn't compile within {TIMEOUT} s")
        program = f"/t/tests2/{name}-native"
        ran, finished = retros.command(f"exec {program} {args}".rstrip())
        got += squeeze(program_output(ran, program))
    else:
        program = TCC
        ran, finished = retros.command(f"exec {TCC} {flags} -run {name}.c {args}".replace("  ", " ").rstrip())
        got = squeeze(program_output(ran, program))
    lines += ran
    got = [l.replace("/t/tests2/", "") for l in got]

    if not finished:
        problems.append(f"didn't finish within {TIMEOUT} s")
    crashed = crash(lines)
    if crashed:
        problems.append("crashed: " + crashed.strip())
    if got != expected:
        problems.append("output")
        write_diff(f"tests2.{name}.native", expected, got, verbose)
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
    p.add_argument("--native", action="store_true", help="also compile everything on RetrOS, with /bin/tcc.o")
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
    conf = read_tests2_conf()
    skips = conf["skip"]
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

        variants = VARIANTS + (NATIVE_VARIANTS if args.native else [])
        for case in cases:
            for variant in variants:
                record(f"{case}.{variant}", run_case(retros, case, variant, args.v))
        for test in tests2:
            if test in skips:
                record(f"tests2.{test}", "skip: " + skips[test])
            else:
                record(f"tests2.{test}", run_tests2(retros, test, conf["args"].get(test, ""), args.v))
        if args.native:
            retros.command("cd /t/tests2")
            for test in tests2:
                if test not in skips:
                    record(f"tests2.{test}.native", run_tests2_native(retros, test, conf, args.v))
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
