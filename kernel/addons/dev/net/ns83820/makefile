# dev rtl8139 makefile
MY_TARGETDIR := $(KERNEL_DEV_ADDONS_BUILD_DIR)/net/ns83820
MY_SRCDIR := $(KERNEL_DEV_ADDONS_DIR)/net/ns83820
MY_TARGET := $(MY_TARGETDIR)/ns83820
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	ns83820.o \
	ns83820_dev.o

MY_INCLUDES := $(STDINCLUDES)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif
