
BOOT_SRCDIR := boot/$(ARCH)
BOOT_TARGETDIR := $(call TOBUILDDIR, boot)

STAGE2 = $(BOOT_TARGETDIR)/stage2

# make a rule for the stage2
MY_TARGETDIR := $(BOOT_TARGETDIR)
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(STAGE2)
MY_OBJS := \
	stage2.o \
	stage2_asm.o \
	smp_boot.o \
	smp_trampoline.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LDFLAGS := -dN
MY_LIBS := $(LIBS_BUILD_DIR)/libc/libc.a
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(MY_SRCDIR)/stage2.ld
MY_GLUE := 

include templates/app.mk

BOOTBLOCK := $(BOOT_SRCDIR)/bootblock.bin
MAKEFLOP := $(BOOT_SRCDIR)/makeflop
CONFIG_FILE := $(BOOT_SRCDIR)/config.ini

$(FINAL): $(STAGE2) $(ALL) $(TOOLS) $(MAKEFLOP) $(CONFIG_FILE) $(BOOTBLOCK)
	$(BOOTMAKER) --strip-debug --strip-binary $(STRIP) $(CONFIG_FILE) -o $(FINAL).pre
	$(MAKEFLOP) $(BOOTBLOCK) $(FINAL).pre $(FINAL)
	@rm -f $(FINAL).pre

disk: $(FINAL)
ifeq ($(OSTYPE),cygwin)
	fdimage -q $(FINAL) a:
else
	dd if=$(FINAL) of=/dev/disk/floppy/raw bs=18k
endif

$(MAKEFLOP): $(MAKEFLOP).c
	$(HOST_CC) -O $(MAKEFLOP).c -o $@

