# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/shell
MY_SRCDIR := $(APPS_DIR)/shell
MY_TARGET :=  $(MY_TARGETDIR)/shell
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	commands.c \
	main.c \
	parse.c \
	statements.c \
	shell_vars.c \
	file_utils.c \
	script.c \
	args.c

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := -lc -lnewos -lsupc++
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif

