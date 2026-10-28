/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* Front-panel bit-bang link (hardware doc section on panel send/
 * receive). Transport is resident (1F2D send / the panel ISRs 1F30/
 * 1F33 for receive, wired into the vector table by crt0.s); this
 * module adds the receive-queue pop algorithm and the display-text
 * helper on top of the confirmed resident_api.h/hardware.h primitives.
 *
 * IMPORTANT: panel_send_byte() (resident 1F2D) has NO TIMEOUT if the
 * panel ISR machinery isn't actually running -- see resident_api.h.
 * Do not call anything in this module before EXIC0 is unmasked and
 * the panel ISR vectors are wired up (crt0.s does both before jumping
 * to main()). */
#ifndef PANEL_H
#define PANEL_H

#include "hardware.h"

/* True if a received byte is available (FEDA.2, the primary pending
 * flag). Check before calling panel_recv_byte(). */
u8 panel_rx_available(void);

/* Pop one received byte from the resident receive queue, per the
 * confirmed pop algorithm (hardware doc): returns the primary slot
 * (FE2C) and shifts the three backup slots (FEBC/FEBD/FEBE) and their
 * occupancy flags (FEDD.0/1/2) up by one, per the exact sequence
 * documented from the stock queue-pop helper. Caller must have already
 * checked panel_rx_available(). */
u8 panel_recv_byte(void);

/* Send the 8-character main display text command (0x02 + 8 bytes).
 * `text` is truncated to 8 characters if longer, and space-padded if
 * shorter (matches the confirmed "8-character main field" framing). */
void panel_display_text(const char *text);

#endif
