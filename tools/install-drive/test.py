#!/usr/bin/env python3
"""
Checks the install drive, out/install-drive.img, in QEMU: boots it, installs RetrOS with it onto a virtual internal
drive, and checks that drive afterwards (README.md, "Checking a drive in QEMU").

QEMU's PC boots in legacy BIOS mode, from the install drive attached by USB. The internal drive is an IDE drive with
a GPT like a Windows install's. The test picks SystemRescue's serial console entry, which puts its shell on QEMU's
serial port, where the test types into it.

Usage: tools/install-drive/test.py [--keep]
"""

import argparse
import hashlib
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import uuid
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
DRIVE = os.path.join(HERE, "out", "install-drive.img")
TEST_DIR = os.path.join(HERE, "out", "test")
QEMU = "qemu-system-x86_64"

INTERNAL_MB = 256
SECTOR = 512


def make_gpt_drive(path, mb):
    """
    Writes a drive with a GPT like a Windows install's: a protective MBR, the GPT's header and partition entries at the
    drive's start, their copies at its end, and an EFI system partition and a data partition.
    """
    sectors = mb * 2048
    last = sectors - 1
    first_usable, last_usable = 34, last - 33

    def entry(type_guid, first, end, name):
        return (uuid.UUID(type_guid).bytes_le + uuid.uuid4().bytes_le + struct.pack("<QQQ", first, end, 0)
                + name.encode("utf-16-le").ljust(72, b"\0"))

    entries = entry("C12A7328-F81F-11D2-BA4B-00A0C93EC93B", 2048, 2048 + 204800 - 1, "EFI system partition")
    entries += entry("EBD0A0A2-B9E5-4433-87C0-68B6B72699C7", 2048 + 204800, last_usable, "Basic data partition")
    entries = entries.ljust(128 * 128, b"\0")
    disk_guid = uuid.uuid4().bytes_le

    def header(my_lba, alternate_lba, entries_lba):
        h = struct.pack("<8sIIIIQQQQ16sQIII", b"EFI PART", 0x00010000, 92, 0, 0, my_lba, alternate_lba, first_usable,
                        last_usable, disk_guid, entries_lba, 128, 128, zlib.crc32(entries))
        return (h[:16] + struct.pack("<I", zlib.crc32(h)) + h[20:]).ljust(SECTOR, b"\0")

    mbr = bytearray(SECTOR)
    mbr[446:462] = struct.pack("<B3sB3sII", 0, b"\x00\x02\x00", 0xEE, b"\xff\xff\xff", 1, min(last, 0xFFFFFFFF))
    mbr[510:512] = b"\x55\xaa"

    with open(path, "wb") as f:
        f.truncate(sectors * SECTOR)
        f.write(mbr)
        f.seek(1 * SECTOR)
        f.write(header(1, last, 2))
        f.write(entries)
        f.seek((last - 32) * SECTOR)
        f.write(entries)
        f.write(header(last, 1, last - 32))


def has_backup_gpt(path):
    with open(path, "rb") as f:
        f.seek(-SECTOR, os.SEEK_END)
        return f.read(8) == b"EFI PART"


