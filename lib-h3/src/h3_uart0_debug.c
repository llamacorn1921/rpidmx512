/**
 * @file h3_uart0_debug.c
 *
 */
/* Copyright (C) 2018-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>

#include "h3.h"
#include "h3_uart.h"

void __attribute__((cold)) uart0_init(void) {
	h3_uart_begin(0, 115200, H3_UART_BITS_8, H3_UART_PARITY_NONE, H3_UART_STOP_1BIT);

	while ((H3_UART0->USR & UART_USR_BUSY) == UART_USR_BUSY) {
		(void) H3_UART0->O00.RBR;
	}
}

void uart0_putc(int c) {
	while (!(H3_UART0->LSR & UART_LSR_THRE))
		;
	H3_UART0->O00.THR = (uint32_t) (c);
}

void uart0_puts(char *s) {
	while (*s != '\0') {
		if (*s == '\n') {
			uart0_putc('\r');
		}
		uart0_putc(*s++);
	}
}

int uart0_getc(void) {
	if (__builtin_expect(((H3_UART0->LSR & UART_LSR_DR) != UART_LSR_DR), 1)) {
		return EOF;
	}

	const int c = (int) H3_UART0->O00.RBR;

#if defined (UART0_ECHO)
	uart0_putc(c);
#endif

	return c;
}
