# app makefile

MY_TARGETDIR := $(APPS_BUILD_DIR)/test_input
MY_SRCDIR := $(APPS_DIR)/test_input
MY_TARGET :=  $(MY_TARGETDIR)/test_input
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	test_input.o


MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := $(ULIBS)
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif



