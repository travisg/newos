# app makefile

MY_TARGETDIR := $(APPS_BUILD_DIR)/test_output
MY_SRCDIR := $(APPS_DIR)/test_output
MY_TARGET :=  $(MY_TARGETDIR)/test_output
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)


MY_SRCS := \
	test_output.c

MY_LIBS := -lc -lnewos -lsupc++

include templates/app.mk

endif



