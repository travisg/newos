KERNEL_ADDONS_ARCH_DIR := $(KERNEL_ADDONS_DIR)/arch/$(ARCH)
KERNEL_ADDONS_ARCH_BUILD_DIR := $(KERNEL_ADDONS_BUILD_DIR)

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_ADDONS_ARCH_DIR)/, \
	console \
))
