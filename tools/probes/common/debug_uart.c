/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* debug_uart.c -- debug output (dbg_init, dbg_putc) on the hardware UART:
 * CN3 / KCT-19 TXD (P24), 115200 bps, 8 data bits, no parity, 1 stop bit.
 *
 * Used instead of debug_serial_asm.s (mic jack, P1.5) by images that run
 * the panel while printing. The mic-port version times its bits by
 * polling timer 1, so panel-link interrupts can stretch them, and it
 * drives the mic PTT line (hardware reference 4.1). The UART shifts the
 * bits out in hardware. uart.c is the code baudsweep proved at every
 * speed, 2026-10-16. */
#include "debug_serial.h"
#include "uart.h"

void dbg_init(void)
{
    uart_init_framed(UART_115200, UART_8N1);
}

void dbg_putc(u8 c)
{
    uart_send_byte(c);
}
