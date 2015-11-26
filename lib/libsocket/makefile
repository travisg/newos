# this file handles building libsocket.so

MY_TARGET := $(LIBS_BUILD_DIR)/libsocket.so
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_STATIC_TARGET := $(LIBS_BUILD_DIR)/libsocket.a

MY_TARGETDIR := $(LIBS_BUILD_DIR)/libsocket
MY_SRCDIR := $(LIBS_DIR)/libsocket
MY_SRCS := \
	socket.c

include templates/lib.mk

endif

