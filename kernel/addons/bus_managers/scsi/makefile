# bus manager makefile
MY_TARGETDIR := $(KERNEL_BUS_MANAGERS_ADDONS_BUILD_DIR)/scsi
MY_SRCDIR := $(KERNEL_BUS_MANAGERS_ADDONS_DIR)/scsi
MY_TARGET :=  $(MY_TARGETDIR)/scsi
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	async.o \
	blocking.o \
	bus_interface.o \
	bus_mgr.o \
	ccb_mgr.o \
	device_mgr.o \
	dpc.o \
	periph_interface.o \
	periph_mgr.o \
	queuing.o \
	scsi_lock.o \
	sg_mgr.o \
	xpt.o \
	xpt_io.o 

MY_INCLUDES := -Iinclude
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

