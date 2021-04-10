/*
 * apic.c - an APIC driver (Assignment 2, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */

#include <types.h>
#include <msr.h>
#include <apic.h>
#include <printf.h>

static void *lapic_base = NULL;

static inline void
cpuid(uint32_t level, uint32_t *eax_out, uint32_t *ebx_out,
		uint32_t *ecx_out, uint32_t *edx_out)
{
	uint32_t eax_, ebx_, ecx_, edx_;

	__asm__ __volatile__ (
		"cpuid"
		: "=a" (eax_), "=b" (ebx_), "=c" (ecx_), "=d" (edx_)
		: "0" (level), "2" (0)
	);
	*eax_out = eax_;
	*ebx_out = ebx_;
	*ecx_out = ecx_;
	*edx_out = edx_;
}

static inline uint32_t
x86_lapic_read(uint32_t offset)
{
	if (lapic_base == X86_LAPIC_X2APIC) {
		uint64_t msr = rdmsr(X86_MSR_X2APIC_BASE + offset);
		return (uint32_t) msr;
	}
	return *(volatile uint32_t *) (lapic_base + (offset << 4));
}

static inline uint32_t
x86_lapic_read_id(void)
{
	if (lapic_base == X86_LAPIC_X2APIC) {
		uint64_t msr = rdmsr(X86_MSR_X2APIC_BASE + X86_LAPIC_ID);
		return (uint32_t) msr;
	}
	return (*(volatile uint32_t *) (lapic_base + (X86_LAPIC_ID << 4))) >> 24;
}

static inline void
x86_x2apic_barrier(void)
{
	/* Seems we need *both* mfence and lfence per Intel (10.12.3) */
	__asm__ __volatile("mfence; lfence" :::
					   "memory");
}

static inline void
x86_lapic_write(uint32_t offset, uint32_t value)
{
	if (lapic_base == X86_LAPIC_X2APIC) {
		x86_x2apic_barrier();
		wrmsr(X86_MSR_X2APIC_BASE + offset, value);
		return;
	}
	*(volatile uint32_t *) (lapic_base + (offset << 4)) = value;
}

static inline void
x86_x2apic_write_icr(uint32_t cmd_low, uint32_t cmd_high)
{
	x86_x2apic_barrier();
	wrmsr(X86_MSR_X2APIC_BASE + X86_LAPIC_ICR, ((uint64_t) cmd_high << 32) | cmd_low);
}

static inline void
x86_lapic_write_icr(uint32_t cmd)
{
	if (lapic_base == X86_LAPIC_X2APIC) {
		x86_x2apic_write_icr(cmd, 0);
		return;
	}
	*(volatile uint32_t *) (lapic_base + (X86_LAPIC_ICR << 4)) = cmd;
	while ((*(volatile uint32_t *) (lapic_base + (X86_LAPIC_ICR << 4)))
				& 0x1000U) ; /* Wait until the command is completed. */
}

void
x86_lapic_enable(void)
{
	uint32_t eax, ebx, ecx, edx;
	uint32_t x2apic = 0;
	uint64_t msr;

	cpuid(1, &eax, &ebx, &ecx, &edx);

	if (!(edx & (1U << 9))) { /* No APIC. */
		lapic_base = NULL;
		return;
	}

	if (((eax >> 8) & 0x0FU) < 6) { /* CPU family: before P6. */
		lapic_base = (void *) 0xFEE00000;
	} else {
		if ((ecx & (1U << 21)) != 0)
			x2apic = X86_MSR_APIC_X2APIC;
		msr = rdmsr(X86_MSR_APIC);
		x86_x2apic_barrier();
		wrmsr(X86_MSR_APIC, msr | X86_MSR_APIC_ENABLE | x2apic);
		if (!x2apic) {
#ifdef __i386__
			lapic_base = (void *) (msr & 0xFFFFF000U);
#else
			lapic_base = (void *) (msr & 0xFFFFFF000ULL);
#endif
		} else {
			lapic_base = X86_LAPIC_X2APIC;
		}
	}

	if (x2apic) {
		printf("x2APIC enabled\n");
	} else {
		printf("APIC enabled, base: %p\n", lapic_base);
	}

	/* Set the spurious-interrupt vector register: enable=1, vector=0xFF. */
	x86_lapic_write(X86_LAPIC_SVR, 0xFF | (0x1U << 8));

	/* Enable the flat model (N/A for x2APIC) .*/
	if (!x2apic)
		x86_lapic_write(X86_LAPIC_DFR, 0xFFFFFFFFU);

	/* Reset the task priority register */
	x86_lapic_write(X86_LAPIC_TPR, 0x00U);
}

void apic_init()
{
	x86_lapic_write(X86_LAPIC_TIMER, 40|0x00020000);
	x86_lapic_write(X86_LAPIC_TIMER_DIVIDE, 0xA);
	x86_lapic_write(X86_LAPIC_TIMER_INIT, 0x400000);

}

void apic_handler()
{
	printf("Timer!\n");
	x86_lapic_write(X86_LAPIC_EOI, 0);
}
