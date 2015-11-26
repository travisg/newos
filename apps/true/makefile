# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/true
MY_SRCDIR := $(APPS_DIR)/true
MY_TARGET :=  $(MY_TARGETDIR)/true
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.c

MY_LIBS := -lc -lnewos -lsupc++

include templates/app.mk

endif

