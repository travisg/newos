# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(GET_LOCAL_DIR)/, \
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
	console/vga_text \
	console/vmware \
	arch/$(ARCH) \
))
