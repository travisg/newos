GLUE_DIR = $(LIB_DIR)/glue
GLUE_OBJ_DIR = $(GLUE_DIR)/$(OBJ_DIR)
GLUE_OBJS = \
	$(GLUE_OBJ_DIR)/crt0.o

DEPS += $(GLUE_OBJS:.o=.d)

GLUE = $(GLUE_OBJ_DIR)/glue.o

$(GLUE): $(GLUE_OBJS)
	@mkdir -p $(GLUE_OBJ_DIR)
	$(LD) $(GLOBAL_LDFLAGS) -r -o $@ $(GLUE_OBJS)

LIBS += $(GLUE) 

glueclean:
	rm -f $(GLUE_OBJS) $(GLUE)

LIBS_CLEAN += glueclean 

# build prototypes
$(GLUE_OBJ_DIR)/%.o: $(GLUE_DIR)/%.c 
	@if [ ! -d $(GLUE_OBJ_DIR) ]; then mkdir -p $(GLUE_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

$(GLUE_OBJ_DIR)/%.d: $(GLUE_DIR)/%.c
	@if [ ! -d $(GLUE_OBJ_DIR) ]; then mkdir -p $(GLUE_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(GLUE_OBJ_DIR)/%.d: $(GLUE_DIR)/%.S
	@if [ ! -d $(GLUE_OBJ_DIR) ]; then mkdir -p $(GLUE_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(GLUE_OBJ_DIR)/%.o: $(GLUE_DIR)/%.S
	@if [ ! -d $(GLUE_OBJ_DIR) ]; then mkdir -p $(GLUE_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

