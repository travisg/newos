# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/testapp
MY_SRCDIR := $(APPS_DIR)/testapp
MY_TARGET :=  $(MY_TARGETDIR)/testapp
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	main.o \
	misctests.o \
	pipetests.o \
	porttests.o \
	sigtests.o \
	threadtests.o \
	vmtests.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := -lc -lnewos -lsupc++
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif

