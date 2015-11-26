# app makefile
MY_SRCDIR := $(GET_LOCAL_DIR)
MY_TARGET := $(call TOBUILDDIR,$(MY_SRCDIR)/rld.so)

MY_SRCS := \
	rld0.c \
	rld.c \
	rldelf.c \
	rldunix.c \
	rldbeos.c \
	rldheap.c \
	rldaux.c

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := $(LIBC_STATIC) $(LIBNEWOS_STATIC)
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(MY_SRCDIR)/arch/$(ARCH)/rld.ld

include templates/simple_object.mk

