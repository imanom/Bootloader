#pragma once

#include <types.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t vsnprintf(char *buffer, size_t n, const char *fmt, va_list args);
size_t vsprintf(char *buffer, const char *fmt, va_list args);
size_t snprintf(char * buffer, size_t n, const char * fmt, ...);
size_t sprintf(char * buffer, const char * fmt, ...);
size_t vprintf(const char *fmt, va_list args);
size_t printf(const char * fmt, ...);

#ifdef __cplusplus
}
#endif
