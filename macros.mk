
# prepends the BUILD_DIR var to each item in the list
TOBUILDDIR = $(addprefix $(BUILD_DIR)/,$(1))

# sees if the target is in the list
FINDINLIST = $(if $(findstring $(1),$(2)),1,0)

# used in template files to build a directory in the build rules
MKDIR = if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi

# gets the local directory of the makefile. Requires gmake 3.80
GET_LOCAL_DIR    = $(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))
