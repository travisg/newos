# bus makefile
KERNEL_BUS_DIR := bus

MY_OBJS += \
	$(KERNEL_BUS_DIR)/bus_init.o \
	$(KERNEL_BUS_DIR)/bus_man.o

# include sub makefiles
include $(KERNEL_DIR)/$(KERNEL_BUS_DIR)/makefile.$(ARCH)
