/*
 * boot.c - a kernel template (Assignment 1, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */

typedef unsigned long long u64;



// Write to the CR3 register with the base address of the PML4 Table

void write_cr3(unsigned long long cr3_value)
{
	asm volatile("mov %0, %%cr3"
				 :
				 : "r"(cr3_value)
				 : "memory");
}

void page_table(u64 *p){
	u64 next_page = 0x0;
	for (int i = 0; i < 1048576; i++)
	{
		p[i] = (next_page + 0x3);
		next_page += 0x1000;
	}
}

void page_directory(u64 *pd, u64 *p){
	for (int j = 0; j < 2048; j++)
	{
		u64 *start_pte = p + 512 * j;
		u64 page_addr = (u64)start_pte;
		pd[j] = page_addr + 0x3;
	}
}

void page_directory_pointer(u64 *pdp, u64 *pd){
	for (int j = 0; j < 512; j++)
	{
		if (j < 4)
		{
			u64 *start_pde = pd + 512 * j;
			u64 page_addr = (u64)start_pde;
			pdp[j] = page_addr + 0x3;
		}
		else
		{
			pdp[j] = 0x0ULL;
		}
	}
}

void pml4_table(u64 *pml4, u64 *pdp){
	for (int j = 0; j < 512; j++)
	{
		if (j < 1)
		{
			u64 *start_pdpe = pdp;
			u64 page_addr = (u64)start_pdpe;
			pml4[j] = page_addr + 0x3;
		}
		else
		{
			pml4[j] = 0x0ULL;
		}
	}

}

// Draw a figure and assign a color to the framebuffer
void draw_figure(unsigned int *fb, int width){
	
	unsigned int color = 0x80FF00FF;

	for (int i = 100; i < 500; i++)
	{
		for (int j = 100; j < 400; j++)
		{
			fb[i + j * width] = color;
		}
	}
}

void kernel_start(unsigned int *fb, int width, int height, void *addr)
{

	//PTE
	u64 *p = (u64 *)addr;
	page_table(p);
	

	//PDE
	u64 *pd = (p + 1048576);
	page_directory(pd,p);

	//PDPE
	u64 *pdp = (pd + 2048);
	page_directory_pointer(pdp,pd);

	//PML4E
	u64 *pml4 = (pdp + 512);
	pml4_table(pml4, pdp);

	//CR3 register
	u64 *start_pml4e = pml4;
	u64 page_addr = (u64)start_pml4e;
	write_cr3(page_addr);


	/* TODO: Draw some simple figure (rectangle, square, etc)
	   to indicate kernel activity. 800,600 */
	draw_figure(fb, width);

	/* Never exit! */
	while (1)
	{
	};
}
