CFLAGS = -O3 -DBOOTXX -D_STANDALONE -DSTANDALONE -DRELOC=0x380000 -DSUN4M -DSUN_BOOTPARAMS -DDEBUG -DDEBUG_PROM -Iboot/$(ARCH)/include
LIBSACFLAGS = -O3 -D_STANDALONE -D__INTERNAL_LIBSA_CREAD -Iboot/$(ARCH)/include
LIBKERNCFLAGS = -O3 -D_KERNEL -Iboot/$(ARCH)/include

STAGE2_OBJS =  boot/$(ARCH)/srt0.o \
	boot/$(ARCH)/promdev.o \
	boot/$(ARCH)/closeall.o \
	boot/$(ARCH)/dvma.o \
	boot/$(ARCH)/stage2.o \
	boot/$(ARCH)/libsa/alloc.o \
	boot/$(ARCH)/libsa/exit.o \
	boot/$(ARCH)/libsa/printf.o \
	boot/$(ARCH)/libsa/memset.o \
	boot/$(ARCH)/libsa/memcpy.o \
	boot/$(ARCH)/libsa/memcmp.o \
	boot/$(ARCH)/libsa/strcmp.o \
	boot/$(ARCH)/libkern/bzero.o \
	boot/$(ARCH)/libkern/udiv.o \
	boot/$(ARCH)/libkern/urem.o \
	boot/$(ARCH)/libkern/__main.o

STAGE2_DEPS = $(STAGE2_OBJS:.o=.d)

boot/$(ARCH)/stage2: $(STAGE2_OBJS)
	ld -dN -Ttext 0x381278 -e start $(STAGE2_OBJS) -o $@

boot/$(ARCH)/libsa/%.o: boot/$(ARCH)/libsa/%.c
	gcc $(LIBSACFLAGS) -c $< -o $@

boot/$(ARCH)/libsa/%.d: boot/$(ARCH)/libsa/%.c
	gcc $(LIBSACFLAGS) -M -MG $< -o $@.1
	@echo -n $(dir $@) > $@; cat $@.1 >> $@; rm $@.1

boot/$(ARCH)/libkern/%.o: boot/$(ARCH)/libkern/%.c
	gcc $(LIBKERNCFLAGS) -c $< -o $@

boot/$(ARCH)/libkern/%.d: boot/$(ARCH)/libkern/%.c
	gcc $(LIBKERNCFLAGS) -M -MG $< -o $@.1
	@echo -n $(dir $@) > $@; cat $@.1 >> $@; rm $@.1

boot/$(ARCH)/libkern/%.o: boot/$(ARCH)/libkern/%.S
	gcc $(LIBKERNCFLAGS) -c $< -o $@

boot/$(ARCH)/libkern/%.d: boot/$(ARCH)/libkern/%.S
	gcc $(LIBKERNCFLAGS) -M -MG $< -o $@.1
	@echo -n $(dir $@) > $@; cat $@.1 >> $@; rm $@.1

boot/$(ARCH)/%.o: boot/$(ARCH)/%.S
	gcc $(CFLAGS) -D_LOCORE -c $< -o $@

boot/$(ARCH)/%.d: boot/$(ARCH)/%.S
	gcc $(CFLAGS) -D_LOCORE -M -MG $< -o $@.1
	@echo -n $(dir $@) > $@; cat $@.1 >> $@; rm $@.1

boot/$(ARCH)/%.o: boot/$(ARCH)/%.c
	gcc $(CFLAGS) -c $< -o $@

boot/$(ARCH)/%.d: boot/$(ARCH)/%.c
	gcc $(CFLAGS) -M -MG $< -o $@.1
	@echo -n $(dir $@) > $@; cat $@.1 >> $@; rm $@.1

stage2clean:
	rm -f $(STAGE2_OBJS) boot/$(ARCH)/stage2 boot/$(ARCH)/a.out

include $(STAGE2_DEPS)
