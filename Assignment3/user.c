/*
 * user.c - an application template (Assignment 2, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */

#include <syscall.h>
#include "userinc/syscall.h"
typedef unsigned long long u64;

__thread int a[100];

void user_start(void)
{
	const char* msg = "System call 1\n";
	const char* msg2 = "System call 2\n";
	
	__syscall1((long)1, (long)msg);
	__syscall1((long)1, (long)msg2);

	for(int i=0; i<100; i++)
	{
		a[i]=i;
	}
	


	while (1) {};
}
