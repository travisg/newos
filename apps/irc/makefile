# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/irc
MY_SRCDIR := $(APPS_DIR)/irc
MY_TARGET :=  $(MY_TARGETDIR)/irc
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	app.o \
	ircengine.o \
	ircreader.o \
	term.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := $(ULIBS)
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif

