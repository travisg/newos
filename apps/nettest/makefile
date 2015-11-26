# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/nettest
MY_SRCDIR := $(APPS_DIR)/nettest
MY_TARGET :=  $(MY_TARGETDIR)/nettest
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.c

MY_LIBS := -lc -lsocket -lnewos -lsupc++

include templates/app.mk

endif

