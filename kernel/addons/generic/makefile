KERNEL_GENERIC_ADDONS_DIR := $(KERNEL_ADDONS_DIR)/generic
KERNEL_GENERIC_ADDONS_BUILD_DIR := $(KERNEL_ADDONS_BUILD_DIR)/generic

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_GENERIC_ADDONS_DIR)/, \
	locked_pool \
	scsi_periph \
	blkman \
))
