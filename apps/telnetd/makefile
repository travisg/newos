# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/telnetd
MY_SRCDIR := $(APPS_DIR)/telnetd
MY_TARGET :=  $(MY_TARGETDIR)/telnetd
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.c

MY_LIBS := -lc -lsocket -lnewos -lsupc++

include templates/app.mk

endif

