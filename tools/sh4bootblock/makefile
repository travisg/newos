
header: ../sh4bootblock.h

../sh4bootblock.h: sh4bootblock.bin
	file2c 'unsigned char sh4bootblock[] = {' '};' < sh4bootblock.bin > $@

sh4bootblock: sh4bootblock.o
	sh-elf-ld -EL -N -Ttext 0x8c000000 sh4bootblock.o -o sh4bootblock

sh4bootblock.bin: sh4bootblock
	sh-elf-objcopy -O binary sh4bootblock $@

sh4bootblock.o: sh4bootblock.S
	sh-elf-gcc -ml -m4 -mhitachi -c $< -o $@

clean:
	rm -f *.o a.out sh4bootblock sh4bootblock.bin
