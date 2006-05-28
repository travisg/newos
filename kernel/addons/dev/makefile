KERNEL_DEV_ADDONS_DIR := $(KERNEL_ADDONS_DIR)/dev
KERNEL_DEV_ADDONS_BUILD_DIR := $(KERNEL_ADDONS_BUILD_DIR)/dev

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_DEV_ADDONS_DIR)/, \
	console \
	disk/netblock \
	disk/scsi/scsi_dsk \
	disk/floppy \
	graphics/vesa \
	input/pckeyboard \
	input/ps2mouse \
	net/ns83820 \
	net/pcnet32 \
	net/rhine \
	net/rtl8139 \
	tty \
))
