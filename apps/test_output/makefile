# app makefile

MY_TARGETDIR := $(APPS_BUILD_DIR)/test_output
MY_SRCDIR := $(APPS_DIR)/test_output
MY_TARGET :=  $(MY_TARGETDIR)/test_output
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)


MY_OBJS := \
	test_output.o


MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := -lc -lnewos -lsupc++
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif



