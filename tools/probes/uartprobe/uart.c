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

/* Bit 7 of STIC: transmit-complete flag. Confirmed from the external
 * application's own proven send primitive (hardware doc 12.4, mask
 * address 0x8178: "clr1 sfr[0xde].7 / mov sfr[0x57],a / ... / bf
 * sfr[0xde].7"). */
#define STIC_TX_DONE 0x80u

void uart_init(void)
{
    /* Mask ROM's own proven order and values (reset_handler writes
     * these three unconditionally every boot, hardware doc 12.4) --
     * rewritten here explicitly rather than trusted as residual state
     * left over from that earlier pass. SCC/BRG first (baud setup),
     * SCM last (mode/enable), matching reset_handler's own relative
     * order. */
    SCC = 0x04;
    BRG = 0x4E;
    SCM = 0xCD;
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
