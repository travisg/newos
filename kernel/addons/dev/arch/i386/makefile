KERNEL_DEV_ARCH_ADDONS_DIR := $(KERNEL_DEV_ADDONS_DIR)/arch/$(ARCH)
KERNEL_DEV_ARCH_ADDONS_BUILD_DIR := $(KERNEL_DEV_ADDONS_BUILD_DIR)/arch/$(ARCH)

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_DEV_ARCH_ADDONS_DIR)/, \
	floppy \
	ide \
	ide2 \
	ide3 \
	keyboard \
	ps2mouse \
	rtl8139 \
	vesa \
))
