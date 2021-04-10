#pragma once

#define X86_MSR_APIC			0x01BU
#define X86_MSR_APIC_ENABLE		0x800U
#define X86_MSR_APIC_X2APIC		0x400U

#define X86_MSR_X2APIC_BASE		0x800U

#define X86_LAPIC_X2APIC		((void *) (-1L))

#define X86_LAPIC_ID			0x02U
#define X86_LAPIC_TPR			0x08U
#define X86_LAPIC_DFR			0x0EU
#define X86_LAPIC_SVR			0x0FU
#define X86_LAPIC_ICR			0x30U

#define X86_LAPIC_EOI			0x0BU
#define X86_LAPIC_TIMER			0x32U
#define X86_LAPIC_TIMER_INIT	0x38U
#define X86_LAPIC_TIMER_DIVIDE	0x3EU

void x86_lapic_enable(void);
void apic_handler(void);
void apic_init(void);

extern void *timer_apic_ptr;