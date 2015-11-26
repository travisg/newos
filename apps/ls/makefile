# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/ls
MY_SRCDIR := $(APPS_DIR)/ls
MY_TARGET :=  $(MY_TARGETDIR)/ls
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.c

MY_LIBS := -lc -lnewos -lsupc++

include templates/app.mk

endif

