
.text

// int atomic_add(int *val, int incr)
.global atomic_add
atomic_add:
	movl	8(%esp),%edx
	
_atomic_add1:
	movl	12(%esp),%ecx
	movl	(%edx),%eax
	addl	%eax,%ecx
	
	lock
	cmpxchgl	%ecx,(%edx)
	
	jnz		_atomic_add1
	
	ret

// int atomic_and(int *val, int incr)
.global atomic_and
atomic_and:
	movl	8(%esp),%edx
	
_atomic_and1:
	movl	12(%esp),%ecx
	movl	(%edx),%eax
	andl	%eax,%ecx
	
	lock
	cmpxchgl	%ecx,(%edx)
	
	jnz		_atomic_and1
	
	ret

// int atomic_or(int *val, int incr)
.global atomic_or
atomic_or:
	movl	8(%esp),%edx
	
_atomic_or1:
	movl	12(%esp),%ecx
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
