ifneq ($(_LIB_MAKE),1)
_LIB_MAKE = 1

LIB_DIR = lib

LIBS = 
LINK_LIBS =
KLIBS =
LINK_KLIBS = 
LIBS_CLEAN =

include $(LIB_DIR)/libsys/libsys.mk
include $(LIB_DIR)/libc/libc.mk
include $(LIB_DIR)/glue/glue.mk

libs: $(LIBS)

klibs: $(KLIBS)

libclean: $(LIBS_CLEAN)

CLEAN += libclean

endif
