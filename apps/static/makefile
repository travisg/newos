# app makefile
MY_SRCDIR := $(GET_LOCAL_DIR)

MY_SRCS := \
	main.c

MY_INCLUDES := -Iinclude/nulibc
MY_LIBS := $(LIBC_STATIC) $(LIBNEWOS_STATIC) $(APPS_BUILD_DIR)/rldtest/girlfriend.so

include templates/app.mk

