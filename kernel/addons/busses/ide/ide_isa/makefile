# bus manager makefile
MY_TARGETDIR := $(KERNEL_BUSSES_IDE_ADDONS_BUILD_DIR)/ide_isa
MY_SRCDIR := $(KERNEL_BUSSES_IDE_ADDONS_DIR)/ide_isa
MY_TARGET :=  $(MY_TARGETDIR)/ide_isa
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	ide_isa.o

MY_INCLUDES := -Iinclude
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

