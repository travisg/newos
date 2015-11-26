# vim: syntax=make

# prepends the BUILD_DIR var to each item in the list
TOBUILDDIR = $(addprefix $(BUILD_DIR)/,$(1))

# sees if the target is in the list
FINDINLIST = $(if $(findstring $(1),$(2)),1,0)

# used in template files to build a directory in the build rules
MKDIR = if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi

# gets the local directory of the makefile. Requires gmake 3.80
GET_LOCAL_DIR  = $(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))

COMMA := ,
SPACE :=
SPACE +=

# test if two files are different, replacing the first
# with the second if so
# args: $1 - temporary file to test
#       $2 - file to replace
define TESTANDREPLACEFILE
	if [ -f "$2" ]; then \
		if cmp "$1" "$2"; then \
			rm -f $1; \
		else \
			mv $1 $2; \
		fi \
	else \
		mv $1 $2; \
	fi
endef

# generate a header file at $1 with an expanded variable in $2
define MAKECONFIGHEADER
	$(MKDIR); \
	echo generating $1; \
	rm -f $1.tmp; \
	LDEF=`echo $1 | tr '/\\.-' '_' | sed "s/C++/CPP/g;s/c++/cpp/g"`; \
	echo \#ifndef __$${LDEF}_H > $1.tmp; \
	echo \#define __$${LDEF}_H >> $1.tmp; \
	for d in `echo $($2) | tr '[:lower:]' '[:upper:]'`; do \
		echo "#define $$d" | sed "s/=/\ /g;s/-/_/g;s/\//_/g;s/\./_/g;s/\//_/g;s/C++/CPP/g" >> $1.tmp; \
	done; \
	echo \#endif >> $1.tmp; \
	$(call TESTANDREPLACEFILE,$1.tmp,$1)
endef
