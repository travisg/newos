# dev makefile
KERNEL_DEV_DIR := dev

MY_OBJS += \
	$(KERNEL_DEV_DIR)/fixed.o \
	$(KERNEL_DEV_DIR)/beos.o

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_DIR)/$(KERNEL_DEV_DIR)/, \
	common \
	arch/$(ARCH) \
	console \
	tty \
	dprint \
))
