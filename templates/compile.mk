# do not use directly, is included by other templates

define MKDIR
	@if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi
endef
define DEPSMSG
	@echo "making deps for $<..."
endef

# make sure the MY_CFLAGS_IN and MY_INCLUDES_IN vars are resolved when the rule is made
$(MY_TARGETDIR_IN)/%.o: MY_CFLAGS_IN:=$(MY_CFLAGS_IN)
$(MY_TARGETDIR_IN)/%.o: MY_INCLUDES_IN:=$(MY_INCLUDES_IN)

$(MY_TARGETDIR_IN)/%.d: MY_CFLAGS_IN:=$(MY_CFLAGS_IN)
$(MY_TARGETDIR_IN)/%.d: MY_INCLUDES_IN:=$(MY_INCLUDES_IN)

# build rules

$(MY_TARGETDIR_IN)/%.o: $(MY_SRCDIR_IN)/%.cpp 
	$(MKDIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(MY_CFLAGS_IN) $(MY_INCLUDES_IN) -o $@

$(MY_TARGETDIR_IN)/%.d: $(MY_SRCDIR_IN)/%.cpp
	$(MKDIR)
	$(DEPSMSG)
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(MY_CFLAGS_IN) $(MY_INCLUDES_IN) -M -MG $<) > $@

$(MY_TARGETDIR_IN)/%.o: $(MY_SRCDIR_IN)/%.c
	$(MKDIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(MY_CFLAGS_IN) $(MY_INCLUDES_IN) -o $@

$(MY_TARGETDIR_IN)/%.d: $(MY_SRCDIR_IN)/%.c
	$(MKDIR)
	$(DEPSMSG)
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(MY_CFLAGS_IN) $(MY_INCLUDES_IN) -M -MG $<) > $@

$(MY_TARGETDIR_IN)/%.o: $(MY_SRCDIR_IN)/%.S
	$(MKDIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(MY_CFLAGS_IN) $(MY_INCLUDES_IN) -o $@

$(MY_TARGETDIR_IN)/%.d: $(MY_SRCDIR_IN)/%.S
	$(MKDIR)
	$(DEPSMSG)
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(MY_CFLAGS_IN) $(MY_INCLUDES_IN) -M -MG $<) > $@

