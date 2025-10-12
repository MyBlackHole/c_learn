obj-m += IS_ERR_OR_NULL.o
obj-m += EXPORT_SYMBOL.o
obj-m += EXPORT_SYMBOL_test.o

CURRENT_PATH:=$(shell pwd)
LINUX_KERNEL:=$(shell uname -r)

ifneq ($(KDIR),)
	LINUX_KERNEL_PATH:=$(KDIR)
else
	LINUX_KERNEL_PATH:=/lib/modules/$(shell uname -r)/build/
endif

all:
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules

clean:
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean
