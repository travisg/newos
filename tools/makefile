TOOLS_SRC_DIR := tools
TOOLS_BUILD_DIR := $(call TOBUILDDIR, tools)

BOOTMAKER := $(TOOLS_BUILD_DIR)/bootmaker
NETBOOT := $(TOOLS_BUILD_DIR)/netboot
BIN2H := $(TOOLS_BUILD_DIR)/bin2h
BIN2ASM := $(TOOLS_BUILD_DIR)/bin2asm

BOOTMAKERSRC := $(TOOLS_SRC_DIR)/bootmaker.c
NETBOOTSRC := $(TOOLS_SRC_DIR)/netboot.c
BIN2HSRC := $(TOOLS_SRC_DIR)/bin2h.c
BIN2ASMSRC := $(TOOLS_SRC_DIR)/bin2asm.c

TOOLS := \
	$(BOOTMAKER) \
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

$(NETBOOT): $(NETBOOTSRC)
	@$(MKDIR)
	$(HOST_CC) -O2 -o $@ $(NETBOOTSRC) $(NETBOOT_LINK_ARGS)

$(BIN2ASM): $(BIN2ASMSRC)
	@$(MKDIR)
	$(HOST_CC) -O2 -o $@ $(BIN2ASMSRC)

$(BIN2H): $(BIN2HSRC)
	@$(MKDIR)
	$(HOST_CC) -O2 -o $@ $(BIN2HSRC)

$(BOOTMAKER): $(BOOTMAKERSRC) tools/sparcbootblock.h
	@$(MKDIR)
	$(HOST_CC) -O2 -o $@ $(BOOTMAKERSRC)

toolsclean:
	rm -f $(TOOLS)

CLEAN += toolsclean
