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
HOST_OBJCOPY = objcopy

BOOTMAKER = tools/bootmaker
NETBOOT = tools/netboot

ifeq ($(ARCH),i386)
	CC = gcc
	LD = ld
	AS = as
	AR = ar
	GLOBAL_CFLAGS = -fno-pic -O
	GLOBAL_LDFLAGS = 
	LIBGCC = -lgcc
	LIBGCC_PATH = lib/libgcc/$(ARCH)
endif

ifeq ($(ARCH),sh4)
	CC = sh-elf-gcc
	LD = sh-elf-ld
	AS = sh-elf-as
	AR = sh-elf-ar
	OBJCOPY = sh-elf-objcopy
	GLOBAL_CFLAGS = -ml -m4 -mhitachi -O
	GLOBAL_LDFLAGS = -EL
	LIBGCC = -lgcc
	LIBGCC_PATH = lib/libgcc/$(ARCH)/ml/m4-single-only
endif
	
ifeq ($(ARCH),sparc)
	CC = sparc-elf-gcc
	LD = sparc-elf-ld
	AS = sparc-elf-as
	AR = sparc-elf-ar
	GLOBAL_CFLAGS =
	GLOBAL_LDFLAGS =
	LIBGCC = -lgcc
	LIBGCC_PATH = lib/libgcc/$(ARCH)
endif
	
ifeq ($(ARCH),alpha)
	CC = gcc
	LD = ld
	AS = as
	AR = ar
	GLOBAL_CFLAGS =
	GLOBAL_LDFLAGS =
	LIBGCC = -lgcc
	LIBGCC_PATH = lib/libgcc/$(ARCH)
endif

OBJ_DIR = obj.$(ARCH)

GLOBAL_CFLAGS += -Wall -W -Wno-multichar -Wno-unused -nostdinc -fno-builtin -DARCH_$(ARCH)

FINAL = boot/$(ARCH)/final

final: $(FINAL)

# sub makefiles are responsible for adding to these
DEPS =
CLEAN =

include lib/lib.mk
include boot/$(ARCH)/stage2.mk
include dev/dev.mk
include kernel/kernel.mk
include apps/apps.mk

BOOTMAKER_ARGS =
ifeq ($(ARCH),sparc)
BOOTMAKER_ARGS += --sparc
endif
ifeq ($(ARCH),sh4)
BOOTMAKER_ARGS += --sh4
endif

$(FINAL): $(KERNEL) $(STAGE2) $(APPS) tools
	$(BOOTMAKER) boot/$(ARCH)/config.ini -o $(FINAL) $(BOOTMAKER_ARGS)
	ln -sf $(FINAL) final.$(ARCH)
	ln -sf $(KERNEL) system.$(ARCH)

ifeq ($(ARCH),i386)
floppy: floppy1

floppy1: $(KERNEL) $(STAGE2) $(APPS) tools
	$(BOOTMAKER) boot/$(ARCH)/config.ini --floppy -o $(FINAL)
	ln -sf $(FINAL) final.$(ARCH)
	ln -sf $(KERNEL) system.$(ARCH)

disk: floppy
	dd if=$(FINAL) of=/dev/disk/floppy/raw bs=18k
endif

tools: $(NETBOOT) $(BOOTMAKER)

$(BOOTMAKER): $(BOOTMAKER).c tools/sh4bootblock.h tools/sparcbootblock.h
	$(HOST_CC) -O3 -o $@ $(BOOTMAKER).c
	
$(NETBOOT): $(NETBOOT).c
ifeq ($(OSTYPE),beos)
	$(HOST_CC) -O3 -o $@ $(NETBOOT).c -lsocket -lnet
else
	$(HOST_CC) -O3 -o $@ $(NETBOOT).c
endif

toolsclean:
	rm -f $(BOOTMAKER) $(NETBOOT) $(NETBOOT_DC)

bootclean: stage2clean
	rm -f $(STAGE2)

CLEAN += toolsclean bootclean

clean: $(CLEAN)
	rm -f $(KERNEL) $(FINAL)
	rm -f final.$(ARCH) system.$(ARCH)

depsclean:
	rm -f $(DEPS)
	
allclean: depsclean clean toolsclean
	rm -f `find . -type f -name '*.d'`
	rm -f `find . -type f -name '*.o'`

include $(DEPS)
