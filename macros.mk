
# prepends the BUILD_DIR var to each item in the list
TOBUILDDIR = $(addprefix $(BUILD_DIR)/,$(1))

# sees if the target is in the list
FINDINLIST = $(if $(findstring $(1),$(2)),1,0)

