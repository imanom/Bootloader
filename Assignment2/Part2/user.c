/*
 * user.c - an application template (Assignment 2, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */

#include <syscall.h>
#include "userinc/syscall.h"
typedef unsigned long long u64;

void user_start(void)
{
	const char* msg = "System call 1\n";
	const char* msg2 = "System call 2\n";
	const char* msg3 = "System call 3\n";

	__syscall1((long)1, (long)msg);
	__syscall1((long)1, (long)msg2);
	*((char *)(0xFFFFFFFFC01FF000)) = 0;  	//0xFFFFFFFFC0000000 + (0x1000*511)
	
	__syscall1((long)1, (long)msg3);
	
	while (1) {};
}
