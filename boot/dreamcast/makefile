
BOOT_SRCDIR := boot/$(ARCH)
BOOT_TARGETDIR := $(call TOBUILDDIR, boot)

STAGE2 = $(BOOT_TARGETDIR)/stage2

# make a rule for the stage2
MY_TARGETDIR := $(BOOT_TARGETDIR)
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(STAGE2)
MY_OBJS := \
	stage2.o \
	serial.o \
	mmu.o \
	vcpu.o \
	vcpu_c.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LDFLAGS := -dN
MY_LIBS := $(LIBS_BUILD_DIR)/libc.a
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(MY_SRCDIR)/stage2.ld
MY_GLUE := 

include templates/app.mk

# build stage1
STAGE1 := $(BOOT_TARGETDIR)/stage1

STAGE1_OBJS = \
	$(BOOT_TARGETDIR)/stage1.o

$(STAGE1): $(STAGE1_OBJS)
	$(LD) $(GLOBAL_LDFLAGS) -N -Ttext 0x8c000000 $(STAGE1_OBJS) -o $(STAGE1)

$(STAGE1).bin: $(STAGE1)
	$(OBJCOPY) -O binary $(STAGE1) $@1
	dd if=/dev/zero of=$(STAGE1).bin bs=4096 count=1 2> /dev/null
	dd if=$(STAGE1).bin1 of=$(STAGE1).bin conv=notrunc 2> /dev/null
	rm $(STAGE1).bin1

BOOTBLOCK := $(BOOT_SRCDIR)/bootblock.bin
CONFIG_FILE := $(BOOT_SRCDIR)/config.ini
SEMIFINAL := $(BOOT_TARGETDIR)/final.bootdir

$(FINAL): $(SEMIFINAL) $(STAGE1).bin
	cat $(STAGE1).bin $(SEMIFINAL) > $(FINAL)

$(SEMIFINAL): $(STAGE2) $(ALL) $(TOOLS) $(CONFIG_FILE)
	$(BOOTMAKER) --strip-debug --strip-binary $(STRIP) $(CONFIG_FILE) -o $@


