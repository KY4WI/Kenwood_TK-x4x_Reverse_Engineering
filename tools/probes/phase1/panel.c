/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "panel.h"
#include "resident_api.h"
#include "watchdog.h"

/* Bounded wait for the resident link to go idle before calling into
 * panel_send_byte() (resident 1F2D). 1F2D's own internal wait for the
 * PREVIOUS transfer's busy flag (FEDA.1) to clear has NO TIMEOUT (see
 * this file's header and the hardware doc) -- it only clears once the
 * panel ISR machinery (1F30/1F33) actually runs, and 1F33's real
 * interrupt source is still unconfirmed (crt0.s leaves it unwired
 * rather than guess, see that file's "KNOWN OPEN GAP" note). Until
 * that's resolved, calling panel_send_byte() a second time while busy
 * is genuinely still set would hang forever INSIDE the resident
 * routine, where nothing -- not even the watchdog, whose NMI vector is
 * also a bare RETI stub in this build -- can get control back. So:
 * poll the busy bit OURSELVES first, servicing the watchdog every
 * iteration, and simply give up on this byte (and every later byte in
 * this call) rather than ever entering 1F2D while busy. This is a
 * diagnostic safety net, not a fix for the display itself not
 * updating -- see panel.h's header comment. */
#define PANEL_SEND_MAX_WAIT 2000u

static u8 panel_send_byte_safe(u8 b)
{
    u16 i;
    for (i = 0; i < PANEL_SEND_MAX_WAIT; i++) {
        if (!(PANEL_STATE_FEDA & PANEL_TX_BUSY_BIT)) {
            panel_send_byte(b);
            return 1;
        }
        wdt_poll();
    }
    return 0;
}

u8 panel_rx_available(void)
{
    return PANEL_STATE_FEDA & PANEL_RX_PENDING_BIT;
}

u8 panel_recv_byte(void)
{
    u8 result = PANEL_RX_BYTE;
    u8 fedd = PANEL_STATE_FEDD;
    u8 feda = PANEL_STATE_FEDA;
    u8 new_fedd;

    /* FEDD.0 -> FEDA.2 */
    if (fedd & PANEL_QUEUE_BIT0) feda |= PANEL_RX_PENDING_BIT;
    else feda &= ~PANEL_RX_PENDING_BIT;
    PANEL_STATE_FEDA = feda;

    /* FEDD.1 -> FEDD.0 ; FEDD.2 -> FEDD.1 ; clear FEDD.2 -- other bits
     * of FEDD (outside this 3-bit queue-occupancy trio) are preserved
     * untouched. */
    new_fedd = fedd & ~(PANEL_QUEUE_BIT0 | PANEL_QUEUE_BIT1 | PANEL_QUEUE_BIT2);
    if (fedd & PANEL_QUEUE_BIT1) new_fedd |= PANEL_QUEUE_BIT0;
    if (fedd & PANEL_QUEUE_BIT2) new_fedd |= PANEL_QUEUE_BIT1;
    PANEL_STATE_FEDD = new_fedd;

    /* FEBC -> FE2C ; FEBD -> FEBC ; FEBE -> FEBD ; 0xFF -> FEBE */
    PANEL_RX_BYTE    = PANEL_RX_BACKUP1;
    PANEL_RX_BACKUP1 = PANEL_RX_BACKUP2;
    PANEL_RX_BACKUP2 = PANEL_RX_BACKUP3;
    PANEL_RX_BACKUP3 = PANEL_IDLE_BYTE;

    return result;
}

void panel_display_text(const char *text)
{
    u8 i;
    u8 done = 0;

    if (!panel_send_byte_safe(PANEL_TEXT))
        return;
    for (i = 0; i < 8; i++) {
        char c;
        if (!done) {
            c = text[i];
            if (c == 0) { done = 1; c = ' '; }
        } else {
            c = ' ';
        }
        if (!panel_send_byte_safe((u8)c))
            return;
    }
}
