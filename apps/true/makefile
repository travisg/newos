# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/true
MY_SRCDIR := $(APPS_DIR)/true
MY_TARGET :=  $(MY_TARGETDIR)/true
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

