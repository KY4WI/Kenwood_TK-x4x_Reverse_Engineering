/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "uart.h"
#include "watchdog.h"

#define SFR8(addr) (*(volatile u8 *)(addr))

#define SCM  SFR8(0xFF50)
#define SCC  SFR8(0xFF52)
#define BRG  SFR8(0xFF53)
#define TXB  SFR8(0xFF57)
#define STIC SFR8(0xFFDE)

/* Bit 7 of STIC: transmit-complete flag (STF), confirmed by name
 * against the real uPD78312A(A) User's Manual's SEIC/SRIC/STIC figure
 * -- see uart.h. */
#define STIC_TX_DONE 0x80u

/* Fixed, already-proven framing: asynchronous mode, 8-bit characters,
 * no parity, 2 stop bits (8N2 -- see uart.h for the bit-by-bit
 * decode). Never changes across the sweep, only SCC/BRG (speed) do. */
#define SCM_ASYNC_8N2 0xCDu

void uart_init(u8 scc_val, u8 brg_val)
{
    SCC = scc_val;
    BRG = brg_val;
    SCM = SCM_ASYNC_8N2; /* TXRDY/RXE established here, ONCE -- see
                           * uart.h. Never rewritten again. */
}

void uart_set_rate(u8 scc_val, u8 brg_val)
{
    /* Deliberately SCC/BRG only -- no SCM write here. See uart.h for
     * why touching SCM (TXRDY) again mid-communication is the
     * documented hazard that caused the original garbage-at-every-rate
     * bug. */
    SCC = scc_val;
    BRG = brg_val;
}

void uart_send_byte(u8 c)
{
    u8 stic = STIC;
    stic &= ~STIC_TX_DONE;
    STIC = stic;

    TXB = c; /* starts the real transmission */

    for (;;) {
        if (STIC & STIC_TX_DONE) break;
        wdt_poll();
    }
}

void uart_puts(const char *s)
{
    /* Flat loop -- see uart.h. Safe here specifically because this
     * image never calls panel_send_byte()/1F2D, so the panel-link
     * interrupts (1F30/1F33) the still-open panel display bug centers
     * on (hardware doc 11.18/22.13) are never triggered in this image
     * at all, and uart_send_byte() above uses pure polling with no
     * UART interrupt enabled either -- there is no comparable
     * asynchronous activity for this loop to collide with. */
    while (*s) {
        uart_send_byte((u8)*s);
        s++;
    }
}
