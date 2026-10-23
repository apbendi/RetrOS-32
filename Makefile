include make/common.mk

ROOT_DIR := $(CURDIR)
BUILD_DIR := bin
INCLUDE_DIR := $(ROOT_DIR)/include
HOST_CC ?= gcc

CFLAGS := $(FREESTANDING_CFLAGS) $(KERNEL_DEBUG_CFLAGS) -std=gnu11 -O1 -D__KERNEL -I $(INCLUDE_DIR)
ASFLAGS := $(ARCH_ASFLAGS)
LDFLAGS := $(ARCH_LDFLAGS)

HOSTFWD_PORT ?= 8080
QEMU_NETDEV = -netdev user,id=net0,hostfwd=tcp::$(HOSTFWD_PORT)-:80
QEMU_OPS = -device e1000,netdev=net0 -serial stdio $(QEMU_NETDEV) -object filter-dump,id=net0,netdev=net0,file=dump.dat -m 32m -rtc base=localtime

# QEMU 11's Cocoa display draws one guest pixel per screen pixel, so on a Retina display its window is half the size
# it used to be. apbendi/tap's qemu-hidpi is QEMU with -display cocoa,scale=N, where scale=1 is the size before 11.0.
# It's keg-only, so it's run from where Homebrew keeps it, when it's installed.
comma := ,
ifeq ($(OS),Windows_NT)
  QEMU ?= qemu-system-i386.exe
else ifeq ($(UNAME_S),Darwin)
  QEMU_HIDPI := $(shell brew --prefix 2>/dev/null)/opt/qemu-hidpi/bin/qemu-system-i386
  QEMU ?= $(if $(wildcard $(QEMU_HIDPI)),$(QEMU_HIDPI),qemu-system-i386)
  # Stock QEMU rejects the scale option, so it's only passed to a QEMU that lists it
  QEMU_HAS_SCALE = $(shell $(QEMU) -h 2>/dev/null | grep -F 'scale=<number>')
  QEMU_DISPLAY ?= $(if $(QEMU_HAS_SCALE),-display cocoa$(comma)scale=1)
  # make qemu suggests the patched build when it runs stock QEMU 11 or later
  QEMU_HINT = $(if $(QEMU_HAS_SCALE),,$(shell v=$$($(QEMU) --version 2>/dev/null | \
    sed -n 's/^QEMU emulator version \([0-9]*\).*/\1/p'); [ -n "$$v" ] && [ "$$v" -ge 11 ] && echo yes))
else
  QEMU ?= qemu-system-i386
endif

ifeq ($(UNAME_S),Darwin)
  GRUB ?= grub-mkrescue /usr/local/lib/grub/i386-pc/ -o myos.iso legacy/multiboot
else
  GRUB ?= grub-mkrescue /usr/lib/grub/i386-pc/ -o myos.iso legacy/multiboot
endif

PROGRAMOBJ := $(addprefix $(BUILD_DIR)/,shell.o networking.o dhcpd.o tcpd.o logd.o taskbar.o about.o)
GFXOBJ := $(addprefix $(BUILD_DIR)/,window.o component.o composition.o gfxlib.o api.o theme.o core.o)
KERNELOBJ := $(addprefix $(BUILD_DIR)/,kernel.o terminal.o helpers.o pci.o virtualdisk.o windowmanager.o icons.o vga.o \
	libc.o interrupts.o irs_entry.o timer.o gdt.o smp.o keyboard.o pcb.o pcb_queue.o memory.o vmem.o kmem.o e1000.o display.o env.o conf.o \
	sync.o kthreads.o ata.o atapi.o bitmap.o rtc.o tss.o kutils.o login.o cmds.o diskdev.o scheduler.o work.o rbuffer.o errors.o kclock.o tar.o color.o loopback.o \
	serial.o io.o syscalls.o list.o hashmap.o vbe.o ksyms.o windowserver.o encoding.o mouse.o ipc.o sysinf.o font8.o net.o fs.o fatfs.o partition.o \
	admin.o usermanager.o user.o group.o snake.o msgbox.o kevents.o textmode.o lz.o timefmt.o)
KERNELOBJ += $(PROGRAMOBJ) $(GFXOBJ)

BOOTOBJ := $(BUILD_DIR)/bootloader.o
LIBOBJ := $(addprefix $(BUILD_DIR)/,printf.o syscall.o graphics.o netlib.o http.o)
KERNEL_SUPPORT := $(addprefix $(BUILD_DIR)/,kcrt0.o multiboot.o)

ALL_OBJS := $(BOOTOBJ) $(KERNELOBJ) $(LIBOBJ) $(KERNEL_SUPPORT)
DEPS := $(ALL_OBJS:.o=.d)

.PHONY: all iso compile kernel net_objects fs_objects multiboot_kernel bootblock symbols apps tools tests build img filesystem create_fs bare re_apps clean test bindir grub grub_fix qemu qemu-headless qemu_kernel docker docker-rebuild reset sync rsync mount run ls vdi

all: img

iso: compile tests apps tools img
	@echo "Finished building ISO image."

compile: bindir $(LIBOBJ) bootblock kernel
	@echo "[compile] done."

bootblock: $(BOOTOBJ)
	$(QUIET)$(LD) $(LDFLAGS) -o $(BUILD_DIR)/bootblock $^ -Ttext 0x7C00 --oformat=binary

multiboot_kernel: $(BUILD_DIR)/multiboot.o $(KERNELOBJ)
	@echo "[kernel] linking multiboot kernel..."
	$(QUIET)$(LD) -o $(BUILD_DIR)/kernelout $^ $(LDFLAGS) -T ./boot/multiboot.ld

