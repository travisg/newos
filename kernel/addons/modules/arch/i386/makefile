KERNEL_ADDONS_MODULES_ARCH_DIR := $(KERNEL_ADDONS_MODULES_DIR)/arch/$(ARCH)
KERNEL_ADDONS_MODULES_ARCH_BUILD_DIR := $(KERNEL_ADDONS_MODULES_BUILD_DIR)

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_ADDONS_MODULES_ARCH_DIR)/, \
	console/text \
	console/vmware \
))
