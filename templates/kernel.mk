MY_TARGET_IN := $(MY_TARGET)
MY_TARGETLIB_IN := $(MY_TARGETLIB)
MY_TARGETDIR_IN := $(MY_TARGETDIR)
MY_SRCDIR_IN := $(MY_SRCDIR)
MY_OBJS_IN := $(MY_OBJS)
MY_CFLAGS_IN := $(MY_CFLAGS)
MY_CPPFLAGS_IN := $(MY_CPPFLAGS)
MY_INCLUDES_IN := $(MY_INCLUDES)
MY_LINKSCRIPT_IN := $(MY_LINKSCRIPT)
MY_LIBS_IN := $(MY_LIBS)

# create a new version in the target directory
_TEMP_OBJS := $(addprefix $(MY_TARGETDIR_IN)/,$(MY_OBJS_IN))

ALL_OBJS := $(ALL_OBJS) $(_TEMP_OBJS)

# add to the global deps
ALL_DEPS := $(ALL_DEPS) $(_TEMP_OBJS:.o=.d)

$(MY_TARGET_IN): MY_LIBS_IN:=$(MY_LIBS_IN)
$(MY_TARGET_IN): MY_LINKSCRIPT_IN:=$(MY_LINKSCRIPT_IN)
$(MY_TARGET_IN): _TEMP_OBJS:=$(_TEMP_OBJS)
$(MY_TARGET_IN): $(_TEMP_OBJS) $(MY_LIBS_IN)
	@$(MKDIR)
	@echo linking $@
	@$(LD) $(GLOBAL_LDFLAGS) -Bdynamic -export-dynamic -dynamic-linker /foo/bar -T $(MY_LINKSCRIPT_IN) -L $(LIBGCC_PATH) -o $@ $(_TEMP_OBJS) $(MY_LIBS_IN) $(LIBGCC)
	@echo creating listing file $@.lst
	@$(OBJDUMP) -d $@ > $@.lst

$(MY_TARGETLIB_IN): MY_LIBS_IN:=$(MY_LIBS_IN)
$(MY_TARGETLIB_IN): MY_LINKSCRIPT_IN:=$(MY_LINKSCRIPT_IN)
$(MY_TARGETLIB_IN): _TEMP_OBJS:=$(_TEMP_OBJS)
$(MY_TARGETLIB_IN): $(_TEMP_OBJS) $(MY_LIBS_IN)
	@$(MKDIR)
	@echo linking $@
	@$(LD) $(GLOBAL_LDFLAGS) -Bdynamic -shared -export-dynamic -dynamic-linker /foo/bar -T $(MY_LINKSCRIPT_IN) -L $(LIBGCC_PATH) -o $@ $(_TEMP_OBJS) $(MY_LIBS_IN) $(LIBGCC)

include templates/compile.mk

MY_TARGET :=
MY_TARGETLIB :=
MY_TARGETDIR :=
MY_SRCDIR :=
MY_OBJS :=
MY_CFLAGS :=
MY_CPPFLAGS :=
MY_INCLUDES :=
MY_LINKSCRIPT :=
MY_LIBS :=

