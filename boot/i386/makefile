
BOOT_SRCDIR := boot/$(ARCH)
BOOT_TARGETDIR := $(call TOBUILDDIR, boot)

STAGE2 = $(BOOT_TARGETDIR)/stage2

ifeq ($(OSTYPE),cygwin)
	BOOT_IMAGE_TARGET ?= a:
else ifeq ($(OSTYPE),linux)
	BOOT_IMAGE_TARGET ?= /dev/fd0
endif

# make a rule for the stage2
MY_TARGETDIR := $(BOOT_TARGETDIR)
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(STAGE2)
MY_OBJS := \
	stage2.o \
	stage2_asm.o \
	smp_boot.o \
	smp_trampoline.o \
	int86.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LDFLAGS := -dN
MY_LIBS := $(LIBS_BUILD_DIR)/libc.a
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(MY_SRCDIR)/stage2.ld
MY_GLUE := 

include templates/app.mk

STAGE1 = $(BOOT_TARGETDIR)/stage1

# rules for stage1
MY_TARGETDIR := $(BOOT_TARGETDIR)
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(STAGE1).elf
MY_OBJS := \
	stage1.o \
	inflate.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LDFLAGS := -dN
MY_LIBS := $(LIBS_BUILD_DIR)/libc.a
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(MY_SRCDIR)/stage1.ld
MY_GLUE :=

include templates/app.mk

BOOTBLOCK := $(BOOT_SRCDIR)/bootblock.bin
BOOTBLOCK_VESA := $(BOOT_SRCDIR)/bootblock.bin.vesa
MAKEFLOP := $(BOOT_SRCDIR)/makeflop
CONFIG_FILE := $(BOOT_SRCDIR)/config.ini

$(FINAL): $(STAGE1) $(STAGE2) $(ALL) $(TOOLS) $(MAKEFLOP) $(CONFIG_FILE) $(BOOTBLOCK)
	$(BOOTMAKER) --strip-debug --strip-binary $(STRIP) $(CONFIG_FILE) -o $(FINAL).prepre
	gzip -f -9 $(FINAL).prepre
	cat $(STAGE1) $(FINAL).prepre.gz > $(FINAL).pre
	$(MAKEFLOP) -p $(shell expr 18 \* 2 \* 512) $(BOOTBLOCK) $(FINAL).pre $(FINAL)
	@rm -f $(FINAL).pre $(FINAL).prepre $(FINAL).prepre.gz

$(FINAL).v: $(STAGE1) $(STAGE2) $(ALL) $(TOOLS) $(MAKEFLOP) $(CONFIG_FILE) $(BOOTBLOCK_VESA)
	$(BOOTMAKER) --strip-debug --strip-binary $(STRIP) $(CONFIG_FILE) -o $(FINAL).prepre
	gzip -f -9 $(FINAL).prepre
	cat $(STAGE1) $(FINAL).prepre.gz > $(FINAL).pre
	$(MAKEFLOP) -p $(shell expr 18 \* 2 \* 512) $(BOOTBLOCK_VESA) $(FINAL).pre $(FINAL).v
	@rm -f $(FINAL).pre $(FINAL).prepre $(FINAL).prepre.gz

$(STAGE1): $(STAGE1).elf
	$(OBJCOPY) -O binary $(STAGE1).elf $(STAGE1)

disk: $(FINAL)
ifeq ($(OSTYPE),cygwin)
	fdimage -q $(FINAL) $(BOOT_IMAGE_TARGET)
else
	dd if=$(FINAL) of=$(BOOT_IMAGE_TARGET) bs=18k
endif

vdisk: $(FINAL).v
ifeq ($(OSTYPE),cygwin)
	fdimage -q $(FINAL).v $(BOOT_IMAGE_TARGET)
else
	dd if=$(FINAL).v of=$(BOOT_IMAGE_TARGET) bs=18k
endif

$(MAKEFLOP): $(MAKEFLOP).c
	$(HOST_CC) -O $(MAKEFLOP).c -o $@

