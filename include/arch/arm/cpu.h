/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_ARCH_ARM_CPU_H
#define _NEWOS_ARCH_ARM_CPU_H

#define PAGE_SIZE 4096

#define _BIG_ENDIAN 0
#define _LITTLE_ENDIAN 1

#if 0
#define global_flush_d_cache() { \
    asm volatile("mcr	p15, 0x0, r0, c9, c10, 0x0	/* clean D-cache */"); \
    asm volatile("mcr	p15, 0x0, r0, c7, c6,  0x0	/* flush D-cache */"); \
    asm volatile("nop; nop; nop;"); \
}
#else
#define global_flush_d_cache() { \
    asm volatile("__foo: mrc	p15, 0x0, r0, c7, c14, 0x3	/* test clean and invalidate Dcache */"); \
    asm volatile("bne __foo"); \
}
#endif

#define global_flush_i_cache() { \
    asm volatile("mcr p15, 0x0, r0, c7, c5, 0x0	/* invalidate I-cache */"); \
    asm volatile("nop; nop; nop;"); \
}

#define global_tlb_flush() \
    asm volatile("mcr p15, 0, r0, c8, c7, 0")

#define read_cpu_id() ({ \
    unsigned int __v; \
    asm volatile("mrc	p15, 0x0, %0, c0, c0, 0x0" : "=r"(__v)); \
    __v; \
})

#define read_cir() ({ \
    unsigned int __v; \
    asm volatile("mrc	p15, 0x0, %0, c0, c0, 0x1" : "=r"(__v)); \
    __v; \
})

#define read_cp15_control_reg() ({ \
    unsigned int __v; \
    asm volatile("mrc 	p15, 0x0, %0, c1, c0, 0x0" : "=r"(__v)); \
    __v; \
})

#define write_cp15_control_reg(val) { \
    asm volatile("mcr 	p15, 0x0, %0, c1, c0, 0x0" :: "r"(val)); \
    asm volatile("nop; nop; nop;"); \
}

#define read_trans_table_base() ({ \
    unsigned int __v; \
    asm volatile("mrc	p15, 0x0, %0, c2, c0, 0x0" : "=r"(__v)); \
    __v; \
})

#define write_trans_table_base(val) { \
    asm volatile("mcr	p15, 0x0, %0, c2, c0, 0x0" :: "r"(val)); \
}

#define write_domain_control_reg(val) { \
    asm volatile("mcr	p15, 0x0, %0, c3, c0, 0x0" :: "r"(val)); \
}

#define read_fault_sr() ({ \
    unsigned int __v; \
    asm volatile("mrc	p15, 0x0, %0, c5, c0, 0x0" : "=r"(__v)); \
    __v; \
})

#define read_fault_ar() ({ \
    unsigned int __v; \
    asm volatile("mrc	p15, 0x0, %0, c6, c0, 0x0" : "=r"(__v)); \
    __v; \
})

#define read_cpsr() ({ \
    unsigned int __v; \
    asm volatile("mrs	%0,cpsr" : "=r"(__v)); \
    __v; \
})

#endif

