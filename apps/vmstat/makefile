# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/vmstat
MY_SRCDIR := $(APPS_DIR)/vmstat
MY_TARGET :=  $(MY_TARGETDIR)/vmstat
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.c

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := -lc -lnewos -lsupc++
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif

