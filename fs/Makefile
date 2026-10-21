include ../make/common.mk

BUILD_DIR := bin
OUTPUTDIR := ../bin
INCLUDE_DIR := ../include

CFLAGS := $(FREESTANDING_CFLAGS) $(KERNEL_DEBUG_CFLAGS) -std=gnu11 -O1 -D__KERNEL -I $(INCLUDE_DIR)
ASFLAGS := $(ARCH_ASFLAGS)
LDFLAGS := $(ARCH_LDFLAGS)

FAT16OBJS := $(addprefix $(BUILD_DIR)/,fat16_read.o fat16_write.o fat16.o)
FSCORE := $(addprefix $(BUILD_DIR)/,fs.o fat16_iface.o)
# FatFs, vendored in fatfs/ (see fatfs/README.md), and fatfs_port.c, the functions it calls into RetrOS
FATFSOBJS := $(addprefix $(BUILD_DIR)/fatfs/,ff.o ffunicode.o) $(BUILD_DIR)/fatfs_port.o
FSOBJS := $(FAT16OBJS) $(FSCORE) $(FATFSOBJS)
DEPS := $(FSOBJS:.o=.d)

.PHONY: all fat16 fs fatfs clean bindir
all: fat16 fs fatfs

fat16: $(OUTPUTDIR)/fat16.o
fs: $(OUTPUTDIR)/fs.o
fatfs: $(OUTPUTDIR)/fatfs.o

$(OUTPUTDIR)/fat16.o: $(FAT16OBJS) | bindir
	$(QUIET)$(LD) -o $@ $(addprefix ./,$(FAT16OBJS)) $(LDFLAGS) -r
	@echo "[fs  ] linked $@"

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
	$(QUIET)$(RM_F) $(OUTPUTDIR)/fat16.o $(OUTPUTDIR)/fs.o $(OUTPUTDIR)/fatfs.o
