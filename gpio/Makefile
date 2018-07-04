
PWD ?= $(shell pwd)

# host build environment

ifeq ($(TARGET), host)

include $(PWD)/host.make

endif

# cross build environment

ifeq ($(TARGET), cross)

include $(PWD)/cross.make

endif

