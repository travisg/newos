ifneq ($(_LIB_MAKE),1)
_LIB_MAKE = 1

LIB_DIR = lib

LIBS = 
LINK_LIBS =
KLIBS =
LINK_KLIBS = 
LIBS_CLEAN =

LIBS_LDSCRIPT = $(LIB_DIR)/ldscripts/$(ARCH)/library.ld

include $(LIB_DIR)/glue/glue.mk
include $(LIB_DIR)/glue/libglue.mk
#include $(LIB_DIR)/libc/libc.mk
include $(LIB_DIR)/nulibc/nulibc.mk
include $(LIB_DIR)/libm/libm.mk

libs: $(LIBS)

klibs: $(KLIBS)

libclean: $(LIBS_CLEAN)

CLEAN += libclean

endif
