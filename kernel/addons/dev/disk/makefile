KERNEL_DEV_DISK_ADDONS_DIR := $(KERNEL_DEV_ADDONS_DIR)/disk
KERNEL_DEV_DISK_ADDONS_BUILD_DIR := $(KERNEL_DEV_ADDONS_BUILD_DIR)/disk

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_DEV_DISK_ADDONS_DIR)/, \
	netblock \
	scsi \
))
