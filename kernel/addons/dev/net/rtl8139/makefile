# dev rtl8139 makefile
MY_TARGETDIR := $(KERNEL_DEV_ADDONS_BUILD_DIR)/net/rtl8139
MY_SRCDIR := $(KERNEL_DEV_ADDONS_DIR)/net/rtl8139
MY_TARGET := $(MY_TARGETDIR)/rtl8139
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	rtl8139.o \
	rtl8139_dev.o

MY_INCLUDES := $(STDINCLUDES)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif
