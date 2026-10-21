include ../make/common.mk

BUILD_DIR := bin
INCLUDE_DIR := ../include

TEST_INCLUDES := -I $(INCLUDE_DIR) -I ./include/
COMMON_FLAGS := $(ARCH_CFLAGS) -O2 -Wall --no-builtin
MOCK := utils/mocks.c utils/test.c


TESTS := pcb_test mem_test net_test http_test

.PHONY: all $(TESTS) run clean bin
all: $(TESTS) run

bin:
	$(call make_dir,$(BUILD_DIR))

mem_test: bin mem_test.c
	$(QUIET)$(CC) mem_test.c -D__MEM_TEST ../bin/bitmap.o ../bin/kmem.o -D__RetrOS32MOCK $(MOCK) $(TEST_INCLUDES) $(COMMON_FLAGS) -o $(BUILD_DIR)/mem_test.o

pcb_test: bin pcb_test.c
	$(QUIET)$(CC) pcb_test.c ../bin/bitmap.o ../bin/pcb_queue.o -D__RetrOS32MOCK $(MOCK) $(TEST_INCLUDES) $(COMMON_FLAGS) -o $(BUILD_DIR)/pcb_test.o

net_test: bin net_test.c
	$(QUIET)$(CC) net_test.c ../bin/rbuffer.o -D__RetrOS32MOCK $(MOCK) $(TEST_INCLUDES) $(COMMON_FLAGS) -D__KERNEL -o $(BUILD_DIR)/net_test.o

http_test: bin http_test.c
	$(QUIET)$(CC) http_test.c ../lib/http.c -D__RetrOS32MOCK $(MOCK) $(TEST_INCLUDES) $(COMMON_FLAGS) -o $(BUILD_DIR)/http_test.o

run:
	$(BUILD_DIR)/mem_test.o
	$(BUILD_DIR)/pcb_test.o
	$(BUILD_DIR)/net_test.o
	$(BUILD_DIR)/http_test.o

clean:
	$(QUIET)$(RM_RF) $(BUILD_DIR)
	$(QUIET)$(RM_F) filesystem.test
