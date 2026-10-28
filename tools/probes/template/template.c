/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* =====================================================================
 * TK-840/940/941 replacement-firmware: starting-point probe.
 *
 * Copy this whole `template/` directory to start your own probe (see
 * tools/probes/README.md's "Writing your own probe" section for the
 * full walkthrough). This file is the only one you need to understand
 * line by line -- crt0.s, panel.c, panel.h and hardware.h are borrowed
 * UNCHANGED from panelprobe/ (see build.sh): proven boot sequence and
 * panel-link primitives, already confirmed on real TK-840 hardware, so
 * you don't have to re-derive them. Read panelprobe/panelprobe.c's own
 * header if you want the full story of how each piece was confirmed
 * (31 "BETA" rounds of real-bench testing) -- none of that history is
 * repeated here on purpose, to keep this file short.
 *
 * What this probe does: on boot, performs the real panel-link
 * handshake and shows "HELLO" on the 8-character display if it
 * succeeds (or "NO SYNC" if it doesn't -- a real negative result, not
 * a crash, see panel.c). Then it idles forever, and whenever the panel
 * sends a byte (any button press or release), it shows that raw byte
 * on the display as two hex digits -- a ready-made "what does this
 * button actually send" tool. Replace the body of the loop below with
 * whatever your own probe needs to test.
 *
 * Safety: this probe never transmits, writes flash or writes the
 * EEPROM, and sends no panel command besides the proven handshake/
 * display-text/mode-announce sequence in panel.c. If you add your own
 * panel sends, read hardware.h's comment on PANEL_CMD_B3_PREFIX (0xB3)
 * first -- some panel bytes are two-byte commands, and sending the
 * first byte alone locks the panel up until power-cycle.
 * ===================================================================== */
#include "panel.h"
#include "watchdog.h"

/* Copied verbatim from panelprobe/panelprobe.c (BETA_23's hex_nibble):
 * uppercase hex, no table lookup. */
static char hex_nibble(u8 v)
{
    return (char)((v < 10) ? ('0' + v) : ('A' + v - 10));
}

/* Kept in its OWN function, off main()'s stack frame, on purpose -- see
 * panelprobe.c's BETA_17 note. main() starts with only 30 bytes of
 * stack before SP runs into the external codeplug/configuration flash
 * at 0xFE00 (hardware.h), and this backend (vbcc78k3) gives every local
 * variable and every intermediate expression value its own permanent
 * stack byte for as long as the function runs -- it does not reuse or
 * coalesce dead slots within one function. A function with more than
 * a local or two (this one has a 3-byte buffer) must not live inside
 * main() itself. */
static void display_byte_hex(u8 b)
{
    char line[3];
    line[0] = hex_nibble((u8)(b >> 4));
    line[1] = hex_nibble((u8)(b & 0x0F));
    line[2] = 0;
    panel_display_text(line);
}

void main(void)
{
    u8 synced;

    wdt_poll();

    /* Real panel sync handshake, then the one-time mode-announce --
     * see panel.c/panel.h for what each actually sends and why both
     * are required before the display will show anything. */
    synced = panel_handshake();
    panel_announce_text_mode();
    if (synced) {
        panel_display_text("HELLO");
    } else {
        panel_display_text("NO SYNC");
    }

    for (;;) {
        wdt_poll(); /* at least once per loop iteration -- see watchdog.h */

        if (panel_rx_available()) {
            u8 b = panel_recv_byte();
            display_byte_hex(b);
            /* Replace the line above with your own test: `b` is the
             * raw byte the panel just sent (a key press or release
             * code -- see include/panel_codes.h for the ones already
             * named, or hardware doc section 11 for how they were
             * found). */
        }
    }
}
