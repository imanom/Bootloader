/*
 * boot.c - a kernel template (Assignment 1, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */

#include <fb.h>
#include <printf.h>
#include <interrupts.h>
#include <apic.h>

#include <kernel_syscall.h>
#include <types.h>
#include "kerninc/cpuid.h"
#include <hypercall.h>
#include <memory.h>
#include <rdtsc.h>
#include <gnttab.h>

#define HYPERVISOR_XEN 0
#define HYPERVISOR_NONE 4
#define HYPERVISOR_NUM 1

typedef unsigned long long u64;
typedef __INT64_TYPE__ bmk_time_t;

void *default_trap_ptr;
void *pagefault_trap_ptr;
void *rsp_base_addr;
void *timer_apic_ptr;

// Write to the CR3 register with the base address of the PML4 Table

void write_cr3(unsigned long long cr3_value)
{
	asm volatile("mov %0, %%cr3"
				 :
				 : "r"(cr3_value)
				 : "memory");
}

void page_table(u64 *p, void *user_buffer, u64 stack_pointer_base, int type, int user_pages)
{
	u64 next_page = 0x0;

	//kernel
	if (type == 0)
	{
		for (int i = 0; i < 1048576; i++)
		{
			p[i] = (next_page + 0x3);

			next_page += 0x1000;
		}
	}

	//user
	else
	{
		u64 *start_pte = (u64 *)user_buffer;
		u64 page_addr = (u64)start_pte;

		p[0] = stack_pointer_base + 0x7;
		p[1] = page_addr + 0x7;
		user_pages--;

		for (int i = 2; i < 511; i++)
		{
			if (user_pages != 0)
			{
				p[i] = page_addr + (i - 1) * 0x1000 + 0x7;
				user_pages--;
			}
			else
			{
				p[i] = 0x0ULL;
			}
		}
		p[511] = 0x6;
	}
}

void page_directory(u64 *pd, u64 *p, int type)
{

	//kernel
	if (type == 0)
	{
		for (int j = 0; j < 2048; j++)
		{
			u64 *start_pte = p + 512 * j;
			u64 page_addr = (u64)start_pte;
			pd[j] = page_addr + 0x3;
		}
	}

	//user
	else
	{
		for (int j = 0; j < 512; j++)
		{

			if (j == 0)
			{
				pd[j] = (u64)p + 0x7;
			}

			//other entries are null
			else
			{
				pd[j] = 0x0ULL;
			}
		}
	}
}

void page_directory_pointer(u64 *pdp, u64 *pd, int type)
{
	for (int j = 0; j < 512; j++)
	{

		//first 4 entries in kernel
		if (type == 0 && j < 4)
		{
			u64 *start_pde = pd + 512 * j;
			u64 page_addr = (u64)start_pde;
			pdp[j] = page_addr + 0x3;
		}

		//Last 1 entry in user
		else if (type == 1 && j == 511)
		{
			pdp[j] = (u64)pd + 0x7;
		}

		//other entries are null
		else
		{
			pdp[j] = 0x0ULL;
		}
	}
}

void pml4_table(u64 *pml4, u64 *pdp, u64 *pdp_kernel, int type)
{
	for (int j = 0; j < 512; j++)
	{
		//Level 4 containing only kernel page table
		if (type == 0)
		{
			//first entry for kernel
			if (j == 0)
			{
				pml4[j] = (u64)pdp + 0x3;
			}
			//other entries are null
			else
			{
				pml4[j] = 0x0ULL;
			}
		}

		//Level 4 for new page table containing kernel and user
		else
		{
			//first entry for kernel
			if (j == 0)
			{
				pml4[j] = (u64)pdp_kernel + 0x3;
			}

			//last entry for user
			else if (j == 511)
			{

				pml4[j] = (u64)pdp + 0x7;
			}
			//other entries are null
			else
			{
				pml4[j] = 0x0ULL;
			}
		}
	}
}

u64 *setup_kernel_pagetable(void *addr)
{
	//PTE
	u64 *p = (u64 *)addr;
	page_table(p, NULL, 0, 0, 0);

	//PDE
	u64 *pd = (p + 1048576);
	page_directory(pd, p, 0);

	//PDPE
	u64 *pdp = (pd + 2048);
	page_directory_pointer(pdp, pd, 0);

	//PML4E
	u64 *pml4 = (pdp + 512);
	pml4_table(pml4, pdp, NULL, 0);

	//CR3 register
	u64 *start_pml4e = pml4;
	u64 page_addr = (u64)start_pml4e;
	write_cr3(page_addr);

	return pdp;
}

void setup_user_pagetable(void *addr, u64 *pdp_kernel, void *user_buffer, int user_pages)
{

	//PTE
	u64 *p = (u64 *)addr;
	u64 stack_pointer_base = (u64)(addr - 0x1000);
	page_table(p, user_buffer, stack_pointer_base, 1, user_pages);

	//PDE
	u64 *pd = (p + 512);
	page_directory(pd, p, 1);

	//PDPE
	u64 *pdp = (pd + 512);
	page_directory_pointer(pdp, pd, 1);

	//PML4E
	u64 *pml4 = (pdp + 512);
	pml4_table(pml4, pdp, pdp_kernel, 1);

	//CR3 register
	u64 *start_pml4e = pml4;
	u64 page_addr = (u64)start_pml4e;
	write_cr3(page_addr);
}

void setup_pagetable(void *addr, void *user_addr, void *user_buffer, int user_pages)
{

	u64 *pdp_kernel = setup_kernel_pagetable(addr);
	printf("Allocated initial kernel page table.\n");

	setup_user_pagetable(user_addr, pdp_kernel, user_buffer, user_pages);
	printf("Allocated new page table containing user.\n\n");
}

void init_idt_table(int num, void *ptr)
{
	struct idt_descriptor *i = &idt[num];

	i->i_hioffset = (u64)ptr >> 32;				//3
	i->i_midoffset = ((u64)ptr >> 16) & 0xffff; //2
	i->i_looffset = (u64)ptr & 0xffff;			//1

	i->i_selector = 0x8;
	i->i_ist = 0;
	i->i_type = 14;
	i->i_dpl = 0;
	i->i_p = 1;

	i->i_zero = 0;
	i->i_xx1 = 0;
	i->i_xx2 = 0;
	i->i_xx3 = 0;
}

void x86_initidt()
{
	for (int i = 0; i < 256; i++)
	{
		if (i == 14)
		{
			init_idt_table(i, pagefault_trap_ptr);
		}
		else if (i < 32)
		{
			init_idt_table(i, default_trap_ptr);
		}
		else if (i == 40)
		{
			init_idt_table(i, timer_apic_ptr);
		}
		else
		{
			init_idt_table(i, 0);
		}
	}
}

void idt_pointer_init()
{
	idt_pointer_t idt_ptr;

	idt_ptr.limit = sizeof(idt) - 1;
	idt_ptr.base = (uint64_t)(void *)idt;
	load_idt(&idt_ptr);
}

void default_handler(void *rsp)
{
	printf("RSP pointer: %p\n", (u64)rsp);
}
void pagefault_handler()
{
	u64 base_addr = (u64)rsp_base_addr;
	u64 pte = base_addr - 0x5000;
	u64 *pte_ptr = (u64 *)pte;
	u64 new_page_addr = base_addr - 0x1000;
	pte_ptr[511] = new_page_addr + 0x7;

	u64 pml4_addr = (u64)(new_page_addr - 0x1000);
	write_cr3(pml4_addr);

	printf("Page fault handled! \n");
}

void interrupt_and_tss_setup(void *rsp0_stack)
{
	init_tss_segment(rsp0_stack);
	load_tss_segment((u64)(0x28), (tss_segment_t *)rsp0_stack);
	x86_initidt();
	idt_pointer_init();
}

void tls_setup(void *addr)
{
	u64 base_addr = (u64)addr;
	u64 pte = base_addr;
	u64 *pte_ptr = (u64 *)pte;
	u64 new_page_addr = base_addr + 0x7000;
	pte_ptr[510] = new_page_addr + 0x7;

	u64 pml4_addr = (u64)(base_addr + 0x3000);
	write_cr3(pml4_addr);

	printf("tls page mapped!");
}

struct tls_block
{
	struct tls_block *myself;
	char padding[4096 - 8];
};

void set_tls_info(void *addr)
{
	struct tls_block *tls = (struct tls_block *)addr;

	unsigned long p = (unsigned long)tls;

	tls->myself = tls;
	__asm__ __volatile__("wrmsr" ::
							 "c"(0xc0000100),
						 "a"((unsigned)(p)),
						 "d"((unsigned)(p >> 32)));
}

void set_fs(void *addr)
{
	set_tls_info(addr);
}

/* 

Detect XEN hypervisor

*/
struct hypervisor_entry
{
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t min_leaves;
};

static struct hypervisor_entry hypervisors[HYPERVISOR_NUM] = {
	/* Xen: "XenVMMXenVMM" */
	[HYPERVISOR_XEN] = {0x566e6558, 0x65584d4d, 0x4d4d566e, 2}

};

