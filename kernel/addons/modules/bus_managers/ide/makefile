# bus manager makefile
MY_SRCDIR := $(GET_LOCAL_DIR)
MY_TARGETDIR := $(call TOBUILDDIR, $(MY_SRCDIR))
MY_TARGET :=  $(MY_TARGETDIR)/ide

MY_SRCS := \
	ata.c \
	atapi.c \
	basic_prot.c \
	channel_mgr.c \
	device_mgr.c \
	ide.c \
	ide_sim.c \
	DMA.c \
	PIO.c \
	queuing.c \
	scsi2ata.c \
	scsi2atapi.c \
	sync.c 

include templates/kernel_addons.mk
