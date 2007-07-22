
BOOT_SRCDIR := $(GET_LOCAL_DIR)
BOOT_TARGETDIR := $(call TOBUILDDIR, boot)

STAGE2 := $(BOOT_TARGETDIR)/stage2/stage2

ifeq ($(OSTYPE),cygwin)
	BOOT_IMAGE_TARGET ?= a:
else
	BOOT_IMAGE_TARGET ?= /dev/fd0
endif

# make a rule for the stage2
MY_TARGETDIR := $(BOOT_TARGETDIR)/stage2
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(STAGE2)
MY_SRCS := \
	stage2.c \
	stage2_asm.S \
	smp_boot.c \
	smp_trampoline.S \
	int86.S

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LDFLAGS := -dN
MY_LIBS := $(LIBS_BUILD_DIR)/libc.a
MY_LIBPATHS :=
MY_DEPS := $(MY_LIBS)
MY_LINKSCRIPT := $(MY_SRCDIR)/stage2.ld
MY_GLUE := 

include templates/app.mk

STAGE1 = $(BOOT_TARGETDIR)/stage1/stage1

# rules for stage1
MY_TARGETDIR := $(BOOT_TARGETDIR)/stage1
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(STAGE1).elf
MY_SRCS := \
	stage1.c \
	inflate.c

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LDFLAGS := -dN
MY_LIBS := $(LIBS_BUILD_DIR)/libc.a
MY_LIBPATHS :=
MY_DEPS := $(MY_LIBS)
MY_LINKSCRIPT := $(MY_SRCDIR)/stage1.ld
MY_GLUE :=

include templates/app.mk

# rules to build the bootblock
BOOTBLOCK = $(BOOT_TARGETDIR)/bootblock/bootblock

MY_TARGETDIR := $(BOOT_TARGETDIR)/bootblock
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(BOOTBLOCK).elf
MY_SRCS := \
	bootblock.S

MY_INCLUDES :=
MY_CFLAGS := 
MY_LDFLAGS := -dN
MY_LIBS :=
MY_LIBPATHS :=
MY_DEPS := 
MY_LINKSCRIPT := $(MY_SRCDIR)/bootblock.ld
MY_GLUE :=

include templates/app.mk

MAKEFLOP := $(BOOT_TARGETDIR)/makeflop
MAKEFLOPSRC := $(BOOT_SRCDIR)/makeflop.c
CONFIG_FILE := $(BOOT_SRCDIR)/config.ini

$(FINAL): $(BOOTBLOCK) $(STAGE1) $(STAGE2) $(ALL) $(TOOLS) $(MAKEFLOP) $(CONFIG_FILE) $(BOOTBLOCK)
	$(BOOTMAKER) --strip-debug --strip-binary $(STRIP) $(CONFIG_FILE) -o $(FINAL).prepre
	gzip -f -9 $(FINAL).prepre
	cat $(STAGE1) $(FINAL).prepre.gz > $(FINAL).pre
	$(MAKEFLOP) -p $(shell expr 18 \* 2 \* 512) $(BOOTBLOCK) $(FINAL).pre $(FINAL)
	$(MAKEFLOP) -p $(shell expr 18 \* 2 \* 512 \* 80) $(BOOTBLOCK) $(FINAL).pre $(FINAL).flp
	@rm -f $(FINAL).pre $(FINAL).prepre $(FINAL).prepre.gz

$(STAGE1): $(STAGE1).elf
	$(OBJCOPY) -O binary $< $@

$(BOOTBLOCK): $(BOOTBLOCK).elf
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -mi8086 -d $< > $@.16-bit.lst

disk: $(FINAL)
ifeq ($(OSTYPE),cygwin)
	fdimage -q $(FINAL) $(BOOT_IMAGE_TARGET)
else
	dd if=$(FINAL) of=$(BOOT_IMAGE_TARGET) bs=18k
endif

$(MAKEFLOP): $(MAKEFLOPSRC)
	$(HOST_CC) -O $(MAKEFLOPSRC) -o $@

