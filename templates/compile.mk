# do not use directly, is included by other templates

# make sure the MY_COMPILEFLAGS_IN, MY_CFLAGS_IN, MY_CPPFLAGS_IN, and MY_INCLUDES_IN vars are resolved when the rule is made
$(MY_COBJS_IN) $(MY_CPPOBJS_IN) $(MY_ASMOBJS_IN): MY_COMPILEFLAGS_IN:=$(MY_COMPILEFLAGS_IN)
$(MY_COBJS_IN) $(MY_CPPOBJS_IN) $(MY_ASMOBJS_IN): MY_CFLAGS_IN:=$(MY_CFLAGS_IN)
$(MY_COBJS_IN) $(MY_CPPOBJS_IN) $(MY_ASMOBJS_IN): MY_CPPFLAGS_IN:=$(MY_CPPFLAGS_IN)
$(MY_COBJS_IN) $(MY_CPPOBJS_IN) $(MY_ASMOBJS_IN): MY_INCLUDES_IN:=$(MY_INCLUDES_IN)

# build rules

# empty rule for dependency files, they are built by the compile process
$(MY_TARGETDIR_IN)/%.d:

#$(warning MY_OBJS_IN = $(MY_OBJS_IN))

$(MY_CPPOBJS_IN): $(MY_TARGETDIR_IN)/%.o: $(MY_SRCDIR_IN)/%.cpp
	@$(MKDIR)
	@echo compiling $<
	$(NOECHO)$(CC) -c $< $(GLOBAL_COMPILEFLAGS) $(GLOBAL_CPPFLAGS) $(MY_COMPILEFLAGS_IN) $(MY_CPPFLAGS_IN) $(MY_INCLUDES_IN) -MD -MT $@ -MF $(@:%o=%d) -o $@

$(MY_COBJS_IN): $(MY_TARGETDIR_IN)/%.o: $(MY_SRCDIR_IN)/%.c
	@$(MKDIR)
	@echo compiling $<
	$(NOECHO)$(CC) -c $< $(GLOBAL_COMPILEFLAGS) $(GLOBAL_CFLAGS) $(MY_COMPILEFLAGS_IN) $(MY_CFLAGS_IN) $(MY_INCLUDES_IN) -MD -MT $@ -MF $(@:%o=%d) -o $@

$(MY_ASMOBJS_IN): $(MY_TARGETDIR_IN)/%.o: $(MY_SRCDIR_IN)/%.S
	@$(MKDIR)
	@echo assembling $<
	$(NOECHO)$(CC) -c $< $(GLOBAL_COMPILEFLAGS) $(GLOBAL_ASFLAGS) $(GLOBAL_CFLAGS) $(MY_COMPILEFLAGS_IN) $(MY_CFLAGS_IN) $(MY_INCLUDES_IN) -MD -MT $@ -MF $(@:%o=%d) -o $@
