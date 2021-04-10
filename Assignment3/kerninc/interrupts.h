#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <printf.h>

/* GDT, see kernel_entry.S */
extern uint64_t gdt[];
typedef unsigned long long u64;

extern void *default_trap_ptr;
extern void *pagefault_trap_ptr;
extern void *rsp_base_addr;
/*
 * NOTE: When declaring the IDT table, make
 * sure it is properly aligned, e.g.,
 *
 * ... idt[256] __attribute__((aligned(16)));
 * rather than just idt[256]
 *
 * The TSS segment _must_ be page-aligned, so you
 * can even allocate a dedicated page for it.
 * Please note that if TSS is not page-aligned, you
 * may encounter very weird issues that are hard
 * to track or debug!
 */

void init_idt_table(int num, void *ptr);
void idt_pointer_init(void);
void x86_initidt(void);
void default_handler(void* rsp);
void pagefault_handler(void);

struct idt_descriptor {
	u64 i_looffset:16;	/* gate offset (lsb) */
	u64 i_selector:16;	/* gate segment selector */
	u64 i_ist:3;		/* IST select */
	u64 i_xx1:5;		/* reserved */
	u64 i_type:5;	/* segment type */
	u64 i_dpl:2;		/* segment descriptor priority level */
	u64 i_p:1;		/* segment descriptor present */
	u64 i_midoffset:16;	/* gate offset (msb) */
	u64 i_hioffset:32;	/* gate offset (msb) */
	u64 i_xx2:8;		/* reserved */
	u64 i_zero:5;	/* must be zero */
	u64 i_xx3:19;	/* reserved */

} __attribute__((__packed__));
static struct idt_descriptor idt[256] __attribute__ ((aligned(16)));


/* Load IDT */

/* Please define and initialize this variable and then load
   this 80-bit "pointer" (it is similar to GDT's 80-bit "pointer") */
struct idt_pointer 
{
	uint16_t limit;
	uint64_t base;
} __attribute__((__packed__));

typedef struct idt_pointer idt_pointer_t;


static inline void load_idt(idt_pointer_t *idtp)
{
	__asm__ __volatile__ ("lidt %0; sti" /* also enable interrupts */
		:
		: "m" (*idtp)
	);
}




/*
 * TSS segment, see also https://wiki.osdev.org/Task_State_Segment
 */
struct tss_segment 
{
	uint32_t reserved1;
	uint64_t rsp[3]; /* rsp[0] is used, everything else not used */
	uint64_t reserved2;
	uint64_t ist[7]; /* not used, initialize to 0s */
	uint64_t reserved3;
	uint16_t reserved4;
	uint16_t iopb_base; /* must be sizeof(struct tss), I/O bitmap not used */
} __attribute__((__packed__));

typedef struct tss_segment tss_segment_t;


static inline
void init_tss_segment(void* addr)
{
	tss_segment_t *t = (tss_segment_t *)addr;	//addr points to end of rsp stack and beginning of tss page
	t->rsp[0] = (u64)addr;
	t->iopb_base = sizeof(tss_segment_t);
	t->ist[0]=0;
	t->ist[1]=0;
	t->ist[2]=0;
	t->ist[3]=0;
	t->ist[4]=0;
	t->ist[5]=0;
	t->ist[6]=0;
	t->reserved1=0, t->reserved2=0, t->reserved3=0, t->reserved4=0;
	t->rsp[1]=0, t->rsp[2]=0;

}

/*
 * This function initializes the TSS descriptor in GDT, you have
 * to place two 64-bit dummy 0x0 entries in GDT and specify
 * the selector of the first entry here.
 *
 * After initializing the GDT entries, this function will load the
 * TSS descriptor from GDT as the final step.
 *
 * You have to specify GDT's selector, the base address and _limit_
 * for the TSS segment.
 */
static inline
void load_tss_segment(uint16_t selector, tss_segment_t *tss)
{
	uint64_t base = (uint64_t) tss;
	uint16_t limit = sizeof(tss_segment_t) - 1;

	/* The TSS descriptor is initialized according to AMD64's Figure 4-22,
	   https://www.amd.com/system/files/TechDocs/24593.pdf */

	/* Initialize GDT's dummy entries */
	uint64_t *tss_gdt = (uint64_t *) ((char *) gdt + selector);
	/* Present=1, DPL=0, Type=9 (TSS), 16-bit limit, lower 32 bits of 'base' */
	tss_gdt[0] = ((base & 0xFF000000ULL) << 32) | (1ULL << 47) | (0x9ULL << 40) | ((base & 0x00FFFFFFULL) << 16) | limit;
	/* Upper 32 bits of 'base' */
	tss_gdt[1] = base >> 32;

	/* Load TSS, use the specified selector */
	__asm__ __volatile__ ("ltr %0"
		:
		: "rm" (selector)
	);
}
