# bus manager makefile
MY_TARGETDIR := $(KERNEL_DEV_DISK_SCSI_ADDONS_BUILD_DIR)/scsi_dsk
MY_SRCDIR := $(KERNEL_DEV_DISK_SCSI_ADDONS_DIR)/scsi_dsk
MY_TARGET :=  $(MY_TARGETDIR)/scsi_dsk
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	scsi_dsk.o 

MY_INCLUDES := -Iinclude
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

