
PWD ?= $(shell pwd)

# host build environment

ifeq ($(TARGET), host)

include $(PWD)/host.mk

endif

# cross build environment

ifeq ($(TARGET), cross)

include $(PWD)/cross.mk

endif

