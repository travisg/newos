/*
** Copyright 2002-2003, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __newos__libc_stdlib__hh__
# define __newos__libc_stdlib__hh__


# include <stddef.h>

# ifdef __cplusplus
#  include <new>
# endif

# ifdef __cplusplus
namespace std
{extern "C" {
# endif


int	      atoi(char const *);
unsigned int  atoui(const char *num);
long          atol(const char *num);
unsigned long atoul(const char *num);

long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);

void * malloc(size_t);
void   free(void *);
void * realloc(void *, size_t);
void * calloc(size_t, size_t);

char *getenv(char const *);

void *bsearch(void const *, void const *, size_t, size_t, int (*) (void const *, void const *));
void  qsort(void *, size_t, size_t, int (*)(void const *, void const *));

# define	RAND_MAX	0x7fffffff
int   rand(void);
void  srand(unsigned);

# if !_KERNEL
void abort(void);
void exit(int);
# endif

# ifdef __cplusplus
}} /* extern "C" */
# endif

/* terrible hack to get around the different kernel name for malloc and free */
# if _KERNEL
#  define malloc kmalloc
#  define free kfree
#  include <kernel/heap.h>
# endif

# define max(a, b) ((a) > (b) ? (a) : (b))
# define min(a, b) ((a) < (b) ? (a) : (b))

// non (C++) standard stuff goes here
# ifdef __cplusplus
extern "C"
{
// define size_t to be correct in C++
#  define size_t ::std::size_t
# endif

int   setenv(char const *, char const *, int);
int   putenv(char const *);
void  unsetenv(char const *);

int   heapsort(void *, size_t , size_t , int (*)(void const *, void const *));
int   mergesort(void *, size_t, size_t, int (*)(void const *, void const *));
int   radixsort(unsigned char const **, int, unsigned char const *, unsigned int);
int   sradixsort(unsigned char const **, int, unsigned char const *, unsigned int);

void * reallocf(void *, size_t);
void * memalign(size_t, size_t);
void * valloc(size_t);

long long strtoll(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);

/* getopt related items */
extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

int getopt(int argc, char * const *argv, const char *optstring);

int   rand_r(unsigned int *ctx);
long  random(void);
void  srandom(unsigned long);

# if !_KERNEL
void _exit(int);
# endif

# ifdef __cplusplus
#  undef size_t
}
# endif

#endif // end of include gaurd

#if defined(__cplusplus) && !defined(_NEWOS_NO_LIBC_COMPAT)
using ::std::abort;
//using ::std::atexit;
using ::std::exit;

using ::std::getenv;
//using ::std::system;

using ::std::calloc;
using ::std::malloc;
using ::std::realloc;
using ::std::free;

using ::std::atol;
//using ::std::atof;
using ::std::atoi;
//using ::std::mblen;
//using ::std::mbstowcs;
//using ::std::mbtowc;
//using ::std::strtod;
using ::std::strtol;
using ::std::strtoul;
//using ::std::wctomb;
//using ::std::wcstombs;

using ::std::bsearch;
using ::std::qsort;

//using ::std::abs;
//using ::std::div;
//using ::std::labs;
//using ::std::ldiv;
using ::std::srand;
using ::std::rand;
#endif
