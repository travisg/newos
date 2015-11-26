# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/static
MY_SRCDIR := $(APPS_DIR)/static
MY_TARGET :=  $(MY_TARGETDIR)/static
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.c

MY_INCLUDES := -Iinclude/nulibc
MY_LIBS := $(LIBC_STATIC) $(LIBNEWOS_STATIC) $(APPS_BUILD_DIR)/rldtest/girlfriend.so

include templates/app.mk

endif

