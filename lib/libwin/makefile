MY_TARGET := $(LIBS_BUILD_DIR)/libwin/libwin.so
MY_STATIC_TARGET := $(LIBS_BUILD_DIR)/libwin/libwin.a

MY_TARGETDIR := $(LIBS_BUILD_DIR)/libwin
MY_SRCDIR := $(LIBS_DIR)/libwin
MY_OBJS := Button.o \
	Canvas.o \
	Connection.o \
	Rect.o \
	Window.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := 
MY_LINKSCRIPT := $(LIBS_LDSCRIPT)

# include sub makefiles
#include $(addsuffix /makefile, $(addprefix $(MY_SRCDIR)/, \
#))

include templates/lib.mk


