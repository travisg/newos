# do not use directly, is included by other templates

# make sure the MY_CFLAGS_IN, MY_CPPFLAGS_IN, and MY_INCLUDES_IN vars are resolved when the rule is made
$(MY_TARGETDIR_IN)/%.o: MY_CFLAGS_IN:=$(MY_CFLAGS_IN)
$(MY_TARGETDIR_IN)/%.o: MY_CPPFLAGS_IN:=$(MY_CPPFLAGS_IN)
$(MY_TARGETDIR_IN)/%.o: MY_INCLUDES_IN:=$(MY_INCLUDES_IN)

# build rules

# empty rule for dependency files, they are built by the compile process
$(MY_TARGETDIR_IN)/%.d:

$(MY_TARGETDIR_IN)/%.o: $(MY_SRCDIR_IN)/%.cpp 
	@$(MKDIR)
	@echo compiling $<
	@$(CC) -c $< $(GLOBAL_CPPFLAGS) $(MY_CPPFLAGS_IN) $(MY_INCLUDES_IN) -MD -MT $@ -MF $(@:%o=%d) -o $@

$(MY_TARGETDIR_IN)/%.o: $(MY_SRCDIR_IN)/%.c
	@$(MKDIR)
	@echo compiling $<
	@$(CC) -c $< $(GLOBAL_CFLAGS) $(MY_CFLAGS_IN) $(MY_INCLUDES_IN) -MD -MT $@ -MF $(@:%o=%d) -o $@

$(MY_TARGETDIR_IN)/%.o: $(MY_SRCDIR_IN)/%.S
	@$(MKDIR)
	@echo assembling $<
	@$(CC) -c $< $(GLOBAL_ASFLAGS) $(GLOBAL_CFLAGS) $(MY_CFLAGS_IN) $(MY_INCLUDES_IN) -MD -MT $@ -MF $(@:%o=%d) -o $@
