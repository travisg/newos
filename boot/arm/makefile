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

CONFIG_FILE := $(BOOT_SRCDIR)/config.ini

SEMIFINAL := $(BOOT_TARGETDIR)/final.bootdir

$(SEMIFINAL): $(STAGE2) $(ALL) $(TOOLS) $(MAKEFLOP) $(CONFIG_FILE)
#	$(BOOTMAKER) $(CONFIG_FILE) -o $(SEMIFINAL)
	$(BOOTMAKER) --strip-debug --strip-binary $(STRIP) $(CONFIG_FILE) -o $(SEMIFINAL)

FINAL_ASMINCLUDE := $(BOOT_TARGETDIR)/final.asminclude

$(FINAL_ASMINCLUDE): $(SEMIFINAL) $(TOOLS)
	$(BIN2ASM) < $(SEMIFINAL) > $(FINAL_ASMINCLUDE)

STAGE1 := $(BOOT_TARGETDIR)/stage1
STAGE1_OBJS := $(BOOT_TARGETDIR)/stage1.o

ALL_DEPS := $(ALL_DEPS) $(STAGE1_OBJS:.o=.d)

$(BOOT_TARGETDIR)/stage1.o: $(BOOT_SRCDIR)/stage1.S $(FINAL_ASMINCLUDE)
	@mkdir -p $(BOOT_TARGETDIR)
	$(CC) $(GLOBAL_CFLAGS) -I. -Iinclude -I$(BOOT_TARGETDIR) -c $< -o $@

$(BOOT_TARGETDIR)/stage1.d: $(BOOT_SRCDIR)/stage1.S
	@mkdir -p $(BOOT_TARGETDIR)
	@echo "making deps for $<..."
	($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -g -Iinclude -I$(BOOT_TARGETDIR) -M -MG $<) > $@

#$(FINAL): $(ALL) $(TOOLS)
$(FINAL): $(STAGE1_OBJS) $(STAGE2)
	$(LD) $(GLOBAL_LDFLAGS) -dN --script=$(BOOT_SRCDIR)/stage1.ld $(STAGE1_OBJS) -o $@.elf
	$(OBJCOPY) -O binary $@.elf $@
