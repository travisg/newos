# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/init
MY_SRCDIR := $(APPS_DIR)/init
MY_TARGET :=  $(MY_TARGETDIR)/init
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	init.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := -lc -lnewos -lsupc++
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif

