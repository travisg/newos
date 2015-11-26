# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/init
MY_SRCDIR := $(APPS_DIR)/init
MY_TARGET :=  $(MY_TARGETDIR)/init
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	init.c

MY_LIBS := -lc -lnewos -lsupc++

include templates/app.mk

endif

