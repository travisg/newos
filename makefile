ifeq ($(ARCH), )
ARCH = i386
#ARCH = sparc
#ARCH = sh4
#ARCH = alpha
endif

HOST_CC = gcc
HOST_LD = ld
HOST_AS = as
HOST_AR = ar

BOOTMAKER = tools/bootmaker
NETBOOT = tools/netboot

ifeq ($(ARCH),i386)
	CC = gcc
	LD = ld
	AS = as
	AR = ar
	GLOBAL_CFLAGS = -fno-pic
	GLOBAL_LDFLAGS = 
	LIBGCC = -lgcc
	LIBGCC_PATH = libgcc/$(ARCH)
endif

ifeq ($(ARCH),sh4)
	CC = sh-elf-gcc
	LD = sh-elf-ld
	AS = sh-elf-as
	AR = sh-elf-ar
	GLOBAL_CFLAGS = -ml -m4 -mhitachi
	GLOBAL_LDFLAGS = -EL
	LIBGCC = -lgcc
	LIBGCC_PATH = libgcc/$(ARCH)/ml/m4-single-only
endif
	
ifeq ($(ARCH),sparc)
	CC =
	LD =
	AS =
	AR =
	GLOBAL_CFLAGS =
	GLOBAL_LDFLAGS =
	LIBGCC =
	LIBGCC_PATH =
endif
	
ifeq ($(ARCH),alpha)
	CC = gcc
	LD = ld
	AS = as
	AR = ar
	GLOBAL_CFLAGS =
	GLOBAL_LDFLAGS =
	LIBGCC = -lgcc
	LIBGCC_PATH = libgcc/$(ARCH)
endif

GLOBAL_CFLAGS += -Wall -W -Werror -Wno-multichar -nostdinc -O -fno-builtin -DARCH_$(ARCH)

FINAL = boot/final

final: $(FINAL)

include lib/lib.mk
include dev/dev.mk
include kernel/kernel.mk
include boot/$(ARCH)/stage2.mk

$(FINAL): $(KERNEL) $(STAGE2) tools
ifeq ($(ARCH),sparc)
	$(BOOTMAKER) boot/config.ini -o $(FINAL) --sparc
else
	$(BOOTMAKER) boot/config.ini -o $(FINAL)
endif
	ln -sf $(FINAL) final
	ln -sf $(KERNEL) system

floppy: floppy1

ifeq ($(ARCH),i386)
floppy1: $(KERNEL) $(STAGE2) tools
	$(BOOTMAKER) boot/config.ini --floppy -o $(FINAL)
	ln -sf $(FINAL) final
	ln -sf $(KERNEL) system
endif

disk: floppy
	dd if=$(FINAL) of=/dev/disk/floppy/raw bs=18k

tools: $(NETBOOT) $(BOOTMAKER)

$(BOOTMAKER): $(BOOTMAKER).c
	$(HOST_CC) -O3 -o $@ $(BOOTMAKER).c
	
$(NETBOOT): $(NETBOOT).c
ifeq ($(OSTYPE),beos)
	$(HOST_CC) -O3 -o $@ $(NETBOOT).c -lsocket -lnet
else
	$(HOST_CC) -O3 -o $@ $(NETBOOT).c
endif

toolsclean:
	rm -f $(BOOTMAKER) $(NETBOOT)

bootclean: stage2clean
	rm -f $(STAGE2)

clean: libclean kernelclean bootclean devclean
	rm -f $(KERNEL) $(FINAL)

depsclean:
	rm -f $(KERNEL_DEPS) $(LIB_DEPS) $(STAGE2_DEPS)
	
allclean: depsclean clean toolsclean
	rm -f `find . -type f -name '*.d'`
	rm -f `find . -type f -name '*.o'`
