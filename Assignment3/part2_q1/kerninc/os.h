/* borrowed from mini-os/os.h */

#pragma once

#define rmb()   __asm__ __volatile__ ("lfence":::"memory")
#define wmb()   __asm__ __volatile__ ("sfence" ::: "memory")

struct __synch_xchg_dummy { unsigned long a[100]; };
#define __synch_xg(x) ((volatile struct __synch_xchg_dummy *)(x))

#define synch_cmpxchg(ptr, old, new) \
((__typeof__(*(ptr)))__synch_cmpxchg((ptr),\
                                     (unsigned long)(old), \
                                     (unsigned long)(new), \
                                     sizeof(*(ptr))))

static inline unsigned long __synch_cmpxchg(volatile void *ptr,
        unsigned long old,
        unsigned long new, int size)
{
    unsigned long prev;
    switch (size) {
        case 1:
            __asm__ __volatile__("lock; cmpxchgb %b1,%2"
                    : "=a"(prev)
                    : "q"(new), "m"(*__synch_xg(ptr)),
                    "0"(old)
                    : "memory");
            return prev;
        case 2:
            __asm__ __volatile__("lock; cmpxchgw %w1,%2"
                    : "=a"(prev)
                    : "r"(new), "m"(*__synch_xg(ptr)),
                    "0"(old)
                    : "memory");
            return prev;
#ifdef __x86_64__
        case 4:
            __asm__ __volatile__("lock; cmpxchgl %k1,%2"
                    : "=a"(prev)
                    : "r"(new), "m"(*__synch_xg(ptr)),
                    "0"(old)
                    : "memory");
            return prev;
        case 8:
            __asm__ __volatile__("lock; cmpxchgq %1,%2"
                    : "=a"(prev)
                    : "r"(new), "m"(*__synch_xg(ptr)),
                    "0"(old)
                    : "memory");
            return prev;
#else
        case 4:
            __asm__ __volatile__("lock; cmpxchgl %1,%2"
                    : "=a"(prev)
                    : "r"(new), "m"(*__synch_xg(ptr)),
                    "0"(old)
                    : "memory");
            return prev;
#endif
    }
    return old;
}