unsigned hypervisor_detect(void)
{
	uint32_t eax, ebx, ecx, edx;
	unsigned i;

	/*
	 * First check for generic "hypervisor present" bit.
	 */
	x86_cpuid(0x0, &eax, &ebx, &ecx, &edx);
	if (eax >= 0x1)
	{
		x86_cpuid(0x1, &eax, &ebx, &ecx, &edx);
		if (!(ecx & (1 << 31)))
			return HYPERVISOR_NONE;
	}
	else
		return HYPERVISOR_NONE;

	/*
	 * CPUID leaf at 0x40000000 returns hypervisor vendor signature.
	 * Source: https://lkml.org/lkml/2008/10/1/246
	 */
	x86_cpuid(0x40000000, &eax, &ebx, &ecx, &edx);
	if (!(eax >= 0x40000000))
		return HYPERVISOR_NONE;

	for (i = 0; i < HYPERVISOR_NUM; i++)
	{
		if (ebx == hypervisors[i].ebx &&
			ecx == hypervisors[i].ecx &&
			edx == hypervisors[i].edx)
			return i;
	}
	return HYPERVISOR_NONE;
}

/*

Initialize hypervisor calls 
And map the shared info data structure

*/

uint32_t hypervisor_base(unsigned id)
{
	uint32_t base, eax, ebx, ecx, edx;

	if (id >= HYPERVISOR_NUM)
		return 0;

	/* Find the base. */
	for (base = 0x40000000; base < 0x40010000; base += 0x100)
	{
		x86_cpuid(base, &eax, &ebx, &ecx, &edx);
		if ((eax - base) >= hypervisors[id].min_leaves &&
			ebx == hypervisors[id].ebx &&
			ecx == hypervisors[id].ecx &&
			edx == hypervisors[id].edx)
		{
			return base;
		}
	}

	return 0;
}

shared_info_t *HYPERVISOR_shared_info;
static void
shared_init(void)
{
	struct xen_add_to_physmap xatp;

	HYPERVISOR_shared_info = (shared_info_t *)_minios_shared_info;
	xatp.domid = DOMID_SELF;
	xatp.idx = 0;
	xatp.space = 0;
	xatp.gpfn = (unsigned long)HYPERVISOR_shared_info >> 12;
	if (HYPERVISOR_memory_op(7, &xatp))
		printf("cannot get shared info\n");
}

void initialize_hypercalls()
{
	uint32_t eax, ebx, ecx, edx;
	uint64_t page = (unsigned long)_minios_hypercall_page;

	if (hypervisor_detect() != HYPERVISOR_XEN)
		return;

	uint32_t xen_base = hypervisor_base(HYPERVISOR_XEN);
	if (!xen_base)
		return;

	x86_cpuid(xen_base + 2, &eax, &ebx, &ecx, &edx);
	if (eax != 1)
	{
		xen_base = 0;
		return;
	}

	__asm__ __volatile("wrmsr" ::
						   "c"(ebx),
					   "a"((uint32_t)(page)),
					   "d"((uint32_t)(page >> 32)));

	printf("initialized XEN hypercalls\n");

	/* Check if Xen VCPU IDs are supported and sane. */
	x86_cpuid(xen_base + 4, &eax, &ebx, &ecx, &edx);
	if (!(eax & 0x8) || ebx != 0)
		printf("XEN VCPU IDs are not supported\n");

	shared_init();
}

void print_xen_version()
{
	unsigned long version = HYPERVISOR_xen_version(XENVER_version, NULL);
	unsigned int minor = version & 0x0000FFFF;
	unsigned int major = (version & 0XFFFF0000) >> 16;

	printf("Xen version major is: %d\n", major);
	printf("Xen version minor is: %d\n", minor);
}

struct pvclock_vcpu_time_info
{
	uint32_t version;
	uint32_t pad0;
	uint64_t tsc_timestamp;
	uint64_t system_time;
	uint32_t tsc_to_system_mul;
	int8_t tsc_shift;
	uint8_t flags;
	uint8_t pad[2];
} __attribute__((__packed__));

/* Xen/KVM wall clock ABI. */
struct pvclock_wall_clock
{
	uint32_t version;
	uint32_t sec;
	uint32_t nsec;
} __attribute__((__packed__));

volatile static struct pvclock_vcpu_time_info *pvclock_ti;
volatile static struct pvclock_wall_clock *pvclock_wc;
static bmk_time_t _x86_cpu_clock_monotonic, _x86_cpu_clock_monotonic_2;
static bmk_time_t rtc_epochoffset;

