# fs makefile
MY_TARGETDIR := $(KERNEL_FS_ADDONS_BUILD_DIR)/zfs
MY_SRCDIR := $(KERNEL_FS_ADDONS_DIR)/zfs
MY_TARGET :=  $(MY_TARGETDIR)/zfs
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	zfs.o \
	zfs_dir.o \
	zfs_file.o \
	zfs_vnode.o

MY_INCLUDES := -Iinclude
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LIBKERNEL)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(KERNEL_ADDONS_DIR)/ldscripts/$(ARCH)/addon.ld

include templates/kernel_addons.mk

endif

