# bus manager makefile
MY_TARGETDIR := $(KERNEL_ADDONS_MODULES_BUILD_DIR)/bus_managers/usb
MY_SRCDIR := $(KERNEL_ADDONS_MODULES_DIR)/bus_managers/usb
MY_TARGET :=  $(MY_TARGETDIR)/usb
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	usb.o \
	usb_enum.o

MY_INCLUDES := -Iinclude
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

