
.text

// int atomic_add(int *val, int incr)
.global atomic_add
atomic_add:
	movl	4(%esp),%edx
	
_atomic_add1:
	movl	8(%esp),%ecx
	movl	(%edx),%eax
	addl	%eax,%ecx
	
	lock
	cmpxchgl	%ecx,(%edx)
	
	jnz		_atomic_add1
	
	ret

// int atomic_and(int *val, int incr)
.global atomic_and
atomic_and:
	movl	4(%esp),%edx
	
_atomic_and1:
	movl	8(%esp),%ecx
	movl	(%edx),%eax
	andl	%eax,%ecx
	
	lock
	cmpxchgl	%ecx,(%edx)
	
	jnz		_atomic_and1
	
	ret

// int atomic_or(int *val, int incr)
.global atomic_or
atomic_or:
	movl	4(%esp),%edx
	
_atomic_or1:
	movl	8(%esp),%ecx
	movl	(%edx),%eax
	orl		%eax,%ecx
	
	lock
	cmpxchgl	%ecx,(%edx)
	
	jnz		_atomic_or1
	
	ret

// int test_and_set(int *val, int set_to)
.global test_and_set
test_and_set:
    movl     4(%esp),%edx     /* load address of variable into edx */

_test_and_set1:
    movl     8(%esp),%ecx    /* load the value to set the lock to */
    movl     (%edx),%eax      /* load the value of variable into eax */

    cmpl     $0, %eax         /* see if someone else got it */
    jnz      _test_and_set2   /* if so, bail out */

    lock
    cmpxchg  %ecx,(%edx)

    jnz      _test_and_set1   /* if zf = 0, cmpxchng failed so redo it */

_test_and_set2:
    ret

// saves the conversion factor needed for system_time
.global cv_factor
cv_factor:
	.word 0
			
.global setup_system_time
setup_system_time:
	movl	4(%esp),%eax
	movl	%eax,cv_factor
	ret
	
// long long system_time();
.global system_time
system_time:
	/* load 64-bit factor into %eax (low), %edx (high) */
	rdtsc		/* time in %edx,%eax */

	pushl	%ebx
	pushl	%ecx
	movl	cv_factor, %ebx
	movl	%edx, %ecx	/* save high half */
	mull	%ebx 		/* truncate %eax, but keep %edx */
	movl	%ecx, %eax
	movl	%edx, %ecx	/* save high half of low */
	mull	%ebx /*, %eax*/
	/* now compute  [%edx, %eax] + [%ecx], propagating carry */
	subl	%ebx, %ebx	/* need zero to propagate carry */
	addl	%ecx, %eax
	adc		%ebx, %edx
	popl	%ecx
	popl	%ebx
	ret

// void i386_context_switch(unsigned int **old_esp, unsigned int *new_esp);
.global i386_context_switch
i386_context_switch:
//	pushfl
	pusha
	movl	36(%esp),%eax
	movl	%esp,(%eax)
	movl	40(%esp),%eax
	movl	%eax,%esp
	popa
//	popfl
	ret

null_idt_descr:
	.word	0
	.word	0,0

.global reboot
reboot:
	lidt	null_idt_descr
	int		$0
done:
	jmp		done
