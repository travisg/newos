# dev tty makefile
MY_SRCDIR := $(GET_LOCAL_DIR)
MY_TARGETDIR := $(call TOBUILDDIR, $(MY_SRCDIR))
MY_TARGET :=  $(MY_TARGETDIR)/tty

MY_SRCS += \
	tty.c \
	tty_master.c \
	tty_slave.c

include templates/kernel_addons.mk
