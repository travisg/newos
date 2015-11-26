# app makefile
LOCAL_DIR := $(GET_LOCAL_DIR)
MY_SRCDIR := $(LOCAL_DIR)
MY_TARGETDIR := $(APPS_BUILD_DIR)/rldtest
MY_TARGET :=  $(MY_TARGETDIR)/rldtest

MY_SRCS := \
	rldtest.c

MY_LIBS := $(MY_TARGETDIR)/librldtest.so -lc -lnewos -lsupc++
MY_DEPS := $(MY_TARGETDIR)/librldtest.so $(MY_TARGETDIR)/girlfriend.so

include templates/app.mk

# build the librldtest.so object
MY_SRCDIR := $(LOCAL_DIR)
MY_TARGETDIR := $(APPS_BUILD_DIR)/rldtest
MY_TARGET := $(MY_TARGETDIR)/librldtest.so

MY_SRCS := \
	shared.c

include templates/lib.mk

# build the girlfriend.so object
MY_SRCDIR := $(LOCAL_DIR)
MY_TARGETDIR := $(APPS_BUILD_DIR)/rldtest
MY_TARGET := $(MY_TARGETDIR)/girlfriend.so

MY_SRCS := \
	girlfriend.c

include templates/lib.mk

