COMMON_DEV_DIR = $(DEV_DIR)/common
COMMON_DEV_OBJ_DIR = $(COMMON_DEV_DIR)/$(OBJ_DIR)
DEV_OBJS += \
	$(COMMON_DEV_OBJ_DIR)/null.o \
	$(COMMON_DEV_OBJ_DIR)/zero.o

DEV_SUB_INCLUDES += \
	-I$(COMMON_DEV_DIR)

$(COMMON_DEV_OBJ_DIR)/%.o: $(COMMON_DEV_DIR)/%.c
	@mkdir -p $(COMMON_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@

$(COMMON_DEV_OBJ_DIR)/%.d: $(COMMON_DEV_DIR)/%.c
	@mkdir -p $(COMMON_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(COMMON_DEV_OBJ_DIR)/%.d: $(COMMON_DEV_DIR)/%.S
	@mkdir -p $(COMMON_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(COMMON_DEV_OBJ_DIR)/%.o: $(COMMON_DEV_DIR)/%.S
	@mkdir -p $(COMMON_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@
