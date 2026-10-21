# Builds fstool, which runs the kernel's own file system objects on a disk image: bin/fstool with the old FAT16
# code, and bin/fstool-fatfs with FatFs. run.sh builds those objects first with make -C fs, then this.
ROOT := ../..
HOST_CC ?= gcc
CFLAGS := -m32 -O1 -g -Wall -no-pie -I $(ROOT)/include
FAT16 := $(ROOT)/bin/fs.o $(ROOT)/bin/fat16.o $(ROOT)/bin/fat16_iface.o
FATFS := $(ROOT)/bin/fs.o $(ROOT)/bin/fatfs.o

all: bin/fstool bin/fstool-fatfs

bin/fstool: fstool.c host.c $(FAT16)
	@mkdir -p bin
	$(HOST_CC) $(CFLAGS) fstool.c host.c $(FAT16) -o $@

bin/fstool-fatfs: fstool.c host.c $(FATFS)
	@mkdir -p bin
	$(HOST_CC) $(CFLAGS) fstool.c host.c $(FATFS) -o $@
