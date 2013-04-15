MY_TARGET_IN := $(MY_TARGET)
MY_TARGETDIR_IN := $(MY_TARGETDIR)
MY_SRCDIR_IN := $(MY_SRCDIR)
MY_SRCS_IN := $(MY_SRCS)
MY_EXTRAOBJS_IN := $(MY_EXTRAOBJS)
MY_CFLAGS_IN := $(MY_CFLAGS)
MY_CPPFLAGS_IN := $(MY_CPPFLAGS)
MY_LDFLAGS_IN := $(MY_LDFLAGS)
MY_INCLUDES_IN := $(MY_INCLUDES)
MY_LIBS_IN := $(MY_LIBS)
MY_LIBPATHS_IN := $(MY_LIBPATHS)
MY_DEPS_IN := $(MY_DEPS)
MY_LINKSCRIPT_IN := $(MY_LINKSCRIPT)
MY_GLUE_IN := $(MY_GLUE)

# XXX remove once we get a libsupc++
MY_LIBS_IN := $(filter-out -lsupc++,$(MY_LIBS_IN))

#$(warning MY_OBJS = $(MY_OBJS))

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
_TEMP_OBJS := $(MY_ASMOBJS_IN) $(MY_CPPOBJS_IN) $(MY_COBJS_IN) $(MY_EXTRAOBJS_IN)
#$(warning _TEMP_OBJS = $(_TEMP_OBJS))

# add to the global object list
ALL_OBJS := $(ALL_OBJS) $(_TEMP_OBJS)

# add to the global deps
ALL_DEPS := $(ALL_DEPS) $(_TEMP_OBJS:.o=.d)

ifeq ($(MY_LINKSCRIPT_IN), )
	MY_LINKSCRIPT_IN := $(APPS_LDSCRIPT)
endif

$(MY_TARGET_IN): MY_LDFLAGS_IN:=$(MY_LDFLAGS_IN)
$(MY_TARGET_IN): MY_LIBS_IN:=$(MY_LIBS_IN)
$(MY_TARGET_IN): MY_LIBPATHS_IN:=$(MY_LIBPATHS_IN)
$(MY_TARGET_IN): MY_LINKSCRIPT_IN:=$(MY_LINKSCRIPT_IN)
$(MY_TARGET_IN): MY_GLUE_IN:=$(MY_GLUE_IN)
$(MY_TARGET_IN): _TEMP_OBJS:=$(_TEMP_OBJS)
$(MY_TARGET_IN):: $(_TEMP_OBJS) $(MY_DEPS_IN) $(MY_GLUE_IN)
	@$(MKDIR)
	@echo linking $@
	@$(LD) $(GLOBAL_LDFLAGS) $(MY_LDFLAGS_IN) --script=$(MY_LINKSCRIPT_IN) -L $(LIBS_BUILD_DIR) $(MY_LIBPATHS_IN) -o $@ $(MY_GLUE_IN) $(_TEMP_OBJS) $(MY_LIBS_IN) $(LIBGCC)
	@echo creating listing file $@.lst
	@$(OBJDUMP) -C -S $@ > $@.lst

include templates/compile.mk

MY_TARGET :=
MY_TARGETDIR :=
MY_SRCDIR :=
MY_SRCS :=
MY_EXTRAOBJS :=
MY_CFLAGS :=
MY_CPPFLAGS :=
MY_LDFLAGS :=
MY_INCLUDES :=
MY_LIBS :=
MY_LIBPATHS :=
MY_DEPS :=
MY_LINKSCRIPT := 
MY_GLUE :=

