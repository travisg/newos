# bus manager makefile
MY_TARGETDIR := $(KERNEL_ADDONS_MODULES_BUILD_DIR)/bus_managers/ide
MY_SRCDIR := $(KERNEL_ADDONS_MODULES_DIR)/bus_managers/ide
MY_TARGET :=  $(MY_TARGETDIR)/ide
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	ata.o \
	atapi.o \
	basic_prot.o \
	channel_mgr.o \
	device_mgr.o \
	ide.o \
	ide_sim.o \
	DMA.o \
	PIO.o \
	queuing.o \
	scsi2ata.o \
	scsi2atapi.o \
	sync.o 

MY_INCLUDES := -Iinclude
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

