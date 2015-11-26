# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/vmtest
MY_SRCDIR := $(APPS_DIR)/vmtest
MY_TARGET :=  $(MY_TARGETDIR)/vmtest
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.c

MY_LIBS := -lc -lnewos -lsupc++

include templates/app.mk

endif