# The kernel is linked once, as an ELF (bin/symbols), and the flat image that boots is copied out of it,
# so every address nm, objdump, lldb and symbols.map give, data included, is the booted kernel's.
kernel: $(BUILD_DIR)/kcrt0.o $(KERNELOBJ)
	@echo "[kernel] linking kernel..."
	$(QUIET)$(LD) -o $(BUILD_DIR)/symbols $^ $(LDFLAGS) -T ./kernel/linker.ld
	$(QUIET)$(OBJCOPY) -O binary $(BUILD_DIR)/symbols $(BUILD_DIR)/kernelout

symbols: kernel
	@echo "[kernel] generating symbols..."
	$(QUIET)nm -C -n $(BUILD_DIR)/symbols | grep ' [Tt] ' | sed 's/ [Tt] / /' > rootfs/sysutil/symbols.map

build: bin/build

bin/build: tools/build.c
	$(call make_dir,$(BUILD_DIR))
	$(QUIET)$(HOST_CC) tools/build.c -O2 -Wall -o $(BUILD_DIR)/build
	@echo "[host] built $(BUILD_DIR)/build"

tools: bin/build
	$(MAKE) -C tools

tests: compile
	$(MAKE) -C tests

# net/ and fs/ build these with their own Makefiles, which track header dependencies.
# The phony targets run those Makefiles on every build, once each, and they rebuild what changed.
$(BUILD_DIR)/net.o: net_objects ;

$(BUILD_DIR)/fs.o $(BUILD_DIR)/fatfs.o: fs_objects ;

net_objects:
	$(MAKE) -C net

fs_objects:
	$(MAKE) -C fs

# The standard C library comes first: TCC's cross compiler, in apps/, links programs against it
apps:
	$(MAKE) -C libc
	$(MAKE) -C apps

img: tools compile apps symbols create_fs sync
	@echo "Finished creating the image."

filesystem:
	dd if=/dev/zero of=filesystem.image bs=512 count=390

create_fs: bin/build
	dd if=/dev/zero of=filesystem.image bs=512 count=390
	$(BUILD_DIR)/build

bare: compile create_fs

re_apps: apps create_fs sync

clean:
	$(MAKE) -C net clean
	$(MAKE) -C fs clean
	$(MAKE) -C apps clean
	$(MAKE) -C libc clean
	$(MAKE) -C tests clean
	$(QUIET)$(RM_F) $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d $(BUILD_DIR)/bootblock $(BUILD_DIR)/kernelout $(BUILD_DIR)/symbols $(BUILD_DIR)/build
	$(QUIET)$(RM_F) .depend filesystem.image filesystem.test
	@echo "Cleaned kernel outputs."

test: clean compile tests

bindir:
	$(call make_dir,rootfs/bin)
	$(call make_dir,$(BUILD_DIR))

grub_fix:
	$(QUIET)$(RM_F) $(BUILD_DIR)/kernel.o

grub: CFLAGS += -DGRUB_MULTIBOOT
grub: grub_fix apps multiboot_kernel
	cp $(BUILD_DIR)/kernelout legacy/multiboot/boot/myos.bin
	$(GRUB)

qemu_kernel: CFLAGS += -DGRUB_MULTIBOOT
qemu_kernel: grub_fix multiboot_kernel
	$(QEMU) $(QEMU_OPS) $(QEMU_DISPLAY) -kernel $(BUILD_DIR)/kernelout

docker-rebuild:
	docker-compose build --no-cache

reset: clean img

docker:
	docker-compose up

vdi: clean docker
	qemu-img convert -f raw -O vdi boot.img boot.vdi

QEMU_CMD = $(QEMU) $(QEMU_OPS) -drive file=RetrOS-32-debug.img,format=raw,index=0,media=disk

QEMU_HEADLESS_CMD = $(QEMU_CMD) -display none

qemu:
	$(if $(QEMU_HINT),@echo "Note: QEMU 11 and later show a small window on Retina displays. To fix it: brew install apbendi/tap/qemu-hidpi")
	$(QEMU_CMD) $(QEMU_DISPLAY)

qemu-headless:
	$(QEMU_HEADLESS_CMD)

sync:
	mkdir -p mnt
	sudo mount -o shortname=winnt RetrOS-32-debug.img ./mnt
	sudo cp -r ./rootfs/* ./mnt/
	sudo umount ./mnt
	@echo "Finished syncing."

rsync:
	mkdir -p mnt
	sudo mount -o shortname=winnt RetrOS-32-debug.img ./mnt
	sudo rsync -av --update ./rootfs/ ./mnt/
	sudo umount ./mnt
	@echo "Finished rsyncing."

mount:
	sudo mount -o shortname=winnt RetrOS-32-debug.img ./mnt

run: img qemu

ls:
	find -name '*.[ch]' | xargs wc -l

# Build rules
-include $(DEPS)

$(BUILD_DIR)/%.o: */%.c
	$(call make_dir,$(BUILD_DIR))
	$(QUIET)$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
	$(QUIET)echo "[CC ] $<"

$(BUILD_DIR)/%.o: kernel/*/%.c
	$(call make_dir,$(BUILD_DIR))
	$(QUIET)$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
	$(QUIET)echo "[CC ] $<"

$(BUILD_DIR)/%.o: */%.s
	$(call make_dir,$(BUILD_DIR))
	$(QUIET)$(AS) $(ASFLAGS) -c $< -o $@
	$(QUIET)echo "[AS ] $<"