class Serial:
    """QEMU's serial port, through a Unix socket: what it prints is collected, and logged to a file."""

    def __init__(self, path, log_path):
        self.sock = socket.socket(socket.AF_UNIX)
        for _ in range(100):
            try:
                self.sock.connect(path)
                break
            except (FileNotFoundError, ConnectionRefusedError):
                time.sleep(0.1)
        else:
            sys.exit(f"test: couldn't connect to QEMU's serial port at {path}")
        self.raw = ""
        self.mark = 0
        self.log = open(log_path, "w")
        self.lock = threading.Lock()
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        while True:
            try:
                data = self.sock.recv(4096)
            except OSError:
                return
            if not data:
                return
            s = data.decode("utf-8", "replace")
            with self.lock:
                self.raw += s
            self.log.write(s)
            self.log.flush()

    def send(self, s):
        """Types s, and makes what's printed from now on what wait looks at."""
        with self.lock:
            self.mark = len(self.raw)
        self.sock.sendall(s.encode())

    def text(self):
        """
        What's been printed since the last send, without terminal escapes, like colors, cursor moves and the shell's
        notes to the terminal. They're taken out of the whole text each time, since one can be split between reads.
        """
        with self.lock:
            raw = self.raw[self.mark:]
        return re.sub(r"\x1b(\[[0-9;?]*[A-Za-z]|\][^\x07\x1b]*(\x07|\x1b\\)|[()][0-9A-Za-z]|[=>78])|\r", "", raw)

    def wait(self, pattern, timeout, what):
        """Waits for pattern in what's been printed since the last send, and returns its match."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            m = re.search(pattern, self.text())
            if m:
                return m
            time.sleep(0.2)
        sys.exit(f"test: no {what} after {timeout} s. The console's log is in {self.log.name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.strip().split("\n\n")[0])
    parser.add_argument("--keep", action="store_true", help="leave the internal drive and the console's log in out/test")
    args = parser.parse_args()

    if not os.path.exists(DRIVE):
        sys.exit("test: there's no out/install-drive.img. Build it first: docker compose run --rm install-drive")
    if not shutil.which(QEMU):
        sys.exit(f"test: there's no {QEMU}. On a Mac: brew install qemu")

    shutil.rmtree(TEST_DIR, ignore_errors=True)
    os.makedirs(TEST_DIR)
    internal = os.path.join(TEST_DIR, "internal.img")
    make_gpt_drive(internal, INTERNAL_MB)
    assert has_backup_gpt(internal)

    # Unix socket paths have a short limit, so the sockets go in a short temporary directory
    sockets = tempfile.mkdtemp(prefix="install-drive-")
    serial_path = os.path.join(sockets, "serial")
    monitor_path = os.path.join(sockets, "monitor")

    qemu = subprocess.Popen([
        QEMU, "-machine", "pc", "-m", "2048", "-smp", "2", "-accel", "tcg,thread=multi",
        "-display", "none", "-no-reboot",
        "-chardev", f"socket,id=serial,path={serial_path},server=on,wait=off", "-serial", "chardev:serial",
        "-monitor", f"unix:{monitor_path},server=on,wait=off",
        "-device", "qemu-xhci,id=xhci",
        # snapshot=on: nothing SystemRescue does changes the drive image
        "-drive", f"if=none,id=usbdrive,file={DRIVE},format=raw,snapshot=on",
        "-device", "usb-storage,bus=xhci.0,drive=usbdrive,bootindex=0",
        "-drive", f"if=none,id=internal,file={internal},format=raw",
        "-device", "ide-hd,drive=internal,bus=ide.0",
    ], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=open(os.path.join(TEST_DIR, "qemu.log"), "w"))

    try:
        serial = Serial(serial_path, os.path.join(TEST_DIR, "console.log"))
        monitor = socket.socket(socket.AF_UNIX)
        monitor.connect(monitor_path)

        def keys(*names):
            for name in names:
                monitor.sendall(f"sendkey {name}\n".encode())
                time.sleep(0.15)

        start = time.time()
        print("test: booting the install drive")
        select_serial_console(serial, keys)

        serial.wait(r"root@\S+ [^\n]*# $", 900, "shell prompt on the serial console")
        print(f"test: SystemRescue's shell is up, after {time.time() - start:.0f} s")

        # As someone at the PC would: the first command, Up brings back the one retros.yaml puts in the history
        serial.send("\x1b[A\r")
        drive = serial.wait(r"Type (\S+) to go ahead|install-retros: .*", 120,
                            "confirmation prompt from install-retros.sh, brought back with Up")
        if not drive.group(1):
            sys.exit(f"test: install-retros.sh stopped: {drive.group(0)}")
        if drive.group(1) != "sda":
            sys.exit(f"test: install-retros.sh picked {drive.group(1)}, not sda, the internal drive")
        serial.send("sda\n")
        result = serial.wait(r"RetrOS is on /dev/sda, and it matches the image|install-retros: .*", 600,
                             "result from install-retros.sh")
        if result.group(0).startswith("install-retros:"):
            sys.exit(f"test: install-retros.sh failed: {result.group(0)}")
        print(f"test: install-retros.sh wrote the image, after {time.time() - start:.0f} s")

        # What the test checks the internal drive against
        serial.send("stat -c 'SIZE %s' /run/archiso/bootmnt/RetrOS-32-debug.img; "
                    "sha256sum /run/archiso/bootmnt/RetrOS-32-debug.img\n")
        size = int(serial.wait(r"SIZE (\d+)", 60, "size of the RetrOS image on the drive").group(1))
        digest = serial.wait(r"([0-9a-f]{64})  /run", 120, "checksum of the RetrOS image").group(1)

        serial.send("poweroff\n")
        try:
            qemu.wait(120)
        except subprocess.TimeoutExpired:
            pass
    finally:
        if qemu.poll() is None:
            qemu.kill()
            qemu.wait()
        shutil.rmtree(sockets, ignore_errors=True)

    with open(internal, "rb") as f:
        written = f.read(size)
    failures = []
    if hashlib.sha256(written).hexdigest() != digest:
        failures.append("the internal drive doesn't start with the RetrOS image")
    if has_backup_gpt(internal):
        failures.append("the GPT's copy at the internal drive's end is still there")
    for failure in failures:
        print(f"test: FAILED: {failure}")
    if not failures:
        print("test: passed. The internal drive starts with the RetrOS image, and the old GPT is gone")
    if not args.keep:
        shutil.rmtree(TEST_DIR, ignore_errors=True)
    sys.exit(1 if failures else 0)


def select_serial_console(serial, keys):
    """
    Picks "Boot SystemRescue with serial console" in SystemRescue's boot menu. syslinux draws the menu on the serial
    port too (SERIAL in sysresccd_head.cfg). Esc leaves the menu for syslinux's boot: prompt, where the entry is picked
    by its label, so it doesn't matter where it is in the menu.
    """
    serial.wait(r"with serial console", 300, "boot menu on the serial console")
    keys("esc")
    serial.wait(r"boot: *$", 30, "boot: prompt")
    serial.send("sysresccd-serial\r")


if __name__ == "__main__":
    main()
