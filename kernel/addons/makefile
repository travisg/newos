KERNEL_ADDONS_INCLUDES := $(KERNEL_INCLUDES)
KERNEL_ADDONS_COMPILEFLAGS := $(KERNEL_COMPILEFLAGS)

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_ADDONS_DIR)/, \
	modules \
	fs \
	dev \
))
