COMMON_DEV_DIR = $(DEV_DIR)/common
COMMON_DEV_OBJ_DIR = $(COMMON_DEV_DIR)/$(OBJ_DIR)
DEV_OBJS += \
	$(COMMON_DEV_OBJ_DIR)/null.o \
	$(COMMON_DEV_OBJ_DIR)/zero.o

DEV_SUB_INCLUDES += \
	-I$(COMMON_DEV_DIR)

$(COMMON_DEV_OBJ_DIR)/%.o: $(COMMON_DEV_DIR)/%.c
	@if [ ! -d $(COMMON_DEV_OBJ_DIR) ]; then mkdir -p $(COMMON_DEV_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@

$(COMMON_DEV_OBJ_DIR)/%.d: $(COMMON_DEV_DIR)/%.c
	@if [ ! -d $(COMMON_DEV_OBJ_DIR) ]; then mkdir -p $(COMMON_DEV_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(COMMON_DEV_OBJ_DIR)/%.d: $(COMMON_DEV_DIR)/%.S
	@if [ ! -d $(COMMON_DEV_OBJ_DIR) ]; then mkdir -p $(COMMON_DEV_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(COMMON_DEV_OBJ_DIR)/%.o: $(COMMON_DEV_DIR)/%.S
	@if [ ! -d $(COMMON_DEV_OBJ_DIR) ]; then mkdir -p $(COMMON_DEV_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@
