KERNEL_ADDONS_INCLUDES := $(KERNEL_INCLUDES)
KERNEL_ADDONS_CFLAGS := $(KERNEL_CFLAGS)

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_ADDONS_DIR)/, \
	modules \
	fs \
	dev \
))
