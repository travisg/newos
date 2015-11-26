# main libc
MY_TARGET := $(LIBS_BUILD_DIR)/libc.so
MY_STATIC_TARGET := $(LIBS_BUILD_DIR)/libc.a

#LIBC_EXTRA_COMPILEFLAGS := -D__STDC_VERSION__=199901

MY_TARGETDIR := $(LIBS_BUILD_DIR)/libc
MY_SRCDIR := $(LIBS_DIR)/libc
MY_SRCS := \
	libc_init.c

MY_INCLUDES := $(STDINCLUDE)
MY_COMPILEFLAGS := $(LIBC_EXTRA_COMPILEFLAGS)

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
MY_TARGET := $(LIBS_BUILD_DIR)/klibc.o

MY_TARGETDIR := $(LIBS_BUILD_DIR)/klibc
MY_SRCDIR := $(LIBS_DIR)/libc
MY_SRCS :=

MY_INCLUDES := $(STDINCLUDE)
MY_COMPILEFLAGS := $(KERNEL_COMPILEFLAGS)
MY_COMPILEFLAGS += $(LIBC_EXTRA_COMPILEFLAGS)
MY_LINKSCRIPT := $(LIBS_LDSCRIPT)

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(MY_SRCDIR)/, \
	locale \
	stdio \
	stdlib \
	string \
))

include templates/combined_object.mk


