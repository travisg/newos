TARGET := pegasos.serial

OBJS := \
	main.o \
	isa.o \
	of.o \
	serial.o 


CFLAGS := -nostdinc -O
LDFLAGS := -Ttext=0x400000

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


