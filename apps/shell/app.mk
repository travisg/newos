SHELLAPP_DIR = $(APPS_DIR)/shell
SHELLAPP_OBJ_DIR = $(SHELLAPP_DIR)/$(OBJ_DIR)
SHELLAPP_OBJS = \
	$(SHELLAPP_OBJ_DIR)/commands.o \
	$(SHELLAPP_OBJ_DIR)/main.o

DEPS += $(SHELLAPP_OBJS:.o=.d)

SHELLAPP = $(SHELLAPP_OBJ_DIR)/shell

$(SHELLAPP): $(SHELLAPP_OBJS) $(LIBS) $(GLUE)
	$(LD) -dN --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(GLUE) $(SHELLAPP_OBJS) $(LINK_LIBS) $(LIBGCC)

shellappclean:
	rm -f $(SHELLAPP_OBJS) $(SHELLAPP)

APPS += $(SHELLAPP)

APPS_CLEAN += shellappclean

$(SHELLAPP_OBJ_DIR)/%.o: $(SHELLAPP_DIR)/%.c
	@if [ ! -d $(SHELLAPP_OBJ_DIR) ]; then mkdir -p $(SHELLAPP_OBJ_DIR); fi
	@mkdir -p $(SHELLAPP_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -o $@

$(SHELLAPP_OBJ_DIR)/%.d: $(SHELLAPP_DIR)/%.c
	@if [ ! -d $(SHELLAPP_OBJ_DIR) ]; then mkdir -p $(SHELLAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
