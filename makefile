
# figures out the system
include make.syscfg

# include top level macros
include macros.mk

# sub-makefiles have to fill these in
ALL_DEPS =
CLEAN =
FINAL =
TOOLS =
ALL_OBJS =

# includes the config of the build
include make.config
include make.config.$(ARCH)

final: $(FINAL)

include apps/makefile
include kernel/makefile
include kernel/addons/makefile
include tools/makefile
include lib/makefile
include boot/$(ARCH)/makefile

clean: $(CLEAN)
	rm -f $(ALL_OBJS)
	rm -f $(ALL)
	rm -f $(FINAL)

depsclean:
	rm -f $(ALL_DEPS)

appsclean:
	rm -rf $(APPS_BUILD_DIR)

allclean: depsclean clean
	rm -f $(TOOLS)

spotless:
	rm -rf $(BUILD_DIR)

#$(warning ALL_OBJS = $(ALL_OBJS))
#$(warning ALL_DEPS = $(ALL_DEPS))

ifeq ($(filter $(MAKECMDGOALS), allclean), )
-include $(ALL_DEPS)
endif

