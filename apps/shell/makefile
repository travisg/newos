# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/shell
MY_SRCDIR := $(APPS_DIR)/shell
MY_TARGET :=  $(MY_TARGETDIR)/shell
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	commands.o \
	main.o \
	parse.o \
	statements.o \
	shell_vars.o \
	file_utils.o \
	script.o \
	args.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := $(ULIBS)
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif

