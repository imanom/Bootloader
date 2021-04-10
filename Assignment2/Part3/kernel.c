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

typedef unsigned long long u64;
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

void page_table(u64 *p, void* user_buffer, u64 stack_pointer_base, int type, int user_pages)
{
	u64 next_page = 0x0;

	//kernel
	if(type==0)
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
		u64 *start_pte = (u64*)user_buffer;
		u64 page_addr = (u64)start_pte;
		
		p[0] = stack_pointer_base+0x7;
		p[1] = page_addr+0x7;
		user_pages--;

		for (int i = 2; i < 511; i++)
		{
			if(user_pages!=0)
			{
				p[i] = page_addr+(i-1)*0x1000+0x7;
				user_pages--;
			}
			else
			{
				p[i] = 0x0ULL;
			}
			
		}
		p[511]=0x6;
	}
}

void page_directory(u64 *pd, u64 *p, int type)
{

	//kernel
	if(type==0){
		for (int j = 0; j < 2048; j++)
		{
			u64 *start_pte = p + 512 * j;
			u64 page_addr = (u64)start_pte;
			pd[j] = page_addr + 0x3;
		}

	}

	//user
	else{
		for (int j = 0; j < 512; j++)
		{
			
			if(j==0){
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
		if (type==0 && j < 4)
		{
			u64 *start_pde = pd + 512 * j;
			u64 page_addr = (u64)start_pde;
			pdp[j] = page_addr + 0x3;
		}

		//Last 1 entry in user
		else if(type==1 && j==511)
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
		if(type==0){
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
		else{
			//first entry for kernel
			if (j == 0)
			{
				pml4[j] = (u64)pdp_kernel + 0x3;
			}

			//last entry for user
			else if (j==511)
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

u64* setup_kernel_pagetable(void *addr)
{
	//PTE
	u64 *p = (u64 *)addr;
	page_table(p,NULL,0,0,0);
	

	//PDE
	u64 *pd = (p + 1048576);
	page_directory(pd,p,0);
	

	//PDPE
	u64* pdp = (pd + 2048);
	page_directory_pointer(pdp,pd,0);
	

	//PML4E
	u64 *pml4 = (pdp + 512);
	pml4_table(pml4, pdp, NULL,0);


	//CR3 register
	u64 *start_pml4e = pml4;
	u64 page_addr = (u64)start_pml4e;
	write_cr3(page_addr);

	return pdp;
}

void setup_user_pagetable(void *addr, u64* pdp_kernel, void * user_buffer, int user_pages)
{

	//PTE
	u64 *p = (u64 *)addr;
	u64 stack_pointer_base = (u64)(addr-0x1000);
	page_table(p, user_buffer,stack_pointer_base,  1, user_pages);
	

	//PDE
	u64 *pd = (p + 512);
	page_directory(pd,p,1);

	//PDPE
	u64 *pdp = (pd + 512);
	page_directory_pointer(pdp,pd,1);

	//PML4E
	u64 *pml4 = (pdp + 512);
	pml4_table(pml4, pdp, pdp_kernel, 1);

	//CR3 register
	u64 *start_pml4e = pml4;
    u64 page_addr = (u64)start_pml4e;
	write_cr3(page_addr);

}

void setup_pagetable(void *addr, void *user_addr, void* user_buffer, int user_pages)
{
	
	u64* pdp_kernel = setup_kernel_pagetable(addr);
	printf("Allocated initial kernel page table.\n");

	setup_user_pagetable(user_addr, pdp_kernel, user_buffer, user_pages);
	printf("Allocated new page table containing user.\n\n");


}


void init_idt_table(int num, void *ptr)
{
	struct idt_descriptor *i = &idt[num];

	i->i_hioffset = (u64)ptr >> 32; //3 
	i->i_midoffset = ((u64)ptr >> 16)& 0xffff; //2 
	i->i_looffset = (u64)ptr & 0xffff; //1

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
	for(int i=0;i<256;i++)
	{
		if(i==14)
		{
			init_idt_table(i, pagefault_trap_ptr);
		}
		else if(i<32)
		{
			init_idt_table(i, default_trap_ptr);
		}
		else if(i==40)
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

	idt_ptr.limit = sizeof(idt)-1;
	idt_ptr.base = (uint64_t)(void*)idt;
	load_idt(&idt_ptr);
}


void default_handler(void* rsp)
{
	printf("RSP pointer: %p\n", (u64)rsp);
}
void pagefault_handler()
{
	u64 base_addr = (u64)rsp_base_addr;
	u64 pte = base_addr - 0x5000;
	u64* pte_ptr =(u64*) pte;
	u64 new_page_addr = base_addr - 0x1000;
	pte_ptr[511] = new_page_addr+0x7;
	
	u64 pml4_addr = (u64)(new_page_addr-0x1000);
	write_cr3(pml4_addr);
	
	printf("Page fault handled! \n");
		
}

void interrupt_and_tss_setup(void * rsp0_stack)
{	
	init_tss_segment(rsp0_stack);
	load_tss_segment((u64)(0x28), (tss_segment_t *)rsp0_stack);
	x86_initidt();
	idt_pointer_init();
}


void tls_setup(void* addr)
{
	u64 base_addr = (u64)addr;
	u64 pte = base_addr;
	u64* pte_ptr =(u64*) pte;
	u64 new_page_addr = base_addr + 0x7000;
	pte_ptr[510] = new_page_addr+0x7;
	
	u64 pml4_addr = (u64)(base_addr+0x3000);
	write_cr3(pml4_addr);

	printf("tls page mapped!");
}

struct tls_block 
{
	struct tls_block *myself;
	char padding[4096-8];
};


void set_tls_info(void* addr)
{
	struct tls_block *tls = (struct tls_block*)addr;
	
	unsigned long p = (unsigned long) tls;

	tls->myself = tls;
	__asm__ __volatile__ ("wrmsr" ::
		"c" (0xc0000100),
		"a" ((unsigned)(p)),
		"d" ((unsigned)(p >> 32))
	);
}

void set_fs(void* addr)
{
	set_tls_info(addr);
}


void kernel_start(void *addr, unsigned int *fb, int width, void *user_addr, void *user_buffer, int user_pages)
{
	fb_init(fb, width, 600);
	
	setup_pagetable(addr, user_addr, user_buffer, user_pages);

	u64 jump_addr = 0xFFFFFFFFC0000000;
	user_stack = (void *)(jump_addr+0x1000);
	void * rsp0_stack = user_addr + 0x5000+0x1000;  //end of stack
	rsp_base_addr = (void*)(rsp0_stack - 0x1000);

	void* ptr = (void*)0xFFFFFFFFC01FE000;

	syscall_init();
	interrupt_and_tss_setup(rsp0_stack);
	x86_lapic_enable();
	apic_init();
	tls_setup(user_addr);
	set_fs(ptr);
	user_jump(user_stack);

	
	/* Never exit! */
	while (1)
	{};
}
