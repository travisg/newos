# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/rm
MY_SRCDIR := $(APPS_DIR)/rm
MY_TARGET :=  $(MY_TARGETDIR)/rm
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	main.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := -lc -lnewos -lsupc++
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif

