FALSEAPP_DIR = $(APPS_DIR)/false
FALSEAPP_OBJ_DIR = $(FALSEAPP_DIR)/$(OBJ_DIR)
FALSEAPP_OBJS = \
	$(FALSEAPP_OBJ_DIR)/main.o

DEPS += $(FALSEAPP_OBJS:.o=.d)

FALSEAPP = $(FALSEAPP_OBJ_DIR)/false

$(FALSEAPP): $(FALSEAPP_OBJS) $(LIBS) $(GLUE)
	$(LD) --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(GLUE) $(FALSEAPP_OBJS) $(LINK_LIBS) $(LIBGCC)

falseappclean:
	rm -f $(FALSEAPP_OBJS) $(FALSEAPP)

APPS += $(FALSEAPP)

APPS_CLEAN += falseappclean

$(FALSEAPP_OBJ_DIR)/%.o: $(FALSEAPP_DIR)/%.c
	@if [ ! -d $(FALSEAPP_OBJ_DIR) ]; then mkdir -p $(FALSEAPP_OBJ_DIR); fi
	@mkdir -p $(FALSEAPP_OBJ_DIR)
	$(CC) -c $< $(APPS_CFLAGS) $(APPS_INCLUDES) -o $@

$(FALSEAPP_OBJ_DIR)/%.d: $(FALSEAPP_DIR)/%.c
	@if [ ! -d $(FALSEAPP_OBJ_DIR) ]; then mkdir -p $(FALSEAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(APPS_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
