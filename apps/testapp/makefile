# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/testapp
MY_SRCDIR := $(APPS_DIR)/testapp
MY_TARGET :=  $(MY_TARGETDIR)/testapp
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.cpp \
	misctests.cpp \
	fputests.cpp \
	pipetests.cpp \
	porttests.cpp \
	sigtests.cpp \
	threadtests.cpp \
	vmtests.cpp

MY_LIBS := -lc -lnewos -lsupc++

include templates/app.mk

endif

