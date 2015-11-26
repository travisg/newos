# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/consoled
MY_SRCDIR := $(APPS_DIR)/consoled
MY_TARGET :=  $(MY_TARGETDIR)/consoled
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.c \
	keyboard_us.c

MY_LIBS := -lc -lnewos -lsupc++

include templates/app.mk

endif

