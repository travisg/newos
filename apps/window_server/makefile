# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/window_server
MY_SRCDIR := $(APPS_DIR)/window_server
MY_TARGET :=  $(MY_TARGETDIR)/window_server
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_OBJS := \
	main.o \
	PS2Mouse.o \
	Renderer_16bpp.o \
	Renderer_vesa.o \
	Region.o \
	GraphicsContext.o \
	Window.o \
	WindowManager.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := -lc -lwin -lnewos -lsupc++
MY_LIBPATHS :=
MY_DEPS :=
MY_GLUE := $(APPSGLUE)

include templates/app.mk

endif

