#pragma once

#include <types.h>

static inline void
x86_cpuid(uint32_t level, uint32_t *eax_out, uint32_t *ebx_out,
		uint32_t *ecx_out, uint32_t *edx_out)
{
	uint32_t eax_, ebx_, ecx_, edx_;

	__asm__ __volatile__ (
		"cpuid"
		: "=a" (eax_), "=b" (ebx_), "=c" (ecx_), "=d" (edx_)
		: "0" (level), "2" (0)
	);
	*eax_out = eax_;
	*ebx_out = ebx_;
	*ecx_out = ecx_;
	*edx_out = edx_;
}
