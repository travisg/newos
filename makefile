ifeq ($(HOSTTYPE),i586)
	HOSTTYPE = i386
endif
ifeq ($(HOSTTYPE),i686)
	HOSTTYPE = i386
endif
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
BIN2H = tools/bin2h
BIN2ASM = tools/bin2asm

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
		OBJCOPY = i386-elf-objcopy
	endif
	ifeq ($(OSTYPE),cygwin)
		CC = i386-linux-gcc
		LD = i386-linux-ld
		AS = i386-linux-as
		AR = i386-linux-ar
		OBJCOPY = i386-linux-objcopy
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
	OBJCOPY = sparc64-elf-objcopy
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
		OBJCOPY = sparc-elf-objcopy
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
		OBJCOPY = alpha-elf-objcopy
	endif
	GLOBAL_CFLAGS =
	GLOBAL_LDFLAGS =
	LIBGCC = -lgcc
	LIBGCC_PATH = lib/libgcc/$(ARCH)
endif

ifeq ($(ARCH),mips)
	ifneq ($(HOSTTYPE),mips)
		CC = mips-elf-gcc
		LD = mips-elf-ld
		AS = mips-elf-as
		AR = mips-elf-ar
		OBJCOPY = mips-elf-objcopy
	endif
	GLOBAL_CFLAGS = -fno-pic -mips4 -meb -G 0
	GLOBAL_LDFLAGS =
	LIBGCC = -lgcc
	LIBGCC_PATH = lib/libgcc/$(ARCH)
endif

ifeq ($(ARCH),ppc)
	ifneq ($(HOSTTYPE),ppc)
		CC = ppc-elf-gcc
		LD = ppc-elf-ld
		AS = ppc-elf-as
		AR = ppc-elf-ar
		OBJCOPY = ppc-elf-objcopy
	endif
	GLOBAL_CFLAGS = -fno-pic -O
	GLOBAL_LDFLAGS =
	LIBGCC = -lgcc
	LIBGCC_PATH = lib/libgcc/$(ARCH)
endif

OBJ_DIR = obj.$(ARCH)

GLOBAL_CFLAGS += -Wall -W -Wno-multichar -Wno-unused -nostdinc -fno-builtin -DARCH_$(ARCH)

# sub makefiles are responsible for adding to these
DEPS =
CLEAN =
FINAL =

final: final1

# include the top level makefile
include boot/$(ARCH)/boot.mk

BOOTMAKER_ARGS =
ifeq ($(ARCH),sparc)
BOOTMAKER_ARGS += --sparc
endif

final1: $(FINAL)
	rm -f final.$(ARCH);ln -sf $(FINAL) final.$(ARCH)
	rm -f system.$(ARCH);ln -sf $(KERNEL) system.$(ARCH)

tools: $(NETBOOT) $(BOOTMAKER) $(BIN2H) $(BIN2ASM)

$(BOOTMAKER): $(BOOTMAKER).c tools/sparcbootblock.h
	$(HOST_CC) -O0 -g -o $@ $(BOOTMAKER).c

NETBOOT_LINK_ARGS =
ifeq ($(OSTYPE),beos)
	NETBOOT_LINK_ARGS = -lsocket -lnet
endif
ifeq ($(shell uname),SunOS)
	NETBOOT_LINK_ARGS = -lsocket -lnsl
endif

$(NETBOOT): $(NETBOOT).c
	$(HOST_CC) -O3 -o $@ $(NETBOOT).c $(NETBOOT_LINK_ARGS)

$(BIN2ASM): $(BIN2ASM).c
	$(HOST_CC) -O3 -o $@ $(BIN2ASM).c

$(BIN2H): $(BIN2H).c
	$(HOST_CC) -O3 -o $@ $(BIN2H).c

toolsclean:
	rm -f $(BOOTMAKER) $(NETBOOT) $(NETBOOT_DC) $(BIN2H) $(BIN2ASM)

CLEAN += toolsclean

clean: $(CLEAN)
	rm -f $(KERNEL) $(FINAL)
	rm -f final.$(ARCH) system.$(ARCH)

depsclean:
	rm -f $(DEPS)

allclean: depsclean clean toolsclean
	rm -f `find . -type f -name '*.d'`
	rm -f `find . -type f -name '*.o'`

ifeq ($(filter $(MAKECMDGOALS), allclean), )
  include $(DEPS)
endif
