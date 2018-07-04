
# module name
MODULE=test

# If defined `KERNELRELEASE` , we've been invoked from kernel build.
# In this case , the module will built into kernel and load when kernel startup.
# Just append the object files of module
ifneq ($(KERNELRELEASE),)
	obj-m := $(MODULE).o
	$(MODULE)-objs := hello.o 

# else call module build process in target build system.
# In this case , the module will built to seperated module that should load 
# independently.
else
	KERNELDIR ?= $(HOME)/Workspace/raspberrypi/linux
	PWD ?= $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

endif


