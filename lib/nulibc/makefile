MY_TARGET := $(LIBS_BUILD_DIR)/libc/libc.so
MY_STATIC_TARGET := $(LIBS_BUILD_DIR)/libc/libc.a
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_TARGETDIR := $(LIBS_BUILD_DIR)/libc
MY_SRCDIR := $(LIBS_DIR)/nulibc
MY_OBJS := \
	nulibc_init.o

MY_INCLUDES := -Iinclude -Iinclude/nulibc
MY_CFLAGS := 
MY_LINKSCRIPT := $(LIBS_LDSCRIPT)

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(MY_SRCDIR)/, \
	hoard \
	locale \
	stdio \
	stdlib \
	string \
	system \
	system/arch/$(ARCH) \
	unistd \
))

include templates/lib.mk

endif

