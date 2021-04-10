/*
 * Copyright 2018 Ruslan Nikolaev <rnikola@vt.edu>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*----------------------------------------------------------------------------

  This implementation is based on the public domain implementation
  of printf available at http://my.execpc.com/~geezer/code/printf.c
  The code has been heavily modified & expanded to suit 'shim' purposes,
  and the 64-bit BCD conversion function has been added.

  ----------------------------------------------------------------------------

  ORIGINAL NOTICE:

  Stripped-down printf()
  Chris Giese	<geezer@execpc.com>	http://my.execpc.com/~geezer

  I, the copyright holder of this work, hereby release it into the
  public domain. This applies worldwide. If this is not legally possible:
  I grant any entity the right to use this work for any purpose,
  without any conditions, unless such conditions are required by law.

  xxx - current implementation of printf("%n" ...) is wrong -- RTFM

  16 Feb 2014:
  - test for NULL buffer moved from sprintf() to vsprintf()

  3 Dec 2013:
  - do_printf() restructured to get rid of confusing goto statements
  - do_printf() now returns EOF if an error occurs
    (currently, this happens only if out-of-memory in vasprintf_help())
  - added vasprintf() and asprintf()
  - added support for %Fs (far pointer to string)
  - compile-time option (PRINTF_UCP) to display pointer values
    as upper-case hex (A-F), instead of lower-case (a-f)
  - the code to check for "%--6d", "%---6d", etc. has been removed;
    these are now treated the same as "%-6d"

  3 Feb 2008:
  - sprintf() now works with NULL buffer; returns size of output
  - changed va_start() macro to make it compatible with ANSI C

  12 Dec 2003:
  - fixed vsprintf() and sprintf() in test code

  28 Jan 2002:
  - changes to make characters 0x80-0xFF display properly

  10 June 2001:
  - changes to make vsprintf() terminate string with '\0'

  12 May 2000:
  - math in DO_NUM (currently num2asc()) is now unsigned, as it should be
  - %0 flag (pad left with zeroes) now works
  - actually did some TESTING, maybe fixed some other bugs

  ----------------------------------------------------------------------------

  %[flag][width][.prec][mod][conv]
  flag:	-	left justify, pad right w/ blanks	DONE
	0	pad left w/ 0 for numerics	DONE
	+	always print sign, + or -	no
	' '	(blank)						no
	#	(???)						no

  width:		(field width)		DONE

  prec:		(precision)				no

  conv:	f,e,g,E,G float				no
	d,i	decimal int					DONE
	u	decimal unsigned			DONE
	o	octal						DONE
	x,X	hex							DONE
	c	char						DONE
	s	string						DONE
	p	ptr							DONE

  mod:	h	short int				DONE
  	hh		char					DONE
	l		long int				DONE
	L,ll	long long int			DONE
	z,t		size_t, ptrdiff_t		DONE

  To do:
  - implement '+' flag
  - implement ' ' flag

----------------------------------------------------------------------------*/

#include <printf.h>
#include <string.h>
#include <fb.h>

/* display pointers in upper-case hex (A-F) instead of lower-case (a-f) */
#define	PRINTF_UCP	1

/* flags used in processing format string */
#define	PR_POINTER	0x001	/* 0x prefix for pointers */
#define	PR_NEGATIVE	0x002	/* PR_DO_SIGN set and num was < 0 */
#define	PR_LEFTJUST	0x004	/* left justify */
#define	PR_PADLEFT0	0x008	/* pad left with '0' instead of ' ' */
#define	PR_DO_SIGN	0x010	/* signed numeric conversion (%d vs. %u) */
#define	PR_8		0x020	/* 8 bit numeric conversion */
#define	PR_16		0x040	/* 16 bit numeric conversion */
#define	PR_64		0x080	/* 64 bit numeric conversion */

#define PR_BUFLEN  64

const char HexDigits[32] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f'
};

static char *write_uword_base10(char *where, size_t num)
{
	do {
		size_t temp = num % 10;
		*--where = temp + '0';
		num = num / 10;
	} while (num != 0);
	return where;
}

typedef void (*fnptr_t) (char, void *);

/*****************************************************************************
  name:	do_printf
  action:	minimal subfunction for ?printf, calls function
	'fn' with arg 'ptr' for each character to be output
  returns:total number of characters output
*****************************************************************************/

