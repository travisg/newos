LIB_DIR = lib

LIBS = 
LIBS_CLEAN =

include $(LIB_DIR)/libc/libc.mk

libs: $(LIBS)

libclean: $(LIBS_CLEAN)
