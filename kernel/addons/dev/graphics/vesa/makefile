# dev i386 vesa makefile
MY_TARGETDIR := $(KERNEL_DEV_ARCH_ADDONS_BUILD_DIR)/vesa
MY_SRCDIR := $(KERNEL_DEV_ARCH_ADDONS_DIR)/vesa
MY_TARGET := $(MY_TARGETDIR)/vesa
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	vesa.o

MY_INCLUDES := $(STDINCLUDES)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif
