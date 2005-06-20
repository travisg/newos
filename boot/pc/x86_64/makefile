
BOOT_SRCDIR := $(GET_LOCAL_DIR)
BOOT_TARGETDIR := $(call TOBUILDDIR, boot)

STAGE2 := $(BOOT_TARGETDIR)/stage2

ifeq ($(OSTYPE),cygwin)
	BOOT_IMAGE_TARGET ?= a:
else
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
MY_DEPS := $(MY_LIBS)
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
MY_DEPS := $(MY_LIBS)
MY_LINKSCRIPT := $(MY_SRCDIR)/stage1.ld
MY_GLUE :=

include templates/app.mk

# rules to build the bootblock
BOOTBLOCK = $(BOOT_TARGETDIR)/bootblock

MY_TARGETDIR := $(BOOT_TARGETDIR)
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(BOOTBLOCK).elf
MY_OBJS := \
	bootblock.o

MY_INCLUDES :=
MY_CFLAGS := 
MY_LDFLAGS := -dN
MY_LIBS :=
MY_LIBPATHS :=
MY_DEPS := 
MY_LINKSCRIPT := $(MY_SRCDIR)/bootblock.ld
MY_GLUE :=

include templates/app.mk

MAKEFLOP := $(BOOT_SRCDIR)/makeflop
CONFIG_FILE := $(BOOT_SRCDIR)/config.ini

$(FINAL): $(BOOTBLOCK) $(STAGE1) $(TOOLS) $(MAKEFLOP)
	cat $(STAGE1) > $(FINAL).pre
	$(MAKEFLOP) -p $(shell expr 18 \* 2 \* 512) $(BOOTBLOCK) $(FINAL).pre $(FINAL)
	@rm -f $(FINAL).pre $(FINAL).prepre $(FINAL).prepre.gz

#$(FINAL): $(BOOTBLOCK) $(STAGE1) $(STAGE2) $(ALL) $(TOOLS) $(MAKEFLOP) $(CONFIG_FILE)
#	$(BOOTMAKER) --strip-debug --strip-binary $(STRIP) $(CONFIG_FILE) -o $(FINAL).prepre
#	gzip -f -9 $(FINAL).prepre
#	cat $(STAGE1) $(FINAL).prepre.gz > $(FINAL).pre
#	$(MAKEFLOP) -p $(shell expr 18 \* 2 \* 512) $(BOOTBLOCK) $(FINAL).pre $(FINAL)
#	@rm -f $(FINAL).pre $(FINAL).prepre $(FINAL).prepre.gz

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

$(MAKEFLOP): $(MAKEFLOP).c
	$(HOST_CC) -O $(MAKEFLOP).c -o $@

