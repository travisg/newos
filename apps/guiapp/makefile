# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/guiapp
MY_SRCDIR := $(APPS_DIR)/guiapp
MY_TARGET :=  $(MY_TARGETDIR)/guiapp
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	app.cpp

MY_LIBS := -lc -lwin -lnewos -lsupc++

include templates/app.mk

endif