static inline uint64_t
mul64_32(uint64_t a, uint32_t b)
{
	uint64_t prod;
	__asm__(
		"mul %%rdx ; "
		"shrd $32, %%rdx, %%rax"
		: "=a"(prod)
		: "0"(a), "d"((uint64_t)b));
	return prod;
}

static inline bmk_time_t
_pvclock_monotonic(void)
{
	uint32_t version;
	uint64_t delta, time_now;

	do
	{
		version = pvclock_ti->version;
		__asm__("mfence" ::
					: "memory");
		delta = rdtsc() - pvclock_ti->tsc_timestamp;
		if (pvclock_ti->tsc_shift < 0)
			delta >>= -pvclock_ti->tsc_shift;
		else
			delta <<= pvclock_ti->tsc_shift;
		time_now = mul64_32(delta, pvclock_ti->tsc_to_system_mul) +
				   pvclock_ti->system_time;
		__asm__("mfence" ::
					: "memory");
	} while ((pvclock_ti->version & 1) || (pvclock_ti->version != version));

	return (bmk_time_t)time_now;
}

#define NSEC_PER_SEC 1000000000ULL

static bmk_time_t
pvclock_read_wall_clock(void)
{
	uint32_t version;
	volatile bmk_time_t wc_boot;

	do
	{
		version = pvclock_wc->version;
		__asm__("mfence" ::
					: "memory");
		wc_boot = pvclock_wc->sec * NSEC_PER_SEC;
		wc_boot += pvclock_wc->nsec;
		__asm__("mfence" ::
					: "memory");
	} while ((pvclock_wc->version & 1) || (pvclock_wc->version != version));

	return wc_boot;
}

void pvclock_init()
{
	pvclock_ti = (struct pvclock_vcpu_time_info *)&HYPERVISOR_shared_info->vcpu_info[0].time;
	pvclock_wc = (struct pvclock_wall_clock *)&HYPERVISOR_shared_info->wc_version;

	_x86_cpu_clock_monotonic = _pvclock_monotonic();
	printf("\ninitial monotonic clock:%ld\n", _x86_cpu_clock_monotonic);

	rtc_epochoffset = pvclock_read_wall_clock();
	printf("initial wall clock:%ld\n", rtc_epochoffset + _x86_cpu_clock_monotonic);

	while (1)
	{
		_x86_cpu_clock_monotonic_2 = _pvclock_monotonic();
		rtc_epochoffset = pvclock_read_wall_clock();
		if ((_x86_cpu_clock_monotonic_2 - _x86_cpu_clock_monotonic) >= 1000000000)
		{
			break;
		}
	}
	printf("\nclock values after 1 second:\n");
	printf("monotonic clock:%ld\n", _x86_cpu_clock_monotonic_2);

	printf("wall clock:%ld\n", rtc_epochoffset + _x86_cpu_clock_monotonic_2);
}

void shared_memory_init(void *addr)
{
	struct gnttab_map_grant_ref op;
	op.ref = 511;
	op.dom = (domid_t) 76;
	op.host_addr = (uint64_t)(addr+0x2000);
	op.flags = GNTMAP_host_map;
	
	int rc = HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, &op, 1);
	if (rc != 0 || op.status != GNTST_okay)
	{
		printf("GNTTABOP_map_grant_ref failed: "
			   "returned %d, status %d\n",
			   rc, op.status);
	}
	else{
		printf("GNTTABOP_map_grant_ref worked.\n");
	}

	printf("\nother side: %s\n", (char*)(addr+0x2000));
	
}

void kernel_start(void *addr, unsigned int *fb, int width, void *user_addr, void *user_buffer, int user_pages)
{
	fb_init(fb, width, 600);

	setup_pagetable(addr, user_addr, user_buffer, user_pages);

	u64 jump_addr = 0xFFFFFFFFC0000000;
	user_stack = (void *)(jump_addr + 0x1000);
	void * rsp0_stack = user_addr + 0x5000 + 0x1000; //end of stack
	rsp_base_addr = (void *)(rsp0_stack - 0x1000);

	void *ptr = (void *)0xFFFFFFFFC01FE000;

	syscall_init();
	interrupt_and_tss_setup(rsp0_stack);
	//x86_lapic_enable();
	//apic_init();
	tls_setup(user_addr);
	set_fs(ptr);

	unsigned i = hypervisor_detect();
	printf("\nXen Hypervisor detect: %d\n", i);
	initialize_hypercalls();
	print_xen_version();
	pvclock_init();
	shared_memory_init(user_addr + 0x8000);

	user_jump(user_stack);

	/* Never exit! */
	while (1)
	{
	};
}
