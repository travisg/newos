# dev i386 pcnet32 makefile
MY_TARGETDIR := $(KERNEL_DEV_ARCH_ADDONS_BUILD_DIR)/pcnet32
MY_SRCDIR := $(KERNEL_DEV_ARCH_ADDONS_DIR)/pcnet32
MY_TARGET := $(MY_TARGETDIR)/pcnet32
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	pcnet32.o \
	pcnet32_dev.o

MY_INCLUDES := $(STDINCLUDES)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif
