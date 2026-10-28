/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "debug_serial.h"
#include "ui.h"
#include "panel.h"

/* All four of these are mutable statics with NO explicit initializer --
 * same rule as model.c's g_tx_inhibited (see that file's comment and
 * phase1.ld's ramdata_region comment): this is what routes them to
 * .bss/ramdata_region (real RAM, zeroed by the resident 1F0C call
 * before main() runs) instead of unwritable flash. 5 bytes total, well
 * inside ramdata_region's 32-byte budget. */
static u8  g_model;         /* model_id_t, narrowed to u8 */
static u8  g_rx_locked;
static u8  g_screen;        /* 0=model, 1=freq, 2=lock */
static u8  g_last_panel_byte;
static u16 g_tick;

/* How many ui_poll() calls to wait before advancing to the next screen.
 * Arbitrary -- there is no confirmed timing reference for "how fast
 * should the panel text rotate" anywhere in the hardware doc, and this
 * loop has no calibrated delay in it at all. Large enough that panel_send_byte()
 * (10 bytes per update, resident 1F2D, bit-banged) isn't called so often
 * it could starve the queue-drain loop in phase1.c; small enough that a
 * person watching the panel actually sees it cycle. Tune once real
 * hardware is in front of someone. */
#define UI_SCREEN_HOLD_TICKS 20000U

/* Fixed RX test channel, in the units a person reads off a radio
 * display (the RF channel, not the internal LO) -- same 450.000 MHz
 * value rf.h/rf.c derive RX_RF from, with real evidence (hardware doc
 * section 6.4's own worked PLL example). Kept as a string literal (goes
 * to flash/.data, read-only) rather than formatted at runtime -- this
 * project avoids runtime number-to-string conversion on this CPU where
 * a compile-time constant will do, same reasoning as pll.h/rf.c's
 * float/32-bit-arithmetic avoidance. If RF_TEST_CHANNEL in rf.c is
 * ever changed, this string must be updated by hand to match. */
#define UI_FREQ_TEXT "450.000"

/* Show a text on the display and mirror it to the debug output (CN3). */
static void show_text(const char *s)
{
    panel_display_text(s);
    dbg_show(s);
}

static void ui_show(void)
{
    switch (g_screen) {
    case 0:
        show_text(model_name((model_id_t)g_model));
        break;
    case 1:
        show_text(UI_FREQ_TEXT);
        break;
    default:
        /* No squelch case here on purpose -- see ui.h's file comment.
         * Only two real, confirmed facts exist about the RF state:
         * the channel (screen 1) and whether the PLL is locked to it
         * (this screen). */
        show_text(g_rx_locked ? "LOCK" : "UNLOCK");
        break;
    }
}

void ui_init(model_id_t model, u8 rx_locked)
{
    g_model = (u8)model;
    g_rx_locked = rx_locked;
    g_screen = 0;
    g_tick = 0;
    ui_show();
}

void ui_poll(void)
{
    g_tick++;
    if (g_tick >= UI_SCREEN_HOLD_TICKS) {
        g_tick = 0;
        g_screen++;
        if (g_screen > 2)
            g_screen = 0;
        ui_show();
    }
}

void ui_note_panel_byte(u8 b)
{
    /* Recorded only -- see ui.h's file comment for why this project
     * does not decode/dispatch it yet. g_last_panel_byte exists so a
     * future, evidence-based key handler has something real to read;
     * nothing in this codebase reads it back yet, which is deliberate,
     * not an oversight. */
    g_last_panel_byte = b;
}
