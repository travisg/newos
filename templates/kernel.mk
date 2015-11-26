MY_TARGET_IN := $(MY_TARGET)
MY_TARGETLIB_IN := $(MY_TARGETLIB)
MY_TARGETDIR_IN := $(MY_TARGETDIR)
MY_SRCDIR_IN := $(MY_SRCDIR)
MY_SRCS_IN := $(MY_SRCS)
MY_COMPILEFLAGS_IN := $(KERNEL_COMPILEFLAGS) $(MY_COMPILEFLAGS)
MY_CFLAGS_IN := $(MY_CFLAGS)
MY_CPPFLAGS_IN := $(MY_CPPFLAGS)
MY_INCLUDES_IN := $(STDINCLUDE) $(MY_INCLUDES)
MY_LIBS_IN := $(MY_LIBS)
MY_DEPS_IN := $(MY_DEPS)
MY_LINKSCRIPT_IN := $(MY_LINKSCRIPT)

ifeq ($(MY_LINKSCRIPT_IN), )
    MY_LINKSCRIPT_IN := $(KERNEL_DIR)/arch/$(ARCH)/kernel.ld
endif

# extract the different source types out of the list
#$(warning MY_SRCS_IN = $(MY_SRCS_IN))
MY_CPPSRCS_IN := $(filter %.cpp,$(MY_SRCS_IN))
MY_CSRCS_IN := $(filter %.c,$(MY_SRCS_IN))
MY_ASMSRCS_IN := $(filter %.S,$(MY_SRCS_IN))

#$(warning MY_CPPSRCS_IN = $(MY_CPPSRCS_IN))
#$(warning MY_CSRCS_IN = $(MY_CSRCS_IN))
#$(warning MY_ASMSRCS_IN = $(MY_ASMSRCS_IN))

# build a list of objects
MY_CPPOBJS_IN := $(addprefix $(MY_TARGETDIR_IN)/,$(patsubst %.cpp,%.o,$(MY_CPPSRCS_IN)))
MY_COBJS_IN := $(addprefix $(MY_TARGETDIR_IN)/,$(patsubst %.c,%.o,$(MY_CSRCS_IN)))
MY_ASMOBJS_IN := $(addprefix $(MY_TARGETDIR_IN)/,$(patsubst %.S,%.o,$(MY_ASMSRCS_IN)))
_TEMP_OBJS := $(MY_CPPOBJS_IN) $(MY_COBJS_IN) $(MY_ASMOBJS_IN)
#$(warning _TEMP_OBJS = $(_TEMP_OBJS))

# add to the global object list
ALL_OBJS := $(ALL_OBJS) $(_TEMP_OBJS)

# add to the global deps
ALL_DEPS := $(ALL_DEPS) $(_TEMP_OBJS:.o=.d)

$(MY_TARGET_IN): MY_LIBS_IN:=$(MY_LIBS_IN)
$(MY_TARGET_IN): MY_LINKSCRIPT_IN:=$(MY_LINKSCRIPT_IN)
$(MY_TARGET_IN): _TEMP_OBJS:=$(_TEMP_OBJS)
$(MY_TARGET_IN): $(_TEMP_OBJS) $(MY_DEPS_IN) $(MY_LIBS_IN)
	@$(MKDIR)
	@echo linking $@
	$(NOECHO)$(LD) $(GLOBAL_LDFLAGS) -Bdynamic -export-dynamic -dynamic-linker /foo/bar -T $(MY_LINKSCRIPT_IN) -L $(LIBGCC_PATH) -o $@ $(_TEMP_OBJS) $(MY_LIBS_IN) $(LIBGCC)
	@echo creating listing file $@.lst
	$(NOECHO)$(OBJDUMP) -C -S $@ > $@.lst

$(MY_TARGETLIB_IN): MY_LIBS_IN:=$(MY_LIBS_IN)
$(MY_TARGETLIB_IN): MY_LINKSCRIPT_IN:=$(MY_LINKSCRIPT_IN)
$(MY_TARGETLIB_IN): _TEMP_OBJS:=$(_TEMP_OBJS)
$(MY_TARGETLIB_IN): $(_TEMP_OBJS) $(MY_DEPS_IN) $(MY_LIBS_IN)
	@$(MKDIR)
	@echo linking $@
	$(NOECHO)$(LD) $(GLOBAL_LDFLAGS) -Bdynamic -shared -export-dynamic -dynamic-linker /foo/bar -T $(MY_LINKSCRIPT_IN) -L $(LIBGCC_PATH) -o $@ $(_TEMP_OBJS) $(MY_LIBS_IN) $(LIBGCC)

include templates/compile.mk

MY_CPPSRCS_IN :=
MY_CSRCS_IN :=
MY_ASMSRCS_IN :=

MY_TARGET :=
MY_TARGETLIB :=
MY_TARGETDIR :=
MY_SRCDIR :=
MY_SRCS :=
MY_COMPILEFLAGS :=
MY_CFLAGS :=
MY_CPPFLAGS :=
MY_INCLUDES :=
MY_LINKSCRIPT :=
MY_LIBS :=
MY_DEPS :=

