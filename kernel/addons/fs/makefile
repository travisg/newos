KERNEL_FS_ADDONS_DIR := $(KERNEL_ADDONS_DIR)/fs
KERNEL_FS_ADDONS_BUILD_DIR := $(KERNEL_ADDONS_BUILD_DIR)/fs

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_FS_ADDONS_DIR)/, \
	fat \
	iso9660 \
	nfs \
	zfs \
))
