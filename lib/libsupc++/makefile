MY_TARGET := $(LIBS_BUILD_DIR)/libsupc++/libsupc++.so
MY_STATIC_TARGET := $(LIBS_BUILD_DIR)/libsupc++/libsupc++.a

MY_TARGETDIR := $(LIBS_BUILD_DIR)/libsupc++
MY_SRCDIR := $(LIBS_DIR)/libsupc++
MY_OBJS := tinfo.o \
	   tinfo2.o \
	   eh_alloc.o \
	   eh_aux_runtime.o \
	   eh_catch.o \
	   eh_exception.o \
	   eh_globals.o \
	   eh_personality.o \
	   eh_terminate.o \
	   eh_throw.o \
	   pure.o \

MY_INCLUDES := $(STDINCLUDE) -I$(MY_SRCDIR) -I$(TOOLCHAIN_PATH)/include/g++-v3/$(ARCH)
MY_CPPFLAGS = $(CFLAGS)
MY_LINKSCRIPT := $(LIBS_LDSCRIPT)

# include sub makefiles
#include $(addsuffix /makefile, $(addprefix $(MY_SRCDIR)/, \
#))

include templates/lib.mk

