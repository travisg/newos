FORTUNEAPP_DIR = $(APPS_DIR)/fortune
FORTUNEAPP_OBJ_DIR = $(FORTUNEAPP_DIR)/$(OBJ_DIR)
FORTUNEAPP_OBJS = \
	$(FORTUNEAPP_OBJ_DIR)/main.o

DEPS += $(FORTUNEAPP_OBJS:.o=.d)

FORTUNEAPP = $(FORTUNEAPP_OBJ_DIR)/fortune

$(FORTUNEAPP): $(FORTUNEAPP_OBJS) $(LIBS) $(GLUE)
	$(LD) --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(GLUE) $(FORTUNEAPP_OBJS) $(LINK_LIBS) $(LIBGCC)

fortuneappclean:
	rm -f $(FORTUNEAPP_OBJS) $(FORTUNEAPP)

APPS += $(FORTUNEAPP)

APPS_CLEAN += fortuneappclean

$(FORTUNEAPP_OBJ_DIR)/%.o: $(FORTUNEAPP_DIR)/%.c
	@if [ ! -d $(FORTUNEAPP_OBJ_DIR) ]; then mkdir -p $(FORTUNEAPP_OBJ_DIR); fi
	@mkdir -p $(FORTUNEAPP_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -o $@

$(FORTUNEAPP_OBJ_DIR)/%.d: $(FORTUNEAPP_DIR)/%.c
	@if [ ! -d $(FORTUNEAPP_OBJ_DIR) ]; then mkdir -p $(FORTUNEAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
