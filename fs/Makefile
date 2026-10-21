include ../make/common.mk

BUILD_DIR := bin
OUTPUTDIR := ../bin
INCLUDE_DIR := ../include

CFLAGS := $(FREESTANDING_CFLAGS) $(KERNEL_DEBUG_CFLAGS) -std=gnu11 -O1 -D__KERNEL -I $(INCLUDE_DIR)
ASFLAGS := $(ARCH_ASFLAGS)
LDFLAGS := $(ARCH_LDFLAGS)

# fs.c, the file layer
FSCORE := $(BUILD_DIR)/fs.o
# FatFs, vendored in fatfs/ (see fatfs/README.md), with fatfs_port.c, the functions it calls into RetrOS, and
# fatfs_iface.c, the file system operations on top of it
FATFSOBJS := $(addprefix $(BUILD_DIR)/fatfs/,ff.o ffunicode.o) $(addprefix $(BUILD_DIR)/,fatfs_port.o fatfs_iface.o)
FSOBJS := $(FSCORE) $(FATFSOBJS)
DEPS := $(FSOBJS:.o=.d)

.PHONY: all fs fatfs clean bindir
all: fs fatfs

fs: $(OUTPUTDIR)/fs.o
fatfs: $(OUTPUTDIR)/fatfs.o

$(OUTPUTDIR)/fs.o: $(FSCORE) | bindir
	$(QUIET)$(LD) -o $@ $(addprefix ./,$(FSCORE)) $(LDFLAGS) -r
	@echo "[fs  ] linked $@"

$(OUTPUTDIR)/fatfs.o: $(FATFSOBJS) | bindir
	$(QUIET)$(LD) -o $@ $(addprefix ./,$(FATFSOBJS)) $(LDFLAGS) -r
	@echo "[fs  ] linked $@"

-include $(DEPS)

# ff.c includes <string.h>, which fatfs/string.h maps to the kernel's libc
$(BUILD_DIR)/fatfs/%.o: fatfs/%.c | bindir
	$(call make_dir,$(BUILD_DIR)/fatfs)
	$(QUIET)$(CC) $(CFLAGS) -I fatfs $(DEPFLAGS) -c $< -o $@
	@echo "[fs  ] cc $(<F)"

$(BUILD_DIR)/%.o: %.c | bindir
	$(QUIET)$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
	@echo "[fs  ] cc $(<F)"

bindir:
	$(call make_dir,$(BUILD_DIR))

clean:
	$(QUIET)$(RM_F) $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d $(BUILD_DIR)/fatfs/*.o $(BUILD_DIR)/fatfs/*.d
	$(QUIET)$(RM_F) $(OUTPUTDIR)/fs.o $(OUTPUTDIR)/fatfs.o
