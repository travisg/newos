# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/cpptest
MY_SRCDIR := $(APPS_DIR)/cpptest
MY_TARGET :=  $(MY_TARGETDIR)/cpptest
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.cpp

MY_LIBS := -lc -lnewos -lstdc++ -lsupc++

include templates/app.mk

endif

