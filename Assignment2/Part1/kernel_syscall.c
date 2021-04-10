#include <kernel_syscall.h>
#include <types.h>
#include <printf.h>
#include <msr.h>


void *kernel_stack; /* Initialized in kernel_entry.S */
void *user_stack = (void *)(0xFFFFFFFFC0001000); /* TODO: Must be initialized to a user stack region */

void *syscall_entry_ptr; /* Points to syscall_entry(), initialized in kernel_entry.S; use that rather than syscall_entry() when obtaining its address */



long do_syscall_entry(long n, long a1, long a2, long a3, long a4, long a5)
{
	// TODO: a system call handler
	// the system call number is in 'n'
	// make sure it is valid
	
	if(n==1){
		//Print the string

		printf("%s\n", a1);
	}




	return 0; /* Success */
}

void syscall_init(void)
{
	/* Enable SYSCALL/SYSRET */
	wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 0x1);

	/* GDT descriptors for SYSCALL/SYSRET (USER descriptors are implicit) */
	wrmsr(MSR_STAR, ((uint64_t) GDT_KERNEL_DATA << 48) | ((uint64_t) GDT_KERNEL_CODE << 32));

	// TODO: register a system call entry point
	wrmsr(MSR_STAR, ((uint64_t) GDT_USER_CODE << 48) | ((uint64_t) GDT_KERNEL_CODE << 32));
	wrmsr(MSR_LSTAR, (uint64_t) syscall_entry_ptr);
	

	/* Disable interrupts (IF) while in a syscall */
	wrmsr(MSR_SFMASK, 1U << 9);
}
