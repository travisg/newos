DEV_OBJS += \
	$(DEV_ARCH_DIR)/console/console.o \
	$(DEV_ARCH_DIR)/console/keyboard.o \
	$(DEV_ARCH_DIR)/pci/pci.o

DEV_ARCH_INCLUDES += \
	-I$(DEV_ARCH_DIR)/console \
	-I$(DEV_ARCH_DIR)/pci
