ifneq ($(_BUS_MAKE),1)
_BUS_MAKE = 1

BUS_DIR = bus
BUS_OBJ_DIR = $(BUS_DIR)/$(OBJ_DIR)
BUS_OBJS = \
	$(BUS_OBJ_DIR)/bus_init.o \
	$(BUS_OBJ_DIR)/bus_man.o 

BUS_INCLUDES = -Iinclude
BUS_SUB_INCLUDES =

DEPS += $(BUS_OBJS:.o=.d)

BUS = $(BUS_OBJ_DIR)/bus.o

include $(BUS_DIR)/bus_$(ARCH).mk

$(BUS): $(BUS_OBJS)
	$(LD) -r -o $@ $(BUS_OBJS)

busses: $(BUS)

busclean:
	rm -f $(BUS_OBJS) $(BUS)

CLEAN += busclean

# build prototypes
$(BUS_OBJ_DIR)/%.o: $(BUS_DIR)/%.c
	@if [ ! -d $(BUS_OBJ_DIR) ]; then mkdir -p $(BUS_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(BUS_INCLUDES) $(BUS_SUB_INCLUDES) -o $@

$(BUS_OBJ_DIR)/%.d: $(BUS_DIR)/%.c
	@if [ ! -d $(BUS_OBJ_DIR) ]; then mkdir -p $(BUS_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(BUS_INCLUDES) $(BUS_SUB_INCLUDES) -M -MG $<) > $@

$(BUS_OBJ_DIR)/%.d: $(BUS_DIR)/%.S
	@if [ ! -d $(BUS_OBJ_DIR) ]; then mkdir -p $(BUS_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(BUS_INCLUDES) $(BUS_SUB_INCLUDES) -M -MG $<) > $@

$(BUS_OBJ_DIR)/%.o: $(BUS_DIR)/%.S
	@if [ ! -d $(BUS_OBJ_DIR) ]; then mkdir -p $(BUS_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(BUS_INCLUDES) $(BUS_SUB_INCLUDES) -o $@
endif
