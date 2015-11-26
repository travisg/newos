# app makefile

MY_TARGETDIR := $(APPS_BUILD_DIR)/test_time
MY_SRCDIR := $(APPS_DIR)/test_time
MY_TARGET :=  $(MY_TARGETDIR)/test_time
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)


MY_SRCS := \
	test_time.c

MY_LIBS := -lc -lnewos -lsupc++

include templates/app.mk

endif


