
BOOT_SRCDIR := boot/$(ARCH)
BOOT_TARGETDIR := $(call TOBUILDDIR, boot)

CONFIG_FILE := $(BOOT_SRCDIR)/config.ini

STAGE2 = $(BOOT_TARGETDIR)/stage2

# make a rule for the stage2
MY_TARGETDIR := $(BOOT_TARGETDIR)
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(STAGE2)
MY_OBJS := \
	stage2.o \
	stage2_asm.o \
	stage2_nextmon.o \
	stage2_text.o

MY_INCLUDES := $(STDINCLUDE) -I.
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LDFLAGS := -dN
MY_LIBS := $(LIBS_BUILD_DIR)/libc/libc.a
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(MY_SRCDIR)/stage2.ld
MY_GLUE := 

include templates/app.mk

SEMIFINAL = $(BOOT_TARGETDIR)/final.bootdir

#$(SEMIFINAL): $(STAGE2) $(ALL) $(CONFIG_FILE) $(TOOLS) 
$(SEMIFINAL): $(STAGE2) $(CONFIG_FILE) $(TOOLS) 
	$(BOOTMAKER) --bigendian $(CONFIG_FILE) -o $(SEMIFINAL)

FINAL_ASMINCLUDE = $(BOOT_TARGETDIR)/final.asminclude

$(FINAL_ASMINCLUDE): $(SEMIFINAL) $(TOOLS)
	$(BIN2ASM) < $(SEMIFINAL) > $(FINAL_ASMINCLUDE)

STAGE1 = $(BOOT_TARGETDIR)/stage1

# rules for stage1
MY_TARGETDIR := $(BOOT_TARGETDIR)
MY_SRCDIR := $(BOOT_SRCDIR)
MY_TARGET := $(STAGE1)
MY_OBJS := \
	stage1.o

MY_INCLUDES := $(STDINCLUDE) -I.
MY_CFLAGS := $(KERNEL_CFLAGS)
MY_LDFLAGS := -dN
MY_LIBS := $(LIBS_BUILD_DIR)/libc/libc.a
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(MY_SRCDIR)/stage1.ld
MY_GLUE :=

include templates/app.mk


AWKPROG='\
{ \
		printf "\0\207\01\07"; \
		printf "\0%c%c%c", $$1 / 65536, $$1 / 256, $$1; \
		printf "\0%c%c%c", $$2 / 65536, $$2 / 256, $$2; \
		printf "\0%c%c%c", $$3 / 65536, $$3 / 256, $$3; \
		printf "\0\0\0\0\04\070\0\0\0\0\0\0\0\0\0\0" \
}'

$(warning $(FINAL))

$(FINAL): $(STAGE1)
	@${SIZE} $(STAGE1)
	@${OBJCOPY} -O binary $(STAGE1) $@.raw
	@(${SIZE} $(STAGE1) | tail +2 | ${AWK} ${AWKPROG} ; cat $@.raw) > $@

