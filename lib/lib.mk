LIB_DIR = lib
LIB_OBJS = \
	$(LIB_DIR)/string.o \
	$(LIB_DIR)/vsprintf.o \
	$(LIB_DIR)/ctype.o 

LIB_DEPS = $(LIB_OBJS:.o=.d)

LIB = $(LIB_DIR)/lib.a

$(LIB): $(LIB_OBJS)
	$(AR) r $@ $(LIB_OBJS)

libclean:
	rm -f $(LIB_OBJS) $(LIB)

include $(LIB_DEPS)

# build prototypes
$(LIB_DIR)/%.o: $(LIB_DIR)/%.c 
	$(CC) $(GLOBAL_CFLAGS) -Iinclude -c $< -o $@

$(LIB_DIR)/%.d: $(LIB_DIR)/%.c
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIB_DIR)/%.d: $(LIB_DIR)/%.S
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIB_DIR)/%.o: $(LIB_DIR)/%.S
	$(CC) $(GLOBAL_CFLAGS) -Iinclude -c $< -o $@

