# fs makefile
MY_SRCDIR := $(GET_LOCAL_DIR)
MY_TARGETDIR := $(call TOBUILDDIR, $(MY_SRCDIR))
MY_TARGET :=  $(MY_TARGETDIR)/zfs
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	zfs.c \
	zfs_dir.c \
	zfs_file.c \
	zfs_vnode.c

include templates/kernel_addons.mk

endif

