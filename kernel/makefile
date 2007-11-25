# main kernel make file
MY_TARGET := $(KERNEL)
MY_TARGETLIB := $(LIBKERNEL)
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

LINKHACK = $(KERNEL_BUILD_DIR)/linkhack

MY_TARGETDIR := $(KERNEL_BUILD_DIR)
MY_SRCDIR := $(KERNEL_DIR)
MY_SRCS := \
	main.c \
	elf.c \
	faults.c \
	heap.c \
	int.c \
	console.c \
	debug.c \
	gdb.c \
	dev.c \
	timer.c \
	time.c \
	port.c \
	sem.c \
	signal.c \
	smp.c \
	syscalls.c \
	thread.c \
	cbuf.c \
	cpu.c \
	vfs.c \
	module.c

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

