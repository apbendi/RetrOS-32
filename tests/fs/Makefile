# Builds fstool, which runs the kernel's own file system objects on a disk image: bin/fs.o, the file layer, and
# bin/fatfs.o, FatFs. run.sh builds those objects first with make -C fs, then this.
ROOT := ../..
HOST_CC ?= gcc
CFLAGS := -m32 -O1 -g -Wall -no-pie -I $(ROOT)/include
OBJS := $(ROOT)/bin/fs.o $(ROOT)/bin/fatfs.o

bin/fstool: fstool.c host.c $(OBJS)
	@mkdir -p bin
	$(HOST_CC) $(CFLAGS) fstool.c host.c $(OBJS) -o $@
