# libc string makefile
LIBC_STRING_DIR := string

MY_SRCS += \
	$(LIBC_STRING_DIR)/bcopy.c \
	$(LIBC_STRING_DIR)/bzero.c \
	$(LIBC_STRING_DIR)/memchr.c \
	$(LIBC_STRING_DIR)/memcmp.c \
	$(LIBC_STRING_DIR)/memcpy.c \
	$(LIBC_STRING_DIR)/memmove.c \
	$(LIBC_STRING_DIR)/memset.c \
	$(LIBC_STRING_DIR)/strcat.c \
	$(LIBC_STRING_DIR)/strchr.c \
	$(LIBC_STRING_DIR)/strcmp.c \
	$(LIBC_STRING_DIR)/strcoll.c \
	$(LIBC_STRING_DIR)/strcpy.c \
	$(LIBC_STRING_DIR)/strdup.c \
	$(LIBC_STRING_DIR)/strerror.c \
	$(LIBC_STRING_DIR)/strlcat.c \
	$(LIBC_STRING_DIR)/strlcpy.c \
	$(LIBC_STRING_DIR)/strlen.c \
	$(LIBC_STRING_DIR)/strncat.c \
	$(LIBC_STRING_DIR)/strncmp.c \
	$(LIBC_STRING_DIR)/strncpy.c \
	$(LIBC_STRING_DIR)/strnicmp.c \
	$(LIBC_STRING_DIR)/strnlen.c \
	$(LIBC_STRING_DIR)/strpbrk.c \
	$(LIBC_STRING_DIR)/strrchr.c \
	$(LIBC_STRING_DIR)/strspn.c \
	$(LIBC_STRING_DIR)/strstr.c \
	$(LIBC_STRING_DIR)/strtok.c \
	$(LIBC_STRING_DIR)/strxfrm.c

include $(addsuffix /makefile, $(addprefix $(MY_SRCDIR)/string/, \
	arch/$(ARCH) \
))
