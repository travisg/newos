
TOOLS_DIR := tools

BOOTMAKER := $(TOOLS_DIR)/bootmaker
NETBOOT := $(TOOLS_DIR)/netboot
BIN2H := $(TOOLS_DIR)/bin2h
BIN2ASM := $(TOOLS_DIR)/bin2asm

TOOLS := \
	$(BOOTMAKER) \
	$(NETBOOT) \
	$(BIN2H) \
	$(BIN2ASM)

NETBOOT_LINK_ARGS =
ifeq ($(OSTYPE),beos)
    NETBOOT_LINK_ARGS = -lnet
endif
ifeq ($(shell uname),SunOS)
    NETBOOT_LINK_ARGS = -lsocket -lnsl
endif

tools: $(TOOLS)

$(NETBOOT): $(NETBOOT).c
	$(HOST_CC) -O2 -o $@ $(NETBOOT).c $(NETBOOT_LINK_ARGS)

$(BIN2ASM): $(BIN2ASM).c
	$(HOST_CC) -O2 -o $@ $(BIN2ASM).c

$(BIN2H): $(BIN2H).c
	$(HOST_CC) -O2 -o $@ $(BIN2H).c

$(BOOTMAKER): $(BOOTMAKER).c tools/sparcbootblock.h
	$(HOST_CC) -O2 -o $@ $(BOOTMAKER).c

toolsclean:
	rm -f $(TOOLS)

CLEAN += toolsclean
