# i386 stage2 makefile
STAGE2_DIR = boot/$(ARCH)
STAGE2_OBJS = $(STAGE2_DIR)/stage2.o \
			  $(STAGE2_DIR)/smp_boot.o \
			  $(STAGE2_DIR)/smp_trampoline.o
STAGE2_DEPS = $(STAGE2_OBJS:.o=.d)

STAGE2 = boot/stage2
STAGE2_ARCH = boot/$(ARCH)/stage2

$(STAGE2): $(STAGE2_ARCH)
	ln -sf ../$< $@  

$(STAGE2_ARCH): $(STAGE2_OBJS) $(LIB)
	$(LD) -dN --script=$(STAGE2_DIR)/stage2.ld -L $(LIBGCC_PATH) $(STAGE2_OBJS) $(LIB) $(LIBGCC) -o $@

stage2clean:
	rm -f $(STAGE2_OBJS) $(STAGE2_DIR)/stage2

include $(STAGE2_DEPS)

# 
$(STAGE2_DIR)/%.o: $(STAGE2_DIR)/%.c 
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -I$(STAGE2_DIR) -o $@

$(STAGE2_DIR)/%.d: $(STAGE2_DIR)/%.c
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -I$(STAGE2_DIR) -M -MG $<) > $@

$(STAGE2_DIR)/%.d: $(STAGE2_DIR)/%.S
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -I$(STAGE2_DIR) -M -MG $<) > $@

$(STAGE2_DIR)/%.o: $(STAGE2_DIR)/%.S
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -I$(STAGE2_DIR) -o $@