static size_t _do_vprintf(const char *fmt, fnptr_t fn, void *ptr, va_list args)
{
	char *where, buf[PR_BUFLEN];
	const char *digits;
	size_t count, actual_wd, given_wd;
	unsigned int state, flags, shift;
	size_t num;

	count = given_wd = 0;
	state = flags = 0;
/* for() and switch() on the same line looks jarring
but the indentation gets out of hand otherwise */
	for (; *fmt; fmt++) switch(state)
	{
/* STATE 0: AWAITING '%' */
	case 0:
/* echo text until '%' seen */
		if (*fmt != '%')
		{
			fn(*fmt, ptr);
			count++;
			break;
		}
/* found %, get next char and advance state to check if next char is a flag */
		fmt++;
		state++;
		flags = 0;
		/* FALL THROUGH */
/* STATE 1: AWAITING FLAGS ('%' or '-' or '0') */
	case 1:
		if (*fmt == '%')	/* %% */
		{
			fn(*fmt, ptr);
			count++;
			state = 0;
			break;
		}
		if (*fmt == '-')
		{
			flags |= PR_LEFTJUST;
			break;
		}
		if (*fmt == '0')
		{
			flags |= PR_PADLEFT0;
/* '0' could be flag or field width -- fall through */
			fmt++;
		}
/* '0' or not a flag char: advance state to check if it's field width */
		state++;
		given_wd = 0;
		/* FALL THROUGH */
/* STATE 2: AWAITING (NUMERIC) FIELD WIDTH */
	case 2:
		if (*fmt >= '0' && *fmt <= '9')
		{
			given_wd = 10 * given_wd + (*fmt - '0');
			break;
		}
/* not field width: advance state to check if it's a modifier */
		state++;
		/* FALL THROUGH */
/* STATE 3: AWAITING MODIFIER charACTERS */
	case 3:
		/* XXX: Assume sizeof(size_t) == sizeof(size_t) */
		if (*fmt == 'z' || *fmt == 't') {
			flags |= PR_64;
			break;
		}
		if (*fmt == 'l')
		{
			if(*(fmt + 1) == 'l') {
				fmt++;
				flags |= PR_64;
				break;
			}
			flags |= PR_64;
			break;
		}
		if (*fmt == 'L') {
			flags |= PR_64;
			break;
		}
		if (*fmt == 'h') {
			if (*(fmt + 1) == 'h') {
				fmt++;
				flags |= PR_8;
			} else {
				flags |= PR_16;
			}
			break;
		}
/* not a modifier: advance state to check if it's a conversion char */
		state++;
		/* FALL THROUGH */
/* STATE 4: AWAITING CONVERSION charACTER */
	case 4:
		where = &buf[PR_BUFLEN - 1];
		*where = '\0';
		digits = HexDigits;
/* pointer and numeric conversions */
		switch(*fmt) {
		case 'p':
#ifndef PRINT_UCP
			digits = HexDigits + 16;
#endif
			flags &= ~(PR_DO_SIGN | PR_NEGATIVE | PR_8 | PR_16 | PR_64);
			flags |= PR_64;
			shift = 4; /* display pointers in hex */
			num = (size_t) va_arg(args, void *);
			if (!num) {
				flags &= ~PR_PADLEFT0;
				where = "(nil)";
				break;
			}
			flags |= PR_POINTER;
			goto DO_NUM_OUT;
		case 'x':
			digits = HexDigits + 16;
			/* FALL THROUGH */
		case 'X':
			flags &= ~PR_DO_SIGN;
			shift = 4; /* display pointers in hex */
			goto DO_NUM;
		case 'd':
		case 'i':
			flags |= PR_DO_SIGN;
			shift = 0;
			goto DO_NUM;
		case 'u':
			flags &= ~PR_DO_SIGN;
			shift = 0;
			goto DO_NUM;

		case 'o':
			flags &= ~PR_DO_SIGN;
			shift = 3;
DO_NUM:
			if (flags & PR_DO_SIGN) {
				ssize_t snum;

				if (flags & PR_64) {
					snum = va_arg(args, int64_t);
				} else {
					snum = va_arg(args, int);
					if (flags & (PR_16 | PR_8))
						snum = (flags & PR_16) ? (int16_t) snum : (int8_t) snum;
				}
				if (snum < 0) {
					flags |= PR_NEGATIVE;
					snum = -snum;
				}
				num = snum;
			} else {
				if (flags & PR_64) {
					num = va_arg(args, uint64_t);
				} else {
					num = va_arg(args, unsigned int);
					if (flags & (PR_16 | PR_8))
						num = (flags & PR_16) ? (uint16_t) num : (uint8_t) num;
				}
			}

DO_NUM_OUT:
			/* Convert binary to octal/decimal/hex ASCII;
			   the math here is _always_ unsigned */
			if (!shift) {
				where = write_uword_base10(where, num);
			} else {
				size_t mask = (1U << shift) - 1;
				do {
					size_t temp = num & mask;
					*--where = digits[temp];
					num = num >> shift;
				} while (num != 0);
			}
			break;

		case 'c':
/* disallow these modifiers for %c */
			flags &= ~(PR_DO_SIGN | PR_NEGATIVE | PR_PADLEFT0);
/* yes; we're converting a character to a string here: */
			where--;
			*where = (char) va_arg(args, int);
			break;
		case 's':
/* disallow these modifiers for %s */
			flags &= ~(PR_DO_SIGN | PR_NEGATIVE | PR_PADLEFT0);
			where = va_arg(args, char *);
			if (!where)
				where = "(null)";
			break;
/* bogus conversion character -- copy it to output and go back to state 0 */
		default:
			fn(*fmt, ptr);
			count++;
			state = flags = given_wd = 0;
			continue;
		}
/* emit formatted string */
		actual_wd = strlen(where);
		if (flags & (PR_POINTER | PR_NEGATIVE))
		{
			actual_wd += 1 + ((flags & PR_POINTER) != 0);
/* if we pad left with ZEROES, do the sign now
(for numeric values; not for %c or %s) */
			if (flags & PR_PADLEFT0) {
				if (flags & PR_POINTER) {
					fn('0', ptr);
					fn('x', ptr);
					count += 2;
				} else {
					fn('-', ptr);
					count++;
				}
			}
		}
/* pad on left with spaces or zeroes (for right justify) */
		if ((flags & PR_LEFTJUST) == 0)
		{
			for (; given_wd > actual_wd; given_wd--)
			{
				fn(flags & PR_PADLEFT0 ? '0' : ' ', ptr);
				count++;
			}
		}
/* if we pad left with SPACES, do the sign now */
		if ((flags & (PR_POINTER | PR_NEGATIVE) &&
					!(flags & PR_PADLEFT0)))
		{
			if (flags & PR_POINTER) {
				fn('0', ptr);
				fn('x', ptr);
				count += 2;
			} else {
				fn('-', ptr);
				count++;
			}
		}
/* emit converted number/char/string */
		for (; *where != '\0'; where++)
		{
			fn(*where, ptr);
			count++;
		}
/* pad on right with spaces (for left justify) */
		if (given_wd < actual_wd)
			given_wd = 0;
		else
			given_wd -= actual_wd;
		for (; given_wd; given_wd--)
		{
			fn(' ', ptr);
			count++;
		}
		/* FALL THROUGH */
	default:
		state = flags = given_wd = 0;
		break;
	}
	return count;
}

