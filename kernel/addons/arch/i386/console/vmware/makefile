# console/vmware makefile
MY_TARGETDIR := $(KERNEL_CONSOLE_ADDONS_BUILD_DIR)/vmware
MY_SRCDIR := $(KERNEL_CONSOLE_ADDONS_DIR)/vmware
MY_TARGET :=  $(MY_TARGETDIR)/vmware
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	console.o

UDES := -I$(MY_SRCDIR)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

