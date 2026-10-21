# Builds fstool, which runs the kernel's own file system objects on a disk image. run.sh builds those objects
# first with make -C fs, then this.
ROOT := ../..
HOST_CC ?= gcc
OBJS := $(ROOT)/bin/fat16.o $(ROOT)/bin/fs.o

bin/fstool: fstool.c host.c $(OBJS)
	@mkdir -p bin
	$(HOST_CC) -m32 -O1 -g -Wall -no-pie -I $(ROOT)/include fstool.c host.c $(OBJS) -o $@
