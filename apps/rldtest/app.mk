RLDTESTAPP_DIR = $(APPS_DIR)/rldtest
RLDTESTAPP_OBJ_DIR = $(RLDTESTAPP_DIR)/$(OBJ_DIR)
RLDTESTAPP_OBJS = \
	$(RLDTESTAPP_OBJ_DIR)/rldtest.o
RLDTESTLIB_OBJS = \
	$(RLDTESTAPP_OBJ_DIR)/shared.o

DEPS += $(RLDTESTAPP_OBJS:.o=.d)

RLDTESTAPP = $(RLDTESTAPP_OBJ_DIR)/rldtest
RLDTESTLIB = $(RLDTESTAPP_OBJ_DIR)/librldtest.so

$(RLDTESTAPP): $(RLDTESTAPP_OBJS) $(LIBS) $(GLUE) $(RLDTESTLIB)
	$(LD) -shared -dynamic -d --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -L$(RLDTESTAPP_OBJ_DIR) -o $@ $(GLUE) $(RLDTESTAPP_OBJS) -lrldtest $(LINK_LIBS) $(LIBGCC)
$(RLDTESTLIB): $(RLDTESTLIB_OBJS) $(LIBS) $(GLUE)
	$(LD) -shared -dynamic -d --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(RLDTESTLIB_OBJS) 

rldtestappclean:
	rm -f $(RLDTESTAPP_OBJS) $(RLDTESTAPP) $(RLDTESTLIB)

APPS += $(RLDTESTAPP) $(RLDTESTLIB)

APPS_CLEAN += rldtestappclean

$(RLDTESTAPP_OBJ_DIR)/%.o: $(RLDTESTAPP_DIR)/%.c
	@if [ ! -d $(RLDTESTAPP_OBJ_DIR) ]; then mkdir -p $(RLDTESTAPP_OBJ_DIR); fi
	@mkdir -p $(RLDTESTAPP_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) -Wunused $(APPS_INCLUDES) -o $@

$(RLDTESTAPP_OBJ_DIR)/%.d: $(RLDTESTAPP_DIR)/%.c
	@if [ ! -d $(RLDTESTAPP_OBJ_DIR) ]; then mkdir -p $(RLDTESTAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@

$(RLDTESTAPP_OBJ_DIR)/%.d: $(RLDTESTAPP_DIR)/%.S
	@mkdir -p $(RLDTESTAPP_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@

$(RLDTESTAPP_OBJ_DIR)/%.o: $(RLDTESTAPP_DIR)/%.S
	@mkdir -p $(RLDTESTAPP_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -o $@
