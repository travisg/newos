KERNEL_CONSOLE_ADDONS_DIR := $(KERNEL_ADDONS_ARCH_DIR)/console
KERNEL_CONSOLE_ADDONS_BUILD_DIR := $(KERNEL_ADDONS_ARCH_BUILD_DIR)/console

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_CONSOLE_ADDONS_DIR)/, \
	text \
	vmware \
))
