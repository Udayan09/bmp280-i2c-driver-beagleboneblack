obj-m := bmp280-udayan.o

ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-
KERN_DIR=/home/udayan/BBBWorkspace/ldd/source/linux-6.19/KERNEL
HOST_KERN_DIR= /lib/modules/$(shell uname -r)/build/
PWD := $(shell pwd)

all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) modules

send_module:
	scp bmp280-udayan.ko udayan@192.168.7.2:~	

clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) clean
		
help:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) help
	$(MAKE) -C $(HOST_KERN_DIR) M=$(PWD) clean
		
host:	
	$(MAKE) -C $(HOST_KERN_DIR) M=$(PWD) modules

dev_overlay:
	dtc -@ -I dts -O dtb -o bmp280.dtbo bmp280.dts
	scp bmp280.dtbo udayan@192.168.7.2:~
