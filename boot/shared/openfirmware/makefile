# this file handles building openfirmware

MY_TARGETDIR := $(call TOBUILDDIR, boot)/shared/openfirmware
MY_TARGET := $(call TOBUILDDIR, boot)/shared/openfirmware.o
MY_SRCDIR := boot/shared/openfirmware
MY_OBJS := \
	openfirmware.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(KERNEL_CFLAGS)

include templates/combined_object.mk

