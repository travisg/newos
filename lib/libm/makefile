MY_TARGET := $(LIBS_BUILD_DIR)/libm.so
MY_STATIC_TARGET := $(LIBS_BUILD_DIR)/libm.a
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_TARGETDIR := $(LIBS_BUILD_DIR)/libm
MY_SRCDIR := $(LIBS_DIR)/libm
MY_OBJS :=

MY_INCLUDES := $(STDINCLUDE) -I$(MY_SRCDIR)/common_source
MY_CFLAGS := $(USER_CFLAGS)
MY_LINKSCRIPT := $(LIBS_LDSCRIPT)

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(MY_SRCDIR)/, \
	common \
	common_source \
	ieee \
	arch/$(ARCH) \
))

include templates/lib.mk

endif

