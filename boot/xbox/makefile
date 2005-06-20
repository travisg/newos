
BOOT_SRCDIR := $(GET_LOCAL_DIR)
BOOT_TARGETDIR := $(call TOBUILDDIR, boot)

STAGE1 := $(BOOT_TARGETDIR)/stage1

# rules for stage1
MY_TARGETDIR := $(BOOT_TARGETDIR)
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(STAGE1)
MY_OBJS := \
	stage1.o \
	inflate.o \
	stage2.o \
	stage2_asm.o \
	fb_console.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LDFLAGS := -dN
MY_LIBS := $(LIBS_BUILD_DIR)/libc.a
MY_LIBPATHS :=
MY_DEPS := $(MY_LIBS)
MY_LINKSCRIPT := $(MY_SRCDIR)/stage1.ld
MY_GLUE :=

include templates/app.mk

BOOTBLOCK := $(BOOT_SRCDIR)/bootblock.bin
BOOTBLOCK_VESA := $(BOOT_SRCDIR)/bootblock.bin.vesa
CONFIG_FILE := $(BOOT_SRCDIR)/config.ini

$(FINAL): $(STAGE1).bin $(ALL) $(TOOLS) $(MAKEFLOP) $(CONFIG_FILE)
	$(BOOTMAKER) --strip-debug --strip-binary $(STRIP) $(CONFIG_FILE) -o $(FINAL).pre
	gzip -f -9 $(FINAL).pre
	cat $(STAGE1).bin $(FINAL).pre.gz > $(FINAL)
	@rm -f $(FINAL).pre $(FINAL).pre.gz

$(STAGE1).bin: $(STAGE1)
	objcopy -O binary $< $@
