# this file handles building glue.o and libglue.o

MY_TARGET := $(LIBS_BUILD_DIR)/glue.o
MY_TARGETDIR := $(LIBS_BUILD_DIR)/glue
MY_SRCDIR := $(LIBS_DIR)/glue
MY_SRCS := \
	crt0.c

MY_INCLUDES := $(STDINCLUDE)
MY_COMPILEFLAGS := $(USER_COMPILEFLAGS)

include templates/combined_object.mk

MY_TARGET := $(LIBS_BUILD_DIR)/libglue.o

MY_TARGETDIR := $(LIBS_BUILD_DIR)/glue
MY_SRCDIR := $(LIBS_DIR)/glue
MY_SRCS := \
	lib0.c

MY_INCLUDES := $(STDINCLUDE)
MY_COMPILEFLAGS := $(USER_COMPILEFLAGS)

include templates/combined_object.mk

