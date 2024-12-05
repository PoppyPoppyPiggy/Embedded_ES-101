obj-m += led_module.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	sudo insmod led_module.ko

remove:
	sudo rmmod led_module

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

