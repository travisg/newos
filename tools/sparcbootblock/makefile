
header: ../sparcbootblock.h

../sparcbootblock.h: sparcbootblock
	file2c 'unsigned char sparcbootblock[] = {' '};' < sparcbootblock > $@

sparcbootblock: sparcbootblock.o
	ld -N -Ttext 0x380000 sparcbootblock.o -o a.out
	(echo -n 01 | tr 01 '\01\03'; tail +3c a.out) > $@
	rm a.out

sparcbootblock.o: sparcbootblock.S
	gcc -c $< -o $@

clean:
	rm -f *.o a.out sparcbootblock
