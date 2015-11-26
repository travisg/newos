# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/window_server
MY_SRCDIR := $(APPS_DIR)/window_server
MY_TARGET :=  $(MY_TARGETDIR)/window_server
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	main.cpp \
	PS2Mouse.cpp \
	Renderer_16bpp.cpp \
	Renderer_vesa.cpp \
	Region.cpp \
	GraphicsContext.cpp \
	Window.cpp \
	WindowManager.cpp

MY_LIBS := -lc -lwin -lnewos -lsupc++

include templates/app.mk

endif

