KERNEL_ADDONS_MODULES_DIR := $(KERNEL_ADDONS_DIR)/modules
KERNEL_ADDONS_MODULES_BUILD_DIR := $(KERNEL_ADDONS_BUILD_DIR)/modules

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_ADDONS_MODULES_DIR)/, \
	generic/blkman \
	generic/locked_pool \
	generic/scsi_periph \
	bus_managers/isa \
	bus_managers/pci \
	bus_managers/ide \
	busses/ide/ide_isa \
	bus_managers/scsi \
	bus_managers/usb \
	busses/usb/ohci \
	arch/$(ARCH) \
))
