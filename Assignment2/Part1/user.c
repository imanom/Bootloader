/*
 * user.c - an application template (Assignment 2, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */

#include <syscall.h>
#include "userinc/syscall.h"

void user_start(void)
{
	// TODO: place some system call here

	// Cast all arguments, including pointers, to 'long'
	// Note that a string is const char * (a pointer)
	const char* msg = "System call 1\n";
	const char* msg2 = "System call 2\n";

	__syscall1((long)1, (long)msg);
	__syscall1((long)1, (long)msg2);

	/* Never exit */
	while (1) {};
}
