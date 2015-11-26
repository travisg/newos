MY_TARGET := $(LIBS_BUILD_DIR)/libwin.so
MY_STATIC_TARGET := $(LIBS_BUILD_DIR)/libwin.a

MY_TARGETDIR := $(LIBS_BUILD_DIR)/libwin
MY_SRCDIR := $(LIBS_DIR)/libwin
MY_SRCS := \
	Button.cpp \
	Canvas.cpp \
	Connection.cpp \
	Rect.cpp \
	Window.cpp

# include sub makefiles
#include $(addsuffix /makefile, $(addprefix $(MY_SRCDIR)/, \
#))

include templates/lib.mk


