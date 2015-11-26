# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/socketd
MY_SRCDIR := $(APPS_DIR)/socketd
MY_TARGET :=  $(MY_TARGETDIR)/socketd
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.c

MY_LIBS := -lc -lsocket -lnewos -lsupc++

include templates/app.mk

endif

