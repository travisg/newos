# if the target isn't set, assume its the last element of the sourcedir path
ifeq ($(MY_TARGET),)
MY_TARGET := $(call TOBUILDDIR,$(MY_SRCDIR)/$(notdir $(MY_SRCDIR)))
endif

#$(warning MY_SRCDIR = $(MY_SRCDIR))
#$(warning MY_TARGET = $(MY_TARGET))

MY_COMPILEFLAGS := $(USER_COMPILEFLAGS) $(MY_COMPILEFLAGS)
MY_INCLUDES := $(STDINCLUDE) $(MY_INCLUDES)
MY_EXTRAOBJS := $(APPSGLUE) $(MY_EXTRAOBJS)
ifeq ($(MY_LINKSCRIPT), )
    MY_LINKSCRIPT := $(APPS_LDSCRIPT)
endif

# XXX remove once we get a libsupc++
MY_LIBS := $(filter-out -lsupc++,$(MY_LIBS))

include templates/simple_object.mk
