LIB_DIR = lib

LIBS = 
KLIBS = 
LIBS_CLEAN =

include $(LIB_DIR)/libc/libc.mk
include $(LIB_DIR)/libsys/libsys.mk

libs: $(LIBS)

klibs: $(KLIBS)

libclean: $(LIBS_CLEAN)

CLEAN += libclean
