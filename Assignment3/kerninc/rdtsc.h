#pragma once

static inline uint64_t
rdtsc(void)
{
	uint32_t eax, edx;
	__asm__ __volatile__("rdtsc" : "=a" (eax), "=d" (edx));
	return ((uint64_t) edx << 32) | eax;
}
