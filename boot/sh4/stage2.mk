# sh4 stage2 makefile
STAGE2_DIR = boot/$(ARCH)
STAGE2_OBJS = $(STAGE2_DIR)/stage2.o \
	$(STAGE2_DIR)/serial.o \
	$(STAGE2_DIR)/mmu.o \
	$(STAGE2_DIR)/asm.o
DEPS += $(STAGE2_OBJS:.o=.d)

STAGE2 = boot/stage2
STAGE2_ARCH = boot/$(ARCH)/stage2

$(STAGE2): $(STAGE2_ARCH)
	ln -sf ../$< $@  

$(STAGE2_ARCH).elf: $(STAGE2_OBJS) $(LIBC)
	$(LD) $(GLOBAL_LDFLAGS) -dN -Ttext=0x8c001200 -L $(LIBGCC_PATH) $(STAGE2_OBJS) $(LIBC) $(LIBGCC) -o $@ 

$(STAGE2_ARCH): $(STAGE2_ARCH).elf
	$(OBJCOPY) -O binary $< $@	

stage2clean:
	rm -f $(STAGE2_OBJS) $(STAGE2_ARCH) $(STAGE2_ARCH).elf

# 
$(STAGE2_DIR)/%.o: $(STAGE2_DIR)/%.c 
	$(CC) $(GLOBAL_CFLAGS) -Iinclude -I$(STAGE2_DIR) -c $< -o $@

$(STAGE2_DIR)/%.d: $(STAGE2_DIR)/%.c
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -I$(STAGE2_DIR) -M -MG $<) > $@

$(STAGE2_DIR)/%.d: $(STAGE2_DIR)/%.S
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -I$(STAGE2_DIR) -M -MG $<) > $@

$(STAGE2_DIR)/%.o: $(STAGE2_DIR)/%.S
	$(CC) $(GLOBAL_CFLAGS) -Iinclude -I$(STAGE2_DIR) -c $< -o $@
