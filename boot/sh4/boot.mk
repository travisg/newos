ifneq ($(_BOOT_MAKE),1)
_BOOT_MAKE = 1

# include targets we depend on
include lib/lib.mk
include kernel/kernel.mk
include apps/apps.mk

# sh4 stage2 makefile
BOOT_DIR = boot/$(ARCH)
BOOT_OBJ_DIR = $(BOOT_DIR)/$(OBJ_DIR)
STAGE2_OBJS = $(BOOT_OBJ_DIR)/stage2.o \
	$(BOOT_OBJ_DIR)/serial.o \
	$(BOOT_OBJ_DIR)/mmu.o \
	$(BOOT_OBJ_DIR)/vcpu.o \
	$(BOOT_OBJ_DIR)/vcpu_c.o
DEPS += $(STAGE2_OBJS:.o=.d)

STAGE2 = $(BOOT_OBJ_DIR)/stage2

$(STAGE2): $(STAGE2_OBJS) $(LIBC)
	$(LD) $(GLOBAL_LDFLAGS) -dN --script=$(BOOT_DIR)/stage2.ld -L $(LIBGCC_PATH) $(STAGE2_OBJS) $(LIBC) $(LIBGCC) -o $@ 

stage2: $(STAGE2)

stage2clean:
	rm -f $(STAGE2_OBJS) $(STAGE2)

CLEAN += stage2clean

FINAL = $(BOOT_DIR)/final

$(FINAL): $(STAGE2) $(KERNEL) $(APPS) tools
	$(BOOTMAKER) $(BOOT_DIR)/config.ini --sh4 -o $(FINAL)

# 
$(BOOT_OBJ_DIR)/%.o: $(BOOT_DIR)/%.c
	@mkdir -p $(BOOT_OBJ_DIR)
	$(CC) $(GLOBAL_CFLAGS) -Iinclude -I$(BOOT_DIR) -c $< -o $@

$(BOOT_OBJ_DIR)/%.d: $(BOOT_DIR)/%.c
	@mkdir -p $(BOOT_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -I$(BOOT_DIR) -M -MG $<) > $@

$(BOOT_OBJ_DIR)/%.d: $(BOOT_DIR)/%.S
	@mkdir -p $(BOOT_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -I$(BOOT_DIR) -M -MG $<) > $@

$(BOOT_OBJ_DIR)/%.o: $(BOOT_DIR)/%.S
	@mkdir -p $(BOOT_OBJ_DIR)
	$(CC) $(GLOBAL_CFLAGS) -Iinclude -I$(BOOT_DIR) -c $< -o $@
endif
