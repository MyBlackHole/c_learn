# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile - hook_write kernel module
#
# Build:
#   make          # builds against running kernel
#   make KDIR=/path/to/kernel/source  # custom kernel tree
#
# The module targets Linux 7.0+ x86_64 using the FlipSwitch
# syscall dispatch patching technique.

obj-m := hook_write.o

hook_write-y := \
	src/flipswitch_core.o \
	src/hook_core.o \
	src/hook_write.o \
	src/path_resolve.o \
	src/relay_chan.o \
	src/control.o \
	src/security.o

# Include path for headers in include/
ccflags-y := -I$(src)/include

# Enable debug prints (pr_debug) - comment out for production
ccflags-y += -DDEBUG

# Prevent the compiler from merging duplicate strings; useful for
# debugging symbol names in kallsyms callbacks.
ccflags-y += -fno-merge-constants

# Default kernel build directory (running kernel)
KDIR  ?= /lib/modules/$(shell uname -r)/build
PWD   := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Install the module (requires root)
install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

.PHONY: all clean install
