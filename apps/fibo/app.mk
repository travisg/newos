FIBOAPP_DIR = $(APPS_DIR)/fibo
FIBOAPP_OBJ_DIR = $(FIBOAPP_DIR)/$(OBJ_DIR)
FIBOAPP_OBJS = \
	$(FIBOAPP_OBJ_DIR)/main.o

DEPS += $(FIBOAPP_OBJS:.o=.d)

FIBOAPP = $(FIBOAPP_OBJ_DIR)/fibo

$(FIBOAPP): $(FIBOAPP_OBJS) $(LIBS) $(GLUE)
	$(LD) -dN --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(GLUE) $(FIBOAPP_OBJS) $(LINK_LIBS) $(LIBGCC)

fiboappclean:
	rm -f $(FIBOAPP_OBJS) $(FIBOAPP)

APPS += $(FIBOAPP)

APPS_CLEAN += fiboappclean

$(FIBOAPP_OBJ_DIR)/%.o: $(FIBOAPP_DIR)/%.c
	@if [ ! -d $(FIBOAPP_OBJ_DIR) ]; then mkdir -p $(FIBOAPP_OBJ_DIR); fi
	@mkdir -p $(FIBOAPP_OBJ_DIR)
	$(CC) -c $< $(APPS_CFLAGS) $(APPS_INCLUDES) -o $@

$(FIBOAPP_OBJ_DIR)/%.d: $(FIBOAPP_DIR)/%.c
	@if [ ! -d $(FIBOAPP_OBJ_DIR) ]; then mkdir -p $(FIBOAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(APPS_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
