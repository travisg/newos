#ifndef _SH4_H
#define _SH4_H

// using 4k pages
#define PAGE_SIZE 4096
#define PAGE_ALIGN(x) (((x) + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1))

#endif

