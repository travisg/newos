# main kernel make file
MY_TARGET := $(KERNEL)
MY_TARGETLIB := $(LIBKERNEL)
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

LINKHACK = $(KERNEL_BUILD_DIR)/linkhack

MY_TARGETDIR := $(KERNEL_BUILD_DIR)
MY_SRCDIR := $(KERNEL_DIR)
MY_OBJS := \
	main.o \
	elf.o \
	faults.o \
	heap.o \
	int.o \
	console.o \
	debug.o \
	gdb.o \
	dev.o \
	timer.o \
	port.o \
	sem.o \
	smp.o \
	syscalls.o \
	thread.o \
	cbuf.o \
	cpu.o \
	vfs.o \
	module.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LIBS := $(LINKHACK).so $(KLIBS)
MY_LINKSCRIPT := $(KERNEL_DIR)/arch/$(ARCH)/kernel.ld

# include sub makefiles
include $(addsuffix /makefile, $(addprefix $(KERNEL_DIR)/, \
	fs \
	vm \
	net \
	arch \
	dev \
	util \
))

include templates/kernel.mk

# Build a shared object with nothing in it to link against the kernel.
# Otherwise the linker optimizes and removes the dynamic section.
# This allows us to build a kernel with a dynamic section but not relocatable.
# Stolen from the FreeBSD sys makefile.
$(LINKHACK).o: $(KERNEL_DIR)/linkhack.c
	$(CC) $(GLOBAL_CFLAGS) -c $< -o $@

$(LINKHACK).so: $(LINKHACK).o
	$(LD) $(GLOBAL_LDFLAGS) --shared -Bdynamic $< -o $@

endif

