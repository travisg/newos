# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/irc
MY_SRCDIR := $(APPS_DIR)/irc
MY_TARGET :=  $(MY_TARGETDIR)/irc
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	app.cpp \
	ircengine.cpp \
	ircreader.cpp \
	term.cpp

MY_LIBS := -lc -lsocket -lnewos -lsupc++

include templates/app.mk

endif

