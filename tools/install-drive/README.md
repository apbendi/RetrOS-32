# The install drive

A USB drive that installs RetrOS-32 on a PC's internal drive.

RetrOS has to be on the internal drive to have its files. A PC can boot it from a USB drive, since the BIOS reads the kernel, but RetrOS has no USB driver, so it can't read anything from that drive afterwards. With the image on the internal drive, RetrOS boots from it and reads its apps and files from it with its IDE driver.

The install drive boots SystemRescue, a Linux system for repairs, which copies the RetrOS image onto the internal drive. It's an ordinary FAT32 drive, so you write it once, and put each new RetrOS build on it by copying the file, from a Mac's Finder or anything else.

## What's here

| File | What it's for |
|---|---|
| `build.sh` | Builds the drive image, `out/install-drive.img`, in a container (`docker compose run --rm install-drive`) |
| `Dockerfile` | That container: Debian 12, with syslinux and the tools for FAT and ISO files |
| `systemrescue.conf` | The SystemRescue release the drive is built from, its checksum, and the key it's signed with |
| `install-retros.sh` | Goes on the drive. You run it on the PC, in SystemRescue, and it writes RetrOS to the internal drive |
| `retros.yaml` | Goes on the drive too: SystemRescue's settings, which put the command that runs `install-retros.sh` in its shell's history |
| `test.py` | Boots the drive in QEMU and installs RetrOS onto a virtual internal drive with it |
| `cache/`, `out/` | SystemRescue's download, and the drive image. Git ignores both |

## Making a drive

You need Docker, and a USB drive of at least 4 GB, which this erases.

1. **Build RetrOS:** `docker compose up`, so the drive gets a fresh `RetrOS-32-debug.img`. Building the drive without it works too, and you copy the image on later.
2. **Build the drive image:** `docker compose run --rm install-drive`. The first time, it downloads SystemRescue (1.4 GB) into `cache/`. Every time, it checks the download's checksum and signature, then writes `out/install-drive.img`, about 2 GB.
3. **Write it to the USB drive.** On a Mac:
   ```
   diskutil list                          # the USB drive is an "external, physical" disk: /dev/diskN
   diskutil unmountDisk /dev/diskN
   sudo dd if=tools/install-drive/out/install-drive.img of=/dev/rdiskN bs=4m
   diskutil eject /dev/diskN
   ```
   On Linux, `sudo dd if=tools/install-drive/out/install-drive.img of=/dev/sdX bs=4M conv=fsync`.

The drive is named after the SystemRescue release, like `RESCUE1303`. Keep that name: SystemRescue finds its files by it.

## Installing RetrOS

This erases everything on the PC's internal drive.

1. **Boot the install drive in legacy BIOS mode.** On a Dell Inspiron 11 3147, set Boot List Option to Legacy and SATA Operation to ATA in the BIOS setup (F2), as RetrOS needs anyway, then press F12 at power-on and pick the USB drive from the legacy entries.
2. **Pick SystemRescue's first entry,** or let its menu time out to it. Not "copy system to RAM": that one lets go of the drive, and the script on it with it.
3. **At SystemRescue's prompt, press Up, then Enter.** Up brings back this command, which you can also type:
   ```
   bash /run/archiso/bootmnt/install-retros.sh
   ```
   It finds the internal drive, shows what's on it, and asks you to type its name, like `sda`. Then it erases the drive, writes the RetrOS image, and checks it. If it can't tell which drive is the internal one, it lists them and asks you to name it: `bash /run/archiso/bootmnt/install-retros.sh /run/archiso/bootmnt/RetrOS-32-debug.img sda`.
4. **`poweroff`,** take out the install drive, and turn the PC on. RetrOS starts from the internal drive.

## A new RetrOS build

Copy the new `RetrOS-32-debug.img` onto the install drive, replacing the old one, and install again. Take it from a fresh `docker compose up`: `make qemu` boots `RetrOS-32-debug.img` itself, and changes it.

Leave SystemRescue's files on the drive alone. One of them, `ldlinux.sys`, is hidden and read-only, and the drive's boot code finds it by where it is on the drive, so the drive stops booting if it's moved or rewritten.

## Checking a drive in QEMU

`tools/install-drive/test.py` checks `out/install-drive.img` without a PC. It needs `qemu-system-x86_64` (`brew install qemu`) and a RetrOS image on the drive, and takes a few minutes, since QEMU emulates the x86 CPU on an Arm Mac.

- **The PC:** QEMU's, whose BIOS boots legacy-style. The install drive is attached by USB, and the internal drive is an IDE drive holding a GPT like a Windows install's.
- **The install:** the test picks SystemRescue's serial console entry, so it can type into its shell. There it presses Up and Enter, as you would, and answers `install-retros.sh`'s question.
- **Afterwards,** it checks that the internal drive starts with the image, byte for byte, and that the GPT's copy at the drive's end is gone.

`--keep` leaves the virtual internal drive and the console's log in `out/test/`.

## A new SystemRescue release

1. In `systemrescue.conf`, set `SYSRESCUE_VERSION`, and `SYSRESCUE_SHA256` from the release's `.sha256` file on [the download page](https://www.system-rescue.org/Download/).
2. Run `docker compose run --rm install-drive`. It also checks the release's signature against the key `systemrescue.conf` names. If SystemRescue has moved to a new key, check the new one, then set `SYSRESCUE_KEY` and `SYSRESCUE_KEY_FINGERPRINT`.
3. Run `test.py`.

## How the drive boots

In legacy mode, a BIOS reads a drive's first sector and runs it, and knows nothing about files. So the drive has a chain of boot code, each piece loading the next, all from syslinux:

1. **Sector 0:** syslinux's MBR code, which loads the first sector of the partition marked active.
2. **The FAT32 partition's boot sector:** syslinux's boot code, around the FAT fields, which loads `ldlinux.sys`. Its sectors were recorded when syslinux was installed, which is why it mustn't move.
3. **`ldlinux.sys`** reads FAT32, so from here on everything is a file: it shows SystemRescue's own syslinux menu, and loads Linux and its startup files.
4. **Linux** finds the partition by its label, which the menu passes it (`archisolabel=`), and runs SystemRescue from it, with the partition mounted at `/run/archiso/bootmnt`.

On the CD image, SystemRescue boots with isolinux, syslinux's version for CDs. `build.sh` copies the CD's files onto the partition and installs syslinux in the same directory, so SystemRescue's menu is used as it is. syslinux's installer and the modules it loads (`*.c32`) have to be the same version, so it replaces SystemRescue's modules with the ones from Debian's syslinux, whose installer it uses.

The drive should boot in UEFI mode too, from the same files, but that hasn't been tried.

The inputs are pinned: the SystemRescue release by its checksum, and syslinux by Debian 12's package. Two builds aren't identical byte for byte, since FAT records when each file was written.
