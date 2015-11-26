# app makefile

MY_TARGETDIR := $(APPS_BUILD_DIR)/test_input
MY_SRCDIR := $(APPS_DIR)/test_input
MY_TARGET :=  $(MY_TARGETDIR)/test_input
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	test_input.c


MY_LIBS := -lc -lnewos -lsupc++

include templates/app.mk

endif



