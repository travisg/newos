MY_TARGET := $(LIBS_BUILD_DIR)/libm.so
MY_STATIC_TARGET := $(LIBS_BUILD_DIR)/libm.a

MY_TARGETDIR := $(LIBS_BUILD_DIR)/libm
MY_SRCDIR := $(LIBS_DIR)/libm
MY_SRCS :=

MY_INCLUDES := -I$(MY_SRCDIR)/common_source

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(MY_SRCDIR)/, \
	common \
	common_source \
	ieee \
	arch/$(ARCH) \
))

include templates/lib.mk
