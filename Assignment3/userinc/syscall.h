#pragma once

/*
 * Based on musl-libc's syscall_arch.h
 * https://git.musl-libc.org/cgit/musl/plain/arch/x86_64/syscall_arch.h
 *
 * See musl-libc's licensing terms
 *
 * System calls can accept 0..5 parameters, use the corresponding __syscalN()
 * function. Note that 'long' in the kernel/user context is 64-bit (but not in
 * the boot loader!) An extra argument, n, specifies the system call number.
 */

/*
 * We diverge from typical Linux's and musl's system calls:
 * for simplicity, we do not pass the system call number in %rax and use %rdi
 * instead, other parameters are off by one register consequently.
 */

static __inline long __syscall0(long n)
{
	unsigned long ret;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "D"(n) : "rcx", "r11", "memory");
	return ret;
}

static __inline long __syscall1(long n, long a1)
{
	unsigned long ret;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "D"(n), "S"(a1)
						  : "rcx", "r11", "memory");
	return ret;
}

static __inline long __syscall2(long n, long a1, long a2)
{
	unsigned long ret;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "D"(n), "S"(a1),
						  "d"(a2) : "rcx", "r11", "memory");
	return ret;
}

static __inline long __syscall3(long n, long a1, long a2, long a3)
{
	unsigned long ret;
	register long r10 __asm__("r10") = a3;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "D"(n), "S"(a1),
						  "d"(a2), "r"(r10): "rcx", "r11", "memory");
	return ret;
}

static __inline long __syscall4(long n, long a1, long a2, long a3, long a4)
{
	unsigned long ret;
	register long r10 __asm__("r10") = a3;
	register long r8 __asm__("r8") = a4;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "D"(n), "S"(a1),
						  "d"(a2), "r"(r10), "r"(r8) : "rcx", "r11", "memory");
	return ret;
}

static __inline long __syscall5(long n, long a1, long a2, long a3, long a4, long a5)
{
	unsigned long ret;
	register long r10 __asm__("r10") = a3;
	register long r8 __asm__("r8") = a4;
	register long r9 __asm__("r9") = a5;
	__asm__ __volatile__ ("syscall" : "=a"(ret) : "D"(n), "S"(a1),
						  "d"(a2), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
	return ret;
}
