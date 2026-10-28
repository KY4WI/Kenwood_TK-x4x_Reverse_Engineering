/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* uart.h -- the CPU's hardware UART (P24/TXD, P25/RXD; KCT-19 accessory
 * pins 15/14, CN3 inside the radio). Send only, polled, 8 data bits, no
 * parity, 2 stop bits.
 *
 * The code is baudsweep's (Kenwood_TK-x4x_Reverse_Engineering,
 * tools/probes/baudsweep/uart.c), unchanged.
 * Proven: baudsweep, clean text at all six speeds below, 2026-10-16;
 * uartprobe, "Hello world!" at 1200 bps, 2026-10-16. */

#ifndef UART_H
#define UART_H

#ifndef U8_TYPEDEF_DEFINED
#define U8_TYPEDEF_DEFINED
typedef unsigned char u8;
#endif

/* Speeds, as the (SCC, BRG) pair uart_init() and uart_set_rate() take:
 * uart_set_rate(UART_38400). bit/s = 6 MHz / (2^(SCC+2) * BRG), from the
 * user's manual (hardware reference section 12.4). Every speed here
 * comes out 0.16% fast. 1200 is what the mask ROM sets at reset. */
#define UART_1200    0x04, 0x4E
#define UART_9600    0x00, 0x9C
#define UART_19200   0x00, 0x4E
#define UART_38400   0x00, 0x27
#define UART_57600   0x00, 0x1A
#define UART_115200  0x00, 0x0D

/* Framing, the SCM value uart_init_framed() takes: asynchronous, 8 data
 * bits, no parity, transmitter and receiver enabled; SCM bit 2 (SL) is
 * the number of stop bits. 8N2 is the mask ROM's (and uart_init()'s). */
#define UART_8N2  0xCD
#define UART_8N1  0xC9

/* As uart_init(), with the framing given. Call once, before the first
 * byte. */
void uart_init_framed(u8 scc_val, u8 brg_val, u8 scm_val);

/* Set the speed and the framing, and enable the transmitter. Call once,
 * before the first byte. */
void uart_init(u8 scc_val, u8 brg_val);

/* Change speed afterwards. Writes only SCC and BRG: the manual warns that
 * rewriting SCM's TXRDY bit while sending can lose or repeat a byte. */
void uart_set_rate(u8 scc_val, u8 brg_val);

/* Send one byte and wait until it has gone, kicking the watchdog. */
void uart_send_byte(u8 c);

/* Send a string (no line ending added). */
void uart_puts(const char *s);

#endif
