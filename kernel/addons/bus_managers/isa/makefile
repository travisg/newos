# bus manager makefile
MY_TARGETDIR := $(KERNEL_BUS_MANAGERS_ADDONS_BUILD_DIR)/isa
MY_SRCDIR := $(KERNEL_BUS_MANAGERS_ADDONS_DIR)/isa
MY_TARGET :=  $(MY_TARGETDIR)/isa
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	isa.o

MY_INCLUDES := -Iinclude
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

