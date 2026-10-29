
<!-- PROJECT SHIELDS -->
<!--
*** I'm using markdown "reference style" links for readability.
*** Reference links are enclosed in brackets [ ] instead of parentheses ( ).
*** See the bottom of this document for the declaration of the reference variables
*** for contributors-url, forks-url, etc. This is an optional, concise syntax you may use.
*** https://www.markdownguide.org/basic-syntax/#reference-style-links
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]
[![LinkedIn][linkedin-shield]][linkedin-url]
-->

<!-- PROJECT LOGO -->
<br />
<div align="center">
  <h1 align="center">RetrOS 32bit</h1>
  

  <p align="center">
    <img src="https://github.com/joexbayer/RetrOS-32/blob/main/graphics/logo.png?raw=true" width="150">
  </p>

  ![Build](https://github.com/joexbayer/RetrOS-32/actions/workflows/pipeline.yml/badge.svg)
  
  <p align="center">
    Fully custom hobby 32bit operating system project focusing on i386 architecture.
    <br />
    <a href="https://oshub.org/projects/retros-32"><strong>Project OSHub page »</strong></a>
    <br />
    <br />
    <a href="https://github.com/joexbayer/RetrOS-32">View Demo</a>
    ·
    <a href="https://github.com/joexbayer/RetrOS-32/issues">Report Bug</a>
    ·
    <a href="https://github.com/joexbayer/RetrOS-32/issues">Request Feature</a>
  </p>
</div>



<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#roadmap">Roadmap</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#acknowledgments">Acknowledgments</a></li>
  </ol>
</details>



<!-- ABOUT THE PROJECT -->
<p align="center">
  <img src="https://github.com/joexbayer/RetrOS-32/blob/development/docs/pictures/retros.gif?raw=true">
</p>

| | | |
|:-------------------------:|:-------------------------:|:-------------------------:|
| <img src="https://github.com/joexbayer/RetrOS-32/blob/main/docs/pictures/screenshot1.png?raw=true"> | <img src="https://github.com/joexbayer/RetrOS-32/blob/main/docs/pictures/screenshot2.png?raw=true"> | <img src="https://github.com/joexbayer/RetrOS-32/blob/main/docs/pictures/screenshot3.png?raw=true">
## About The Project

<p align="center">32bit Hobby Operatingsystem with graphics, multitasking and networking!</p>
<p align="center"><i>Started: 12.05.2022</i></p>

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<p align="center">
  <img src="https://github.com/joexbayer/RetrOS-32/blob/main/docs/pictures/textmode.png?raw=true">
</p>

### Login
There are 3 default users: system, admin and guest. The password for admin is 'admin', while guest has no password.
Currently there is no difference between admin and guest.

You can create a user with the 'admin' command:
```sh
admin create <username> <password>
```

### Built With

This project is built with C & Assembly for the kernel, utilities and build system. C++ for userspace applications and Make for compilation.
Docker used for crossplatform compilation.

![Docker](https://img.shields.io/badge/docker-%230db7ed.svg?style=for-the-badge&logo=docker&logoColor=white)
![](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Hardware
Tested on:
Lenovo x240,
Asus Eee PC series,
Dell Optiplex 780,
Samsung N150 Plus,
Samsung NP-NC10,
IBM Thinkpad a21p

| | |
|:-------------------------:|:-------------------------:|
| <img src="https://github.com/joexbayer/RetrOS-32/blob/main/docs/pictures/hardware1.jpg?raw=true" width="250"> | <img src="https://github.com/joexbayer/RetrOS-32/blob/main/docs/pictures/hardware2.jpg?raw=true" width="250">

<!-- GETTING STARTED -->
## Getting Started

### Prerequisites

Crossplatform: Docker to compile the image file and QEMU for emulation.<br>
For native compilation you will need:
 * i386-elf-gcc, i386-elf-ld and i386-elf-g++ for MacOS.
 * build-essential and gcc-multilib for Linux / WSL (ubuntu)
 *   Also need: grub2, xorriso and xxd (for using Grub as bootloader)


### Installation

_To compile the kernel and its needed programs you simply need to run *make img* and *make qemu* to open QEMU_

#### Linux
1. Clone the repo
   ```sh
   git clone https://github.com/joexbayer/RetrOS-32.git
   ```
2. Check that all dependencies are installed (Only for debian based distros)
   ```sh
   ./debian.sh
   ```

3. Compile and create image
   ```sh
   make compile
   make img
   ```
   
4. Launch QEMU
   ```sh
   make qemu
   ```

5. Use GRUB (Optional)
   ```sh
   make grub
   ```

#### MacOS

Currently MacOS cannot natively compile the build tools as they depend on 32bit x86 code.
Docker is the simplest way if you still wish to compile the operating system.

1. Clone the repository
   ```sh
   git clone https://github.com/joexbayer/RetrOS-32.git
   ```

2. Build using Docker
   ```sh
   docker-compose up --build
   ```
 
#### Windows
Use Docker (see MacOS section) or WSL (see Linux section).

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- USAGE EXAMPLES -->
## Usage

To run RetrOS-32:
  * http://copy.sh/v86/ : Upload .img file as Hard disk image.
  * QEMU: `make qemu` or `qemu-system-i386 <image name>` 
  * 86Box: emulates a late-90s PC, see below
  * Real hardware: Burn .iso file to USB or CD (Use a GRUB iso)

### 86Box

[86Box](https://86box.net/) emulates real PC hardware with its original BIOSes, so it shows what QEMU's simpler BIOS can hide. RetrOS-32 boots on an emulated ASUS KN97, a Pentium II board from 1997.

1. Install 86Box and the ROM set for its version. On macOS:
   ```sh
   brew install --cask 86box
   curl -L -o /tmp/86box-roms.zip https://github.com/86Box/roms/archive/refs/tags/v6.0.zip
   unzip -q /tmp/86box-roms.zip -d /tmp
   mv /tmp/roms-6.0 ~/Library/Application\ Support/net.86box.86Box/roms
   ```
   Elsewhere, see [86Box's documentation](https://86box.readthedocs.io/).
2. Open 86Box once. Its manager shows the folder it keeps virtual machines in, its system directory: on macOS, `~/Library/Application Support/86Box/Virtual Machines/`.
3. In that folder, make a folder called `RetrOS-32` with this `86box.cfg`, with `hdd_01_fn` set to the image's full path:
   ```ini
   [Machine]
   machine = kn97
   cpu_family = pentium2_klamath
   cpu_speed = 266666666
   cpu_multi = 4
   cpu_use_dynarec = 1
   fpu_type = internal
   mem_size = 32768
   time_sync = local

   [Video]
   gfxcard = millennium_ii

   [Input devices]
   keyboard_type = keyboard_ps2
   mouse_type = ps2

   [Hard disks]
   hdd_01_fn = /full/path/to/RetrOS-32/RetrOS-32-debug.img
   hdd_01_ide_channel = 0:0
   hdd_01_parameters = 63, 16, 66, 0, ide
   hdd_01_speed = ramdisk

   [Ports (COM & LPT)]
   serial1_device = stdio

   [Virtual Console (COM) #1]
   mode = 3
   command = exec 2>/dev/null <"$PTY" >serial.log; stty raw -echo; exec cat
   command_terminal = 0
   ```
4. Restart 86Box, start RetrOS-32, and pick a resolution at the boot menu.

Notes:
  * **Rebuilding:** the VM boots the image the build makes, so a rebuild needs only a restart. Don't rebuild while it runs, or run it alongside `make qemu`: they share the file.
  * **The image's size:** 86Box lengthens the image to fit the disk's 66 cylinders, a little over 32 MB. That's harmless: each build makes it afresh.
  * **The serial log** goes to `serial.log` in the VM's folder as it's written, through a pseudoterminal, so it survives 86Box crashing. That's been tried on macOS; Windows has no pseudoterminals.
  * **No network:** 86Box has no Intel e1000, the only network card RetrOS-32 has a driver for.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Project Structure (TBD)
    NETOS - Project
    ├── Dockerfile 
    ├── LICENSE.txt
    ├── Makefile
    ├── apps (userspace applications)
    │   ├── Makefile
    │   ├── <app>
    │   ├── libcore.a
    │   ├── libgraphic.a
    │   ├── libnet.a
    │   ├── readme.md
    │   └── utils
    │       └── *.cpp
    ├── bin 
    │   └── *.o
    ├── boot (custom bootloader)
    │   ├── bootloader.s
    │   └── multiboot.ld
    ├── docker-compose.yml
    ├── docs (documentation)
    │   ├── *.md
    ├── drivers (kernel drivers)
    │   └── *.c
    ├── fs (filesystem)
    │   ├── Makefile
    │   ├── bin
    │   └── *.c
    ├── graphics
    │   └── *.c
    ├── include
    │   ├── arch
    │   │   └── *.h
    │   ├── fs
    │   │   └── *.h
    │   ├── gfx
    │   │   └── *.h
    │   ├── lib
    │   │   └── *.h
    │   ├── net
    │   │   └── *.h
    │   └── *.h
    ├── kernel (main kernel source files)
    │   ├── arch
    │   │   └── *.c
    │   ├── kthreads
    │   │   └── *.c
    │   └── *.c
    ├── legacy (old code)
    ├── lib (libraries)
    │   └── *.c
    ├── net (networking code)
    │   └── *.c
    ├── readme.md
    ├── rootfs (root filesystem for the OS)
    ├── tests (testing code)
    │   ├── Makefile
    │   ├── bin
    │   ├── *_test.c
    │   └── readme.md
    └── tools (build tools)
        ├── scripts
        └── bin
            └── *.c

<!-- ROADMAP -->
## Roadmap

- [x] Custom Bootloader & GRUB compatible
- [x] Stage 2 bootloader 
- [x] Mountable image
- [x] Users 
- [x] Multi-threaded pre-emptive scheduling
- [x] GDT & TSS kernel / userspace separation
- [x] Interrupt handling
- [x] PS/2 Keyboard & Mouse, PIT, VESA (640x480x8), RTC, Serial drivers, E1000
- [x] PCI.
- [x] 8Bit RGB to 8Bit VGA
- [x] Filesystem
- [x] Textmode VGA only
- [x] Networkstack
  - [x] Ethernet, IP, ARP, UDP
  - [x] Socket API
  - [x] Interfaces
    - [x] Loopback
  - [x] DHCP
  - [x] DNS
  - [x] TCP
  - [x] Netcat style commands
  - [ ] Webserver
  - [ ] FTP
  - [ ] IRC
  - [ ] SSH
  - [ ] Telnet
  - [ ] HTTP
  - [ ] HTML
- [x] ATA IDE Driver
  - [x] Ext2 (like) Filesystem
  - [x] FAT16, support for up to 32mb files.
  - [x] read / write
  - [ ] Atapi
- [x] Memory
  - [x] 32bit Virtual Memory
  - [x] kalloc / kfree (kernel)
  - [x] malloc / free (userspace) 
- [x] Graphics
  - [x] Window Manager
  - [x] GFXLib
  - [x] Double framebuffer rendering
  - [x] Mouse events
  - [x] Dynamic resize
  - [x] Fullscreen
  - [x] Widgets Library
  - [ ] Custom HTML to Widgets
- [x] Terminal, Window Server, Process Informtaion, Finder.
- [x] Library (printf, memcpy, etc)
- [x] IPC
- [x] System calls
- [x] C compiler: TinyCC, running on RetrOS, with a standard C library built from musl
- [x] Custom VM to run bytecode.
- [x] Editor with Syntax Highlighting
- [x] Calculator
- [x] 3D Game
- [x] Snake game (Textmode only)
- [x] Userspace threads (shared virtual memory)
- [x] Remote virtual terminal over TCP
- [x] LZ compression library
- [x] Tools
  - [x] Build
  - [x] Sync / Migration Took
  - [x] mkfs
  - [x] Testing
  - [ ] Create & Encode icons

See the [open issues](https://github.com/joexbayer/RetrOS-32/issues) for a full list of proposed features (and known issues).

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- LICENSE -->
## License

Distributed under the MIT License. See `LICENSE.txt` for more information.

### Third-party code

These directories come from other projects and keep their own licenses:

| Directory | Project | License |
|---|---|---|
| `fs/fatfs` | [FatFs](https://elm-chan.org/fsw/ff/), the FAT file system code | FatFs license, BSD-style (`fs/fatfs/LICENSE.txt`) |
| `apps/tcc/tinycc` | [TinyCC](https://bellard.org/tcc/) (TCC), the C compiler, installed as `/bin/tcc.o` | GNU LGPL version 2.1 (`apps/tcc/tinycc/COPYING`); its runtime library is GPL with a linking exception |
| `libc/musl` | [musl](https://musl.libc.org/), the base of the standard C library for RetrOS programs | MIT (`libc/musl/COPYRIGHT`) |

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- CONTACT -->
## Contact

Joe Bayer

Project Link: [https://github.com/joexbayer/RetrOS-32](https://github.com/joexbayer/RetrOS-32)

<p align="right">(<a href="#readme-top">back to top</a>)</p>
