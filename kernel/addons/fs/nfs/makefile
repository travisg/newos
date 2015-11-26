# fs makefile
MY_SRCDIR := $(GET_LOCAL_DIR)
MY_TARGETDIR := $(call TOBUILDDIR, $(MY_SRCDIR))
MY_TARGET :=  $(MY_TARGETDIR)/nfs

MY_SRCS := \
	nfs.c \
	nfs_xdr.c \
	rpc.c

include templates/kernel_addons.mk
