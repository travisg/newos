include $(DEV_ARCH_DIR)/console/console.mk
include $(DEV_ARCH_DIR)/keyboard/keyboard.mk
include $(DEV_ARCH_DIR)/rtl8139/rtl8139.mk
include $(DEV_ARCH_DIR)/ps2mouse/ps2mouse.mk

# Multiple IDE drivers, uncomment the one you want here
#include $(DEV_ARCH_DIR)/ide/ide.mk
include $(DEV_ARCH_DIR)/ide3/ide.mk
