MY_TARGETDIR := $(KERNEL_DEV_ADDONS_BUILD_DIR)/disk/scsi/scsi_dsk
MY_SRCDIR := $(KERNEL_DEV_ADDONS_DIR)/disk/scsi/scsi_dsk
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
