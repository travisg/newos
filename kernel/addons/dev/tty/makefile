# dev tty makefile
MY_TARGETDIR := $(KERNEL_DEV_ADDONS_BUILD_DIR)/tty
MY_SRCDIR := $(KERNEL_DEV_ADDONS_DIR)/tty
MY_TARGET :=  $(MY_TARGETDIR)/tty
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS += \
	tty.o \
	tty_master.o \
	tty_slave.o

MY_INCLUDES := $(STDINCLUDES)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif
