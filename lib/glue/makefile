# this file handles building glue.o and libglue.o

MY_TARGET := $(LIBS_BUILD_DIR)/glue.o
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_TARGETDIR := $(LIBS_BUILD_DIR)/glue
MY_SRCDIR := $(LIBS_DIR)/glue
MY_OBJS := \
	crt0.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)

include templates/combined_object.mk

endif

MY_TARGET := $(LIBS_BUILD_DIR)/libglue.o
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_TARGETDIR := $(LIBS_BUILD_DIR)/glue
MY_SRCDIR := $(LIBS_DIR)/glue
MY_OBJS := \
	lib0.o

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)

include templates/combined_object.mk

endif

