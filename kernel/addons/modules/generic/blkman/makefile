MY_SRCDIR := $(GET_LOCAL_DIR)
MY_TARGETDIR := $(call TOBUILDDIR, $(MY_SRCDIR))
MY_TARGET :=  $(MY_TARGETDIR)/blkman
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	blkman.o 

MY_INCLUDES := -Iinclude
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

