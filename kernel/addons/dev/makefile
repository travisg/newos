KERNEL_DEV_ADDONS_DIR := $(KERNEL_ADDONS_DIR)/dev
KERNEL_DEV_ADDONS_BUILD_DIR := $(KERNEL_ADDONS_BUILD_DIR)/dev

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_DEV_ADDONS_DIR)/, \
	arch/$(ARCH) \
	console \
	disk/netblock \
	disk/scsi/scsi_dsk \
	net/ns83820 \
	net/pcnet32 \
	net/rtl8139 \
	tty \
))
