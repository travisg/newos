MY_SRCDIR := $(GET_LOCAL_DIR)
MY_TARGETDIR := $(call TOBUILDDIR, $(MY_SRCDIR))
MY_TARGET := $(MY_TARGETDIR)/ide2
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	ata_ide_drive.o \
	atapi_ide_drive.o \
	ide.o \
	ide_bus.o \
	ide_drive.o \
	ide_raw.o \
	isa_ide_bus.o \
	pci_ide_bus.o

MY_INCLUDES := $(STDINCLUDES)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif
