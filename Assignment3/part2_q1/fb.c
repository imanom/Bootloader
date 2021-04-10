/*
 * fb.c - a framebuffer console driver (Assignment 2, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */

#include <fb.h>
#include <types.h>

extern unsigned char __ascii_font[2048]; /* ascii_font.c */

#define FONT_WIDTH 8
#define FONT_HEIGHT 16

static unsigned int *Fb;
static unsigned int Width, PosX, PosY, MaxX, MaxY;

#define HELLO_STATEMENT \
	"Framebuffer Console (ECE 6504)\nCopyright (C) 2021 Ruslan Nikolaev\n\n"

void fb_init(unsigned int *fb, unsigned int width, unsigned int height)
{
	size_t i, num = (size_t) width * height;
	const char *__hello_statement = HELLO_STATEMENT;

	/* Clean up the screen */
	for (i = 0; i < num; i++) {
		fb[i] = 0x00000000U;
	}

	Fb = fb;
	Width = width;
	PosX = 0;
	PosY = 0;
	MaxX = width / FONT_WIDTH;
	MaxY = height / FONT_HEIGHT;

	/* Print a hello statement */
	for (i = 0; i < sizeof(HELLO_STATEMENT)-1; i++) {
		fb_output(__hello_statement[i]);
	}
}

static void fb_scrollup(void)
{
	/* Move the text up one row */
	size_t cur = 0, count = Width * ((MaxY - 1) * FONT_HEIGHT);
	size_t row = Width * FONT_HEIGHT;
	do {
		Fb[cur] = Fb[cur+row];
		cur++;
	} while (--count != 0);

	/* Clean up the last row */
	do {
		Fb[cur] = 0x00000000U;
		cur++;
	} while (--row != 0);
}

void fb_output(char ch)
{
	size_t cur;
	unsigned char *ptr;
	if ((signed char) ch <= 0) { /* not in the ASCII subset */
		if (ch == 0) return;
		ch = '?'; /* an unknown character */
	}
	if (ch == '\n' || PosX == MaxX) {
		PosX = 0;
		PosY++;
	}
	if (PosY == MaxY) {
		PosY--;
		fb_scrollup();
	}
	if (ch == '\n')
		return;
	ptr = &__ascii_font[(unsigned char) ch * (FONT_WIDTH * FONT_HEIGHT / 8)];
	cur = (size_t) PosX * FONT_WIDTH + (PosY * FONT_HEIGHT) * Width;
	for (size_t j = 0; j < FONT_HEIGHT; j++) {
		/* for simplicity, assume that FONT_WIDTH=8, i.e., fits in one byte */
		signed char bitmap = ptr[j];
		for (size_t i = 0; i < FONT_WIDTH; i++) {
			signed char color = (bitmap >> 7); /* propagate the sign bit */
			Fb[cur + i] = (signed int) color; /* sign extend to 32 bits */
			bitmap <<= 1;
		}
		cur += Width;
	}
	PosX++;
}
