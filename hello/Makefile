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
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD ?= $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

endif

