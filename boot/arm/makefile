BOOT_SRCDIR := boot/$(ARCH)
BOOT_TARGETDIR := $(call TOBUILDDIR, boot)

STAGE2 = $(BOOT_TARGETDIR)/stage2

# make a rule for the stage2
MY_TARGETDIR := $(BOOT_TARGETDIR)
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(STAGE2)
MY_OBJS := \
    stage2.o \

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
	stage1_main.o \
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
MAKEFLOP := $(BOOT_SRCDIR)/makeflop
CONFIG_FILE := $(BOOT_SRCDIR)/config.ini

$(FINAL): $(STAGE1) $(STAGE2) $(ALL) $(TOOLS) $(CONFIG_FILE)
	$(BOOTMAKER) --strip-debug --strip-binary $(STRIP) $(CONFIG_FILE) -o $(FINAL).pre
	gzip -f -9 $(FINAL).pre
	cat $(STAGE1) $(FINAL).pre.gz > $(FINAL)

$(STAGE1): $(STAGE1).elf
	$(OBJCOPY) -O binary $(STAGE1).elf $(STAGE1)

