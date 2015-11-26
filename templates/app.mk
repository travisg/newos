MY_COMPILEFLAGS := $(USER_COMPILEFLAGS) $(MY_COMPILEFLAGS)
MY_INCLUDES := $(STDINCLUDE) $(MY_INCLUDES)
MY_EXTRAOBJS := $(APPSGLUE) $(MY_EXTRAOBJS)
ifeq ($(MY_LINKSCRIPT), )
    MY_LINKSCRIPT := $(APPS_LDSCRIPT)
endif

# XXX remove once we get a libsupc++
MY_LIBS := $(filter-out -lsupc++,$(MY_LIBS))

include templates/simple_object.mk
