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
MY_LIBS := $(LIBS_BUILD_DIR)/libc/libc.a
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := $(MY_SRCDIR)/stage2.ld
MY_GLUE :=

include templates/app.mk

#$(FINAL): $(ALL) $(TOOLS)
$(FINAL): $(STAGE2)
	touch $(FINAL)
