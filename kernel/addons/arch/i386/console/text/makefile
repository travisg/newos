# arch/i386/console/text makefile
MY_TARGETDIR := $(KERNEL_CONSOLE_ADDONS_BUILD_DIR)/text
MY_SRCDIR := $(KERNEL_CONSOLE_ADDONS_DIR)/text
MY_TARGET :=  $(MY_TARGETDIR)/text
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	console.o

MY_INCLUDES := -Iinclude
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

