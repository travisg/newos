ifeq ($(ARCH), )
ARCH = $(HOSTTYPE)
#ARCH = i386
#ARCH = sparc
#ARCH = sh4
#ARCH = alpha
#ARCH = sparc64
endif

HOST_CC = gcc
HOST_LD = ld
HOST_AS = as
HOST_AR = ar
HOST_OBJCOPY = objcopy

# setup echo
ECHO = echo
ifeq ($(shell uname),SunOS)
	ECHO = /usr/ucb/echo
endif

BOOTMAKER = tools/bootmaker
NETBOOT = tools/netboot

CC = $(HOST_CC)
LD = $(HOST_LD)
AS = $(HOST_AS)
AR = $(HOST_AR)

ifeq ($(ARCH),i386)
	ifneq ($(HOSTTYPE),i386)
		CC = i386-elf-gcc
		LD = i386-elf-ld
		AS = i386-elf-as
		AR = i386-elf-ar
	endif
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
	
ifeq ($(ARCH),sparc64)
	CC = sparc64-elf-gcc
	LD = sparc64-elf-ld
	AS = sparc64-elf-as
	AR = sparc64-elf-ar
	GLOBAL_CFLAGS =
	GLOBAL_LDFLAGS =
	LIBGCC = -lgcc
	LIBGCC_PATH = lib/libgcc/$(ARCH)
endif
	
ifeq ($(ARCH),sparc)
	ifneq ($(HOSTTYPE),sparc)
		CC = sparc-elf-gcc
		LD = sparc-elf-ld
		AS = sparc-elf-as
		AR = sparc-elf-ar
	endif
	GLOBAL_CFLAGS =
	GLOBAL_LDFLAGS =
	LIBGCC = -lgcc
	LIBGCC_PATH = lib/libgcc/$(ARCH)
endif
	
ifeq ($(ARCH),alpha)
	ifneq ($(HOSTTYPE),alpha)
		CC = alpha-elf-gcc
		LD = alpha-elf-ld
		AS = alpha-elf-as
		AR = alpha-elf-ar
	endif
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
	rm -f final.$(ARCH);ln -sf $(FINAL) final.$(ARCH)
	rm -f system.$(ARCH);ln -sf $(KERNEL) system.$(ARCH)

ifeq ($(ARCH),i386)
floppy: floppy1

floppy1: $(KERNEL) $(STAGE2) $(APPS) tools
	$(BOOTMAKER) boot/$(ARCH)/config.ini --floppy -o $(FINAL)
	rm -f final.$(ARCH);ln -sf $(FINAL) final.$(ARCH)
	rm -f system.$(ARCH);ln -sf $(KERNEL) system.$(ARCH)

disk: floppy
	dd if=$(FINAL) of=/dev/disk/floppy/raw bs=18k
endif

tools: $(NETBOOT) $(BOOTMAKER)

$(BOOTMAKER): $(BOOTMAKER).c tools/sh4bootblock.h tools/sparcbootblock.h
	$(HOST_CC) -O3 -o $@ $(BOOTMAKER).c
	
NETBOOT_LINK_ARGS =
ifeq ($(OSTYPE),beos)
	NETBOOT_LINK_ARGS = -lsocket -lnet
endif
ifeq ($(shell uname),SunOS)
	NETBOOT_LINK_ARGS = -lsocket -lnsl
endif

$(NETBOOT): $(NETBOOT).c
	$(HOST_CC) -O3 -o $@ $(NETBOOT).c $(NETBOOT_LINK_ARGS)

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
