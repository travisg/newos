LIBGLUE_DIR = $(LIB_DIR)/glue
LIBGLUE_OBJ_DIR = $(LIBGLUE_DIR)/$(OBJ_DIR)
LIBGLUE_OBJS = \
	$(LIBGLUE_OBJ_DIR)/lib0.o

DEPS += $(LIBGLUE_OBJS:.o=.d)

LIBGLUE = $(LIBGLUE_OBJ_DIR)/libglue.o

$(LIBGLUE): $(LIBGLUE_OBJS)
	@mkdir -p $(LIBGLUE_OBJ_DIR)
	$(LD) $(GLOBAL_LDFLAGS) -r -o $@ $(LIBGLUE_OBJS)

LIBS += $(LIBGLUE) 

libglueclean:
	rm -f $(LIBGLUE_OBJS) $(LIBGLUE)

LIBS_CLEAN += libglueclean 

# build prototypes
$(LIBGLUE_OBJ_DIR)/%.o: $(LIBGLUE_DIR)/%.c 
	@if [ ! -d $(LIBGLUE_OBJ_DIR) ]; then mkdir -p $(LIBGLUE_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(LIBGLUE_OBJ_DIR)/%.d: $(LIBGLUE_DIR)/%.c
	@if [ ! -d $(LIBGLUE_OBJ_DIR) ]; then mkdir -p $(LIBGLUE_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(LIBGLUE_OBJ_DIR)/%.d: $(LIBGLUE_DIR)/%.S
	@if [ ! -d $(LIBGLUE_OBJ_DIR) ]; then mkdir -p $(LIBGLUE_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(LIBGLUE_OBJ_DIR)/%.o: $(LIBGLUE_DIR)/%.S
	@if [ ! -d $(LIBGLUE_OBJ_DIR) ]; then mkdir -p $(LIBGLUE_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

