# main libc
MY_TARGET := $(LIBS_BUILD_DIR)/libc/libc.so
MY_STATIC_TARGET := $(LIBS_BUILD_DIR)/libc/libc.a

LIBC_EXTRA_CFLAGS := -D__STDC_VERSION__=199901

MY_TARGETDIR := $(LIBS_BUILD_DIR)/libc
MY_SRCDIR := $(LIBS_DIR)/libc
MY_OBJS := \
	libc_init.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(LIBC_EXTRA_CFLAGS)
MY_LINKSCRIPT := $(LIBS_LDSCRIPT)

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(MY_SRCDIR)/, \
	hoard \
	locale \
	stdio \
	stdlib \
	string \
	unistd \
	time \
))

include templates/lib.mk

# kernel libc
MY_TARGET := $(LIBS_BUILD_DIR)/klibc/klibc.o

MY_TARGETDIR := $(LIBS_BUILD_DIR)/klibc
MY_SRCDIR := $(LIBS_DIR)/libc
MY_OBJS :=

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(KERNEL_CFLAGS) $(LIBC_EXTRA_CFLAGS)
MY_LINKSCRIPT := $(LIBS_LDSCRIPT)

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(MY_SRCDIR)/, \
	locale \
	stdio \
	stdlib \
	string \
))

include templates/combined_object.mk


