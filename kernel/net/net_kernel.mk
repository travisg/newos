KERNEL_NET_DIR = $(KERNEL_DIR)/net
KERNEL_NET_OBJ_DIR = $(KERNEL_NET_DIR)/$(OBJ_DIR)
KERNEL_OBJS += \
		$(KERNEL_NET_OBJ_DIR)/ethernet.o \
		$(KERNEL_NET_OBJ_DIR)/ipv4.o \
		$(KERNEL_NET_OBJ_DIR)/net.o 

KERNEL_NET_INCLUDES = $(KERNEL_INCLUDES)

$(KERNEL_NET_OBJ_DIR)/%.o: $(KERNEL_NET_DIR)/%.c
	@if [ ! -d $(KERNEL_NET_OBJ_DIR) ]; then mkdir -p $(KERNEL_NET_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_NET_INCLUDES) -o $@

$(KERNEL_NET_OBJ_DIR)/%.d: $(KERNEL_NET_DIR)/%.c
	@if [ ! -d $(KERNEL_NET_OBJ_DIR) ]; then mkdir -p $(KERNEL_NET_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_NET_INCLUDES) -M -MG $<) > $@
