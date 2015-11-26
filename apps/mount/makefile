# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/mount
MY_SRCDIR := $(APPS_DIR)/mount
MY_TARGET :=  $(MY_TARGETDIR)/mount
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.c

MY_LIBS := -lc -lnewos -lsupc++

include templates/app.mk

endif

