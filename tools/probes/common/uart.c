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

#define STIC_TX_DONE 0x80u   /* STIC bit 7: transmit finished (STF) */

/* Asynchronous, 8 data bits, no parity, 2 stop bits, transmitter and
 * receiver enabled: the mask ROM's value. */
#define SCM_ASYNC_8N2 0xCDu

void uart_init(u8 scc_val, u8 brg_val)
{
    SCC = scc_val;
    BRG = brg_val;
    SCM = SCM_ASYNC_8N2;
}

void uart_init_framed(u8 scc_val, u8 brg_val, u8 scm_val)
{
    SCC = scc_val;
    BRG = brg_val;
    SCM = scm_val;
}

void uart_set_rate(u8 scc_val, u8 brg_val)
{
    SCC = scc_val;
    BRG = brg_val;
}

void uart_send_byte(u8 c)
{
    u8 stic = STIC;
    stic &= ~STIC_TX_DONE;
    STIC = stic;

    TXB = c;

    for (;;) {
        if (STIC & STIC_TX_DONE) break;
        wdt_poll();
    }
}

void uart_puts(const char *s)
{
    while (*s) {
        uart_send_byte((u8)*s);
        s++;
    }
}
