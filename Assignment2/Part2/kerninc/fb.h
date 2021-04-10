#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void fb_init(unsigned int *fb, unsigned int width, unsigned int height);
void fb_output(char ch);

#ifdef __cplusplus
}
#endif
