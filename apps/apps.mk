APPS_DIR = apps

APPS =
APPS_CLEAN =
APPS_INCLUDES = -Iinclude

APPS_LDSCRIPT = $(APPS_DIR)/ldscripts/$(ARCH)/app.ld

include $(APPS_DIR)/init/app.mk
include $(APPS_DIR)/shell/app.mk
include $(APPS_DIR)/testapp/app.mk

apps: $(APPS)

appsclean: $(APPS_CLEAN)

CLEAN += appsclean
