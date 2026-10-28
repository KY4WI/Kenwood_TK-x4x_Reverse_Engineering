/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* =====================================================================
 * TK-840 replacement-firmware: baudsweep main.
 *
 * Cycles the real hardware UART through a fixed ladder of bit rates
 * forever, announcing each one and then sending a short test pattern
 * repeatedly, so the bench terminal can just be set to each speed in
 * turn and the result read directly -- no need to rebuild/reflash to
 * try a different rate. Framing is always 8N2 (uart.h) at every speed.
 *
 * RESULT (2026-10-16, rebuilt with KY4WI/vbcc 2945d2e): clean text at
 * all six speeds, 1200-115200. Hardware reference section 12.4.
 *
 * Follow-on to uartprobe. Its first run showed "the alphabet and some
 * non-printing characters" at 1200 -- not its "Hello world!" but the
 * vbcc string bug: the string's first two bytes, "He" (6548h), used as
 * the address, and the open bus returning each address's low byte. The
 * speed was right; the data was not. This image keeps that exact proven
 * 1200-baud register pair as its first, known-good rung, then climbs
 * through the standard PC-COM-port ladder up to 115200.
 *
 * The bit-banged P15 debug channel (already proven, fixed 2400 8N2,
 * independent of the hardware UART entirely) prints one digit at the
 * start of each rate -- '0' through '5' for this table's six entries
 * -- as a liveness breadcrumb: if that keeps advancing but a given
 * hardware-UART rate never shows anything readable, the loop itself is
 * still alive and the problem is specific to that one rate/register
 * pair, not a hang or a watchdog reset.
 *
 * STACK BUDGET: the test pattern is sent as flat uart_puts() calls
 * directly in main()'s inner loop, not through a separate helper
 * function. An earlier draft used one; it added a whole extra call+
 * frame level (4 bytes: a 2-byte return address plus that function's
 * own 2-byte local frame) on top of the already-deep main -> uart_puts
 * -> uart_send_byte -> wdt_poll -> wdt_service chain, which measured
 * out to a peak stack depth of 29 of the 30 bytes available before
 * SP=0xFE20 reaches the external codeplug flash at 0xFE00 -- only 3
 * bytes of margin, the same kind of razor-thin margin panelprobe
 * BETA_17's real stack-overflow bug lived in. Inlining the calls here
 * removes that one extra level and brings the same measured peak down
 * to 25 bytes (7 bytes of margin) -- see build.sh's comment and
 * rebuild with the inspected .s output if this loop body changes
 * again, to re-check the peak rather than assume it stayed put.
 *
 * CORRECTION (2026-10-16): the real cause of "garbage at every rate" was
 * the vbcc string bug. rate_scc[i], rate_brg[i] and rate_banner[i] were
 * read from the wrong addresses, so the UART never got the speeds this
 * table names, and every string sent was wrong. The SCM/TXRDY change
 * below did not fix it (the next bench run was still garbage); it is
 * kept because the manual's warning stands.
 *
 * REVISION HISTORY -- the SCM/TXRDY theory, as it was written:
 * the first built/delivered version of this image called a single
 * uart_init(scc,brg) on every pass through the outer (rate) loop below.
 * On real hardware this produced garbage at every rate, confirmed by
 * the tester across multiple full sweep cycles and multiple stop-bit
 * settings -- not a bench/timing mistake, an actual firmware bug. Root
 * cause, confirmed against the real NEC User's Manual (IEU-1265D)
 * chapter 12.1: uart_init() rewrites SCM, which carries the TXRDY/RXE
 * enable bits, and the manual explicitly documents that rewriting
 * TXRDY mid-communication causes duplicate TxB writes and lost
 * transmit data. uartprobe never hit this (it wrote SCM exactly once,
 * at boot, before any byte had been sent); this image's sweep rewrote
 * it on every single rate change, 6 times a cycle, forever -- and once
 * the first corruption hit, everything downstream (including later
 * passes back through the already-proven 1200 baud settings) stayed
 * corrupted, matching exactly what was observed. Fix: uart_init() is
 * now called exactly once, below, before the sweep starts; every rate
 * change inside the loop calls uart_set_rate() instead, which touches
 * only SCC/BRG and never SCM again. See uart.h for the full quote and
 * derivation. */
#include "uart.h"
#include "watchdog.h"
#include "debug_serial.h"

#define NUM_RATES 6
#define REPS_PER_RATE 5

/* Parallel arrays rather than an array of struct -- deliberately, to
 * avoid exercising struct-array codegen this backend has not been
 * exercised against yet (see KY4WI/vbcc machines/78k3/BUGS.md for the kind of
 * latent codegen bug this project has already found once; staying on
 * well-trodden array/pointer ground here is cheap insurance). */
static const char * const rate_banner[NUM_RATES] = {
    "\r\n\r\n*** 1200 baud, 8N2 ***\r\n",
    "\r\n\r\n*** 9600 baud, 8N2 ***\r\n",
    "\r\n\r\n*** 19200 baud, 8N2 ***\r\n",
    "\r\n\r\n*** 38400 baud, 8N2 ***\r\n",
    "\r\n\r\n*** 57600 baud, 8N2 ***\r\n",
    "\r\n\r\n*** 115200 baud, 8N2 ***\r\n"
};

/* n (SCC PRS2-0) / G (BRG) pairs -- see uart.h for the formula and the
 * full derivation of each entry. */
static const u8 rate_scc[NUM_RATES] = { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const u8 rate_brg[NUM_RATES] = { 0x4E, 0x9C, 0x4E, 0x27, 0x1A, 0x0D };

void main(void)
{
    u8 i;
    u8 rep;

    wdt_poll();
    dbg_putc('1'); /* main() entered */

    /* Establish TXRDY/RXE/framing exactly once, here, before the first
     * byte is ever sent -- see this file's header and uart.h. Every
     * later rate change below uses uart_set_rate() instead, which
     * never touches SCM again. */
    uart_init(rate_scc[0], rate_brg[0]);
    dbg_putc('2'); /* hardware UART (one-time) initialized */

    for (;;) {
        for (i = 0; i < NUM_RATES; i++) {
            wdt_poll();
            uart_set_rate(rate_scc[i], rate_brg[i]);
            dbg_putc('0' + i); /* which rate index is now live */

            for (rep = 0; rep < REPS_PER_RATE; rep++) {
                wdt_poll();
                /* Pattern sent as flat, un-nested uart_puts() calls
                 * directly in this loop (not a separate helper
                 * function) deliberately -- see this file's header
                 * note on the stack-frame budget. */
                uart_puts(rate_banner[i]);
                uart_puts("Hello, World!\r\n");
                uart_puts("ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n");
                uart_puts("abcdefghijklmnopqrstuvwxyz\r\n");
                uart_puts("0123456789\r\n");
                uart_puts("UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU\r\n");
                dbg_putc('.');
                dbg_long_delay();
            }
        }
    }
}
