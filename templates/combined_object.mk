# only build this if FORCE_BUILD is set or MY_TARGET is in the ALL list
ifeq ($(if $(FORCE_BUILD),1,$(call FINDINLIST,$(MY_TARGET),$(ALL))),1)

MY_TARGET_IN := $(MY_TARGET)
MY_TARGETDIR_IN := $(MY_TARGETDIR)
MY_SRCDIR_IN := $(MY_SRCDIR)
MY_SRCS_IN := $(MY_SRCS)
MY_COMPILEFLAGS_IN := $(MY_COMPILEFLAGS)
MY_CFLAGS_IN := $(MY_CFLAGS)
MY_CPPFLAGS_IN := $(MY_CPPFLAGS)
MY_INCLUDES_IN := $(MY_INCLUDES)

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
_TEMP_OBJS := $(MY_CPPOBJS_IN) $(MY_COBJS_IN) $(MY_ASMOBJS_IN)
#$(warning _TEMP_OBJS = $(_TEMP_OBJS))

# add to the global object list
ALL_OBJS := $(ALL_OBJS) $(_TEMP_OBJS)

# add to the global deps
ALL_DEPS := $(ALL_DEPS) $(_TEMP_OBJS:.o=.d)

$(MY_TARGET_IN): $(_TEMP_OBJS)
	@$(MKDIR)
	@echo linking $@
	$(NOECHO)$(LD) $(GLOBAL_LDFLAGS) -r -o $@ $^

include templates/compile.mk

endif # find in ALL

MY_TARGET :=
MY_TARGETDIR :=
MY_SRCDIR :=
MY_SRCS :=
MY_COMPILEFLAGS :=
MY_CFLAGS :=
MY_CPPFLAGS :=
MY_INCLUDES :=

