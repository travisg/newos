KERNEL_ADDONS_MODULES_DIR := $(KERNEL_ADDONS_DIR)/modules
KERNEL_ADDONS_MODULES_BUILD_DIR := $(KERNEL_ADDONS_BUILD_DIR)/modules

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_ADDONS_MODULES_DIR)/, \
	generic/blkman \
	generic/locked_pool \
	generic/scsi_periph \
	bus_managers/ide \
	bus_managers/isa \
	bus_managers/scsi \
	bus_managers/pci \
	busses/ide/ide_isa \
	arch/$(ARCH) \
))
