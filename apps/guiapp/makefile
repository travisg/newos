# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/guiapp
MY_SRCDIR := $(APPS_DIR)/guiapp
MY_TARGET :=  $(MY_TARGETDIR)/guiapp
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	app.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := -lc -lwin -lnewos -lsupc++
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif

