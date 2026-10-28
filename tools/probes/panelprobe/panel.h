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

/* Panel sync handshake -- confirmed by tracing the mask
 * ROM's own reset_handler (0x0040): it sends 0xB0 and waits for the
 * panel to echo 0xB1 back (RAM 0xFE26), retrying via resident 1F2A on
 * timeout, immediately after EI and BEFORE ever sending a display-text
 * command. No build of this project has ever performed this handshake
 * -- every previous attempt went straight to panel_display_text().
 * Returns 1 if the panel acked, 0 if it never did after retrying.
 * Call this once, right after EXIC0 is unmasked and interrupts are
 * enabled, before any other panel_* call. */
u8 panel_handshake(void);

/* BETA_19's panel_link_arm() declaration removed as of BETA_20 -- see
 * panel.c's header and crt0.s's BETA_20 note. It replicated a sequence
 * from sub_118d that, on closer re-reading, runs after sub_118d's own
 * panel sends rather than before, and real-hardware testing confirmed
 * it made no difference. */

/* BETA_22: sends PANEL_CMD_CLONE_ANNOUNCE (0x8E) alone, exactly as
 * sub_118D does right before its own first (and only) 0x02+8-byte
 * display write -- see hardware.h's PANEL_CMD_CLONE_ANNOUNCE comment.
 * Call this once, after the handshake, before the first
 * panel_display_text() call. */
void panel_announce_text_mode(void);

/* BETA_24: eight independent, button-triggered panel-protocol
 * experiments -- see panel.c's BETA_24 header for the full rationale
 * and hardware doc sections 11.8/17.5-17.7 for the evidence behind
 * each one. panelprobe.c's dispatch switch maps one of these to each of the
 * 8 bench-confirmed button press codes.
 *
 * BETA_30: panel_test_real_text(), panel_test_announce_then_burst() and
 * panel_test_stock_string() are retired (already gave their evidence,
 * see panel.c's BETA_30 header) -- Grp Up/Grp Dn/Scan now run the
 * real-time byte picker below instead.
 *
 * BETA_31: panel_test_icon_sweep() (was panel_test_beep_candidate_cycle()
 * through BETA_28) is ALSO retired -- the icon dictionary it existed to
 * complete is done, and its own sweep range unconditionally included
 * 0xB3, which locks up the panel when sent alone (see panel.c's BETA_31
 * header). Replaced on its button by panel_test_b3_subcode(), below. */
void panel_test_tx_toggle(void);
void panel_test_short_burst(void);
void panel_test_secondary_frame(void);
void panel_test_nine_byte_burst(void);

/* BETA_30: real-time, single-byte picker -- for manual exploration at the tester's own pace instead of an
 * automatic timed sweep. panel_picker_reset() is called once from
 * main() at boot (sets the starting candidate to 0x80); the other
 * three are wired to Grp Up (increment), Grp Dn (decrement) and Scan
 * (send the current candidate byte) in panelprobe.c's dispatch switch. See
 * panel.c's BETA_30 header for the full writeup. */
void panel_picker_reset(void);
void panel_test_picker_inc(void);
void panel_test_picker_dec(void);
void panel_test_picker_send(void);

/* BETA_31: sends PANEL_CMD_B3_PREFIX (0xB3) then the picker's current
 * candidate byte, back-to-back -- see panel.c's own header comment and
 * hardware.h's PANEL_CMD_B3_PREFIX comment for why 0xB3 must never be
 * sent alone. Set the candidate with Grp Up/Dn first, same as every
 * other picker use, then press this function's button to send the
 * pair. */
void panel_test_b3_subcode(void);

#endif
