# libc string makefile
LIBC_STRING_DIR := string

MY_OBJS += \
	$(LIBC_STRING_DIR)/bcopy.o \
	$(LIBC_STRING_DIR)/bzero.o \
	$(LIBC_STRING_DIR)/memchr.o \
	$(LIBC_STRING_DIR)/memcmp.o \
	$(LIBC_STRING_DIR)/memcpy.o \
	$(LIBC_STRING_DIR)/memmove.o \
	$(LIBC_STRING_DIR)/memset.o \
	$(LIBC_STRING_DIR)/strcat.o \
	$(LIBC_STRING_DIR)/strchr.o \
	$(LIBC_STRING_DIR)/strcmp.o \
	$(LIBC_STRING_DIR)/strcpy.o \
	$(LIBC_STRING_DIR)/strerror.o \
	$(LIBC_STRING_DIR)/strlcat.o \
	$(LIBC_STRING_DIR)/strlcpy.o \
	$(LIBC_STRING_DIR)/strlen.o \
	$(LIBC_STRING_DIR)/strncat.o \
	$(LIBC_STRING_DIR)/strncmp.o \
	$(LIBC_STRING_DIR)/strncpy.o \
	$(LIBC_STRING_DIR)/strnicmp.o \
	$(LIBC_STRING_DIR)/strnlen.o \
	$(LIBC_STRING_DIR)/strpbrk.o \
	$(LIBC_STRING_DIR)/strrchr.o \
	$(LIBC_STRING_DIR)/strspn.o \
	$(LIBC_STRING_DIR)/strstr.o \
	$(LIBC_STRING_DIR)/strtok.o

include $(addsuffix /makefile, $(addprefix $(MY_SRCDIR)/string/, \
	arch/$(ARCH) \
))
