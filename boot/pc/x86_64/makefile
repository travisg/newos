
BOOT_SRCDIR := $(GET_LOCAL_DIR)
BOOT_TARGETDIR := $(call TOBUILDDIR, boot)

LOADER := $(BOOT_TARGETDIR)/bld

ifeq ($(OSTYPE),cygwin)
	BOOT_IMAGE_TARGET ?= a:
else
	BOOT_IMAGE_TARGET ?= /dev/fd0
endif

LOADER_PAYLOAD := $(BOOT_TARGETDIR)/payload.o
 
# make a rule for the unified loader
MY_TARGETDIR := $(BOOT_TARGETDIR)
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(LOADER)
MY_SRCS := \
	stage1_entry.S \
	stage1.c \
	inflate.c \
	stage2.c \
	mmu.c \
	heap.c \
	multiboot.c \
	stage2_asm.S

#	smp_boot.c \
	smp_trampoline.S

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(KERNEL_CFLAGS) -mno-sse
MY_LDFLAGS := -dN
MY_LIBS := $(LIBS_BUILD_DIR)/libc.a
MY_LIBPATHS :=
MY_DEPS := $(MY_LIBS)
MY_LINKSCRIPT := $(MY_SRCDIR)/stage1.ld
MY_GLUE := 
MY_EXTRAOBJS := $(LOADER_PAYLOAD)

include templates/app.mk

CONFIG_FILE := $(BOOT_SRCDIR)/dummy.ini

$(FINAL): $(LOADER)
	$(OBJCOPY) -O binary $< $@

$(FINAL).bootmaker.gz: $(TOOLS) $(CONFIG_FILE)
	@echo creating $@
	@$(MKDIR)
	@$(BOOTMAKER) --strip-debug --strip-binary $(STRIP) $(CONFIG_FILE) -o $(FINAL).bootmaker
	@gzip -f -9 $(FINAL).bootmaker

$(LOADER_PAYLOAD): $(FINAL).bootmaker.gz
	@echo building and assembling $@
	@$(MKDIR)
	@(echo .section .payload, \"al\"; cat $< | $(BIN2ASM)) > $(LOADER_PAYLOAD).S
	@$(CC) -c $(LOADER_PAYLOAD).S -o $@

disk: $(FINAL)
ifeq ($(OSTYPE),cygwin)
	fdimage -q $(FINAL) $(BOOT_IMAGE_TARGET)
else
	dd if=$(FINAL) of=$(BOOT_IMAGE_TARGET) bs=18k
endif

netinstall: $(FINAL)
	scp $(FINAL) four:/tftproot/grubtest

