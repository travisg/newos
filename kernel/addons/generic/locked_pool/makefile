# bus manager makefile
MY_TARGETDIR := $(KERNEL_GENERIC_ADDONS_BUILD_DIR)/locked_pool
MY_SRCDIR := $(KERNEL_GENERIC_ADDONS_DIR)/locked_pool
MY_TARGET :=  $(MY_TARGETDIR)/locked_pool
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	locked_pool.o 

MY_INCLUDES := -Iinclude
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

