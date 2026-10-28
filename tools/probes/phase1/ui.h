/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* Phase 2 step 4: "prove the receiver/UI" -- cycles the confirmed
 * front-panel 8-character text field (panel.h's panel_display_text(),
 * command 0x02) between three real, confirmed pieces of information:
 * model name (model.h, real mask-ROM-traced decode), the fixed RX test
 * channel (rf.h, real evidence-based frequency), and PLL lock status
 * (rf_rx_bringup()'s own real result, section 6.5's confirmed P32
 * polarity). Throttled internally so the panel link isn't flooded --
 * ui_poll() is meant to be called every main-loop iteration and only
 * actually sends a display update when the screen changes.
 *
 * DELIBERATELY NO SQUELCH SCREEN. The hardware doc (section 14.3, added
 * this same pass) now has a firm, traced conclusion: the squelch/noise-
 * pulse algorithm is confirmed absent from the mask ROM (CI0's own
 * interrupt sources, CRF00/CRF01, have no handler at all -- see the doc
 * for the full trace), not merely unsearched, and can only come from the
 * external stock application firmware this project's environment does
 * not have. hardware.h already warns against exactly this: "Do not add
 * a squelch_status() stub that reads a plausible-looking register --
 * that would be fabricating a hardware contract this project has not
 * confirmed." This module honors that -- there is no squelch_status()
 * anywhere in this codebase, and this UI simply does not claim to show
 * a reading that does not exist.
 *
 * Panel key events: ui_note_panel_byte() records the most recent raw
 * byte received from the panel (phase1.c's existing queue-drain loop
 * calls it), but does NOT decode or dispatch it. The hardware doc's own
 * open-item list (item 6 / section 11) still has full key press/release/
 * repeat mapping as [OPEN] -- inventing a Sys/Group key-code table here
 * would be the same kind of fabrication the squelch case above avoids.
 * This is the plumbing a future, evidence-based key dispatcher (once the
 * protocol is actually confirmed) can build on; for now it just proves
 * bytes really do arrive. */
#ifndef UI_H
#define UI_H

#include "hardware.h"
#include "model.h"

/* Call once after model_identify()/rf_rx_bringup() have both run, with
 * their results. Sends the first screen immediately. */
void ui_init(model_id_t model, u8 rx_locked);

/* Call every main-loop iteration. Internally throttled (see .c file) --
 * cheap to call often, only sends a panel update when the screen
 * actually advances. */
void ui_poll(void);

/* Record the most recent raw byte popped from the panel receive queue.
 * See file comment: recorded only, never decoded/dispatched here. */
void ui_note_panel_byte(u8 b);

#endif
