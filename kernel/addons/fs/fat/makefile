# fs makefile
MY_SRCDIR := $(GET_LOCAL_DIR)
MY_TARGETDIR := $(call TOBUILDDIR, $(MY_SRCDIR))
MY_TARGET :=  $(MY_TARGETDIR)/fat

MY_SRCS := \
	fat.c \
	fat_dir.c \
	fat_file.c \
	fat_vnode.c

include templates/kernel_addons.mk
