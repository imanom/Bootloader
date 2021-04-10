#pragma once

#define va_start(ptr, last)	__builtin_va_start(ptr, last)
#define va_copy(dst, src)	__builtin_va_copy(dst, src)
#define va_end(ptr)			__builtin_va_end(ptr)
#define va_arg(ptr, type)	__builtin_va_arg(ptr, type)

typedef __builtin_va_list va_list;
