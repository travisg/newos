# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/static
MY_SRCDIR := $(APPS_DIR)/static
MY_TARGET :=  $(MY_TARGETDIR)/static
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	main.o

MY_INCLUDES := -Iinclude -Iinclude/nulibc
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := $(LIBC_STATIC) $(APPS_BUILD_DIR)/rldtest/girlfriend.so
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif

