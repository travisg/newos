APPS_DIR = apps

APPS =
APPS_CLEAN =

APPS_LDSCRIPT = $(APPS_DIR)/ldscripts/$(ARCH)/app.ld

include $(APPS_DIR)/testapp/app.mk

apps: $(APPS)

appsclean: $(APPS_CLEAN)

CLEAN += appsclean

# build prototypes
APPS_INCLUDES = -Iinclude

$(APPS_DIR)/%.o: $(APPS_DIR)/%.c
	$(CC) -c $< $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -o $@

$(APPS_DIR)/%.d: $(APPS_DIR)/%.c
	@echo "making deps for $<..."
	@(echo -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