static inline size_t do_vprintf(const char *fmt, fnptr_t fn, void *ptr,
								va_list args)
{
	size_t count = _do_vprintf(fmt, fn, ptr, args);
	fn('\0', ptr);
	return count;
}

typedef struct vsnprintf_output_s {
	char *Cur;
	size_t Num;
} vsnprintf_output_s;

typedef struct vsprintf_output_s {
	char *Cur;
} vsprintf_output_s;

static void vsnprintf_output(char ch, void * _state)
{
	vsnprintf_output_s * state = (vsnprintf_output_s *) _state;

	if (state->Num != 0) {
		*state->Cur++ = (--state->Num) ? ch : '\0';
	}
}

static void vsprintf_output(char ch, void * _state)
{
	vsprintf_output_s * state = (vsprintf_output_s *) _state;
	*state->Cur++ = ch;
}

size_t vsnprintf(char *buf, size_t n, const char *fmt, va_list args)
{
	vsnprintf_output_s state = { .Cur = buf, .Num = n };
	return do_vprintf(fmt, vsnprintf_output, &state, args);
}

size_t vsprintf(char *buf, const char *fmt, va_list args)
{
	vsprintf_output_s state = { .Cur = buf };
	return do_vprintf(fmt, vsprintf_output, &state, args);
}

size_t snprintf(char *buf, size_t n, const char *fmt, ...)
{
	va_list args;
	size_t rv;

	va_start(args, fmt);
	rv = vsnprintf(buf, n, fmt, args);
	va_end(args);
	return rv;
}

size_t sprintf(char *buf, const char *fmt, ...)
{
	va_list args;
	size_t rv;

	va_start(args, fmt);
	rv = vsprintf(buf, fmt, args);
	va_end(args);
	return rv;
}

static void vprintf_output(char ch, void * _state)
{
	fb_output(ch);
}

size_t vprintf(const char *fmt, va_list args)
{
	return do_vprintf(fmt, vprintf_output, NULL, args);
}

size_t printf(const char *fmt, ...)
{
	va_list args;
	size_t rv;

	va_start(args, fmt);
	rv = vprintf(fmt, args);
	va_end(args);
	return rv;
}
