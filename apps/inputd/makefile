# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/inputd
MY_SRCDIR := $(APPS_DIR)/inputd
MY_TARGET :=  $(MY_TARGETDIR)/inputd
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	InputDevice.o \
	InputServer.o \
	KeyboardDevice.o \
	main.o \
	PS2Device.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := -lc -lnewos -lsupc++
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif

