MY_SRCDIR := $(GET_LOCAL_DIR)
MY_TARGETDIR := $(call TOBUILDDIR, $(MY_SRCDIR))
MY_TARGET :=  $(MY_TARGETDIR)/scsi_periph
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	block.c \
	device_mgr.c \
	driver_mgr.c \
	err_handling.c \
	handle_mgr.c \
	removable.c \
	scsi_periph.c

include templates/kernel_addons.mk

endif

