# bus manager makefile
MY_SRCDIR := $(GET_LOCAL_DIR)
MY_TARGETDIR := $(call TOBUILDDIR, $(MY_SRCDIR))
MY_TARGET :=  $(MY_TARGETDIR)/scsi
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	async.c \
	blocking.c \
	bus_interface.c \
	bus_mgr.c \
	ccb_mgr.c \
	device_mgr.c \
	dpc.c \
	periph_interface.c \
	periph_mgr.c \
	queuing.c \
	scsi_lock.c \
	sg_mgr.c \
	xpt.c \
	xpt_io.c 

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

