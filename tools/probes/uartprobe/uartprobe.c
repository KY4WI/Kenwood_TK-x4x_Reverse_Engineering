/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* =====================================================================
 * TK-840 replacement-firmware: uartprobe main.
 *
 * Bring-up test for the on-chip hardware UART (P24/TXD), now physically
 * connected on the bench unit for the first time. Sends "Hello world!"
 * out TXD, repeatedly, so it's visible whenever a terminal gets opened
 * rather than only in the instant right after boot.
 *
 * Also puts one '.' per send out the already-proven bit-banged P15
 * debug channel -- if that dot stream keeps going but nothing ever
 * shows up on the real UART, that tells us the loop itself is alive and
 * the problem is specific to the hardware UART (register setup, wiring,
 * or the baud-rate guess), not a boot/watchdog/crash problem. See
 * uart.h/uart.c for what is and isn't actually confirmed about this
 * peripheral's real bit rate -- SCC/BRG/SCM are the mask ROM's own
 * proven values, but the exact baud number they produce is not
 * confirmed from any datasheet this project has; start a terminal at
 * 9600 first.
 * ===================================================================== */
#include "uart.h"
#include "watchdog.h"
#include "debug_serial.h"

void main(void)
{
    wdt_poll();
    dbg_putc('1'); /* main() entered */

    uart_init();
    dbg_putc('2'); /* hardware UART (re-)initialized */

    for (;;) {
        wdt_poll();
        uart_puts("Hello world!\r\n");
        dbg_putc('.');
        dbg_long_delay();
    }
}
