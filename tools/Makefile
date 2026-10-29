include ../make/common.mk

BUILD_DIR := bin
# build.c and bmp.c are compiled by the root Makefile into host tools; skip them here.
SOURCES := $(filter-out build.c bmp.c,$(wildcard *.c))
OBJECTS := $(SOURCES:%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean
all: $(OBJECTS)

$(BUILD_DIR)/%.o: %.c
	$(call make_dir,$(BUILD_DIR))
	$(QUIET)$(CC) -c $< -o $@
	@echo "[tool] cc $(<F)"

clean:
	$(QUIET)$(RM_RF) $(BUILD_DIR)
