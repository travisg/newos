# bus manager makefile
MY_TARGETDIR := $(KERNEL_GENERIC_ADDONS_BUILD_DIR)/scsi_periph
MY_SRCDIR := $(KERNEL_GENERIC_ADDONS_DIR)/scsi_periph
MY_TARGET :=  $(MY_TARGETDIR)/scsi_periph
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	block.o \
	device_mgr.o \
	driver_mgr.o \
	err_handling.o \
	handle_mgr.o \
	removable.o \
	scsi_periph.o

MY_INCLUDES := -Iinclude
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

