#pragma once

#include <types.h>

#define MSR_EFER	0xC0000080
#define MSR_STAR	0xC0000081
#define MSR_LSTAR	0xC0000082
#define MSR_SFMASK	0xC0000084

/* GDT entries, do not re-arrange those! */
#define GDT_KERNEL_CODE	0x08
#define GDT_KERNEL_DATA	0x10
#define GDT_USER_DATA	0x18
#define GDT_USER_CODE	0x20

static inline uint64_t rdmsr(uint32_t reg)
{
	uint32_t val_low, val_high;

	__asm__ __volatile__ ("rdmsr"
		: "=a" (val_low),
		  "=d" (val_high)
		: "c" (reg)
	);

	return ((uint64_t) val_high << 32) | val_low;
}

static inline void wrmsr(uint32_t reg, uint64_t val)
{
	__asm__ __volatile__ ("wrmsr"
		:
		: "a" ((uint32_t) (val)),
		  "d" ((uint32_t) (val >> 32)),
		  "c" (reg)
	);
}
