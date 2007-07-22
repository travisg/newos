# dev makefile
KERNEL_DEV_DIR := dev

MY_SRCS += \
	$(KERNEL_DEV_DIR)/fixed.c \
	$(KERNEL_DEV_DIR)/beos.c

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_DIR)/$(KERNEL_DEV_DIR)/, \
	common \
	arch/$(ARCH) \
	dprint \
))
