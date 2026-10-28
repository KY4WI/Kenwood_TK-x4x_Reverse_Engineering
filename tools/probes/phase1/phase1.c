/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* TK-840 replacement-firmware: Phase 1 built the toolchain
 * path end to end; Phase 2 (this file, ongoing) builds real receive-
 * only radio behavior on top of it -- model detection with a
 * permanent TX inhibit for unsupported hardware, confirmed-only
 * calibration application, a real RX PLL programming + lock-check pass
 * (rf.h), and now a real panel UI proving all of it (ui.h): model name,
 * RX test channel, and PLL lock status cycle on the confirmed 8-
 * character display field. KEY is never asserted anywhere in this
 * firmware -- TX is explicitly out of scope until Stage D (hardware
 * doc's own staging), and model.c's TX-inhibit latch plus ptt.c's
 * key_assert_safe() gate stand ready for when that day comes.
 *
 * crt0.s has already done, before calling here: DI, SP init, the three
 * confirmed resident startup calls (1F0F/1F0C/1F12), the initial
 * BU4094 safe-state commit, and the first watchdog kick. */

#include "hardware.h"
#include "watchdog.h"
#include "model.h"
#include "panel.h"
#include "outexp.h"
#include "eeprom.h"
#include "calibration.h"
#include "rf.h"
#include "ui.h"
#include "debug_serial.h"

extern const char build_tools[];   /* buildinfo.c, made by build.sh */

void main(void)
{
    u16 stack_check = 0;
    model_id_t model;
    u8 calibrated;
    u8 rx_locked;

    /* Bench-debug breadcrumb trail (see debug_serial.h): after BETA_04
     * came back completely dark, this is the only way to tell "never
     * got this far" from "ran fine, panel just silently no-oped" from
     * outside the radio. Polled bit-bang on P15, independent of the
     * display's unconfirmed 1F33 question. */
    dbg_init();
    dbg_puts("PHASE1 BOOT (DEBUG ON CN3 AT 115200 BPS, 8N1)");
    dbg_line("BUILT WITH: ", build_tools);

    wdt_poll();

    /* Model/variant detect -- doc's own rule: call 1F15, don't
     * duplicate its ADC thresholds. model_identify() decodes the
     * result (model.h: confirmed via real mask-ROM disassembly of
     * 1F15's implementation) and latches a permanent TX inhibit the
     * moment it ever sees an unsupported combination -- calling it
     * here, once, early, is what arms that latch for the rest of this
     * boot; ptt.c's key_assert_safe() consults it before any TX. */
    model = model_identify();
    dbg_line("MODEL: ", model_name(model));

    wdt_poll();

    /* Known-safe output-expander state -- same value the mask ROM's
     * own boot sequence writes (hardware.h's BU4094_RESET_VALUE),
     * already applied once by crt0 before this; re-asserted here so
     * main() doesn't depend on crt0 having done it. */
    outexp_write(BU4094_RESET_VALUE);
    dbg_puts("OUTPUT EXPANDER: RESET STATE");

    /* EEPROM state (moved here from the old selftest probe). */
    if (eeprom_is_blank())
        dbg_puts("EEPROM: BLANK (NEVER WRITTEN)");
    else
        dbg_hex_line("EEPROM: HAS DATA, CHECKSUM ", eeprom_checksum_all());

    wdt_poll();

    /* Apply the confirmed EEPROM calibration bytes (channels 2/3/6
     * only -- see calibration.h) to the DAC, unless the EEPROM reads
     * as never-formatted. Deliberately AFTER the output-expander reset
     * above (known-safe state first), BEFORE any RF/PLL step Phase 2
     * adds later that would actually depend on these being correct. */
    calibrated = calibration_apply_confirmed();
    dbg_puts(calibrated ? "CALIBRATION: APPLIED FROM EEPROM (DAC 2, 3, 6)"
                        : "CALIBRATION: SKIPPED");

    wdt_poll();

    /* Receive-only RF bring-up (rf.h): forces KEY inactive, selects
     * the RX VCO state, programs the PLL to the one channel this
     * project has real evidence for (see rf.c), and waits for lock.
     * Never asserts KEY regardless of outcome -- an unlocked PLL just
     * means the radio stays silent/mistuned on receive, not unsafe;
     * there is no TX path here to protect against. */
    rx_locked = rf_rx_bringup();
    dbg_puts("RECEIVE PLL: TUNED TO 450.000 MHZ (VCO 404.950 MHZ, D = 32396)");
    dbg_puts(rx_locked ? "RECEIVE PLL: LOCKED" : "RECEIVE PLL: NOT LOCKED");

    wdt_poll();

    /* Prove the receiver/UI (Phase 2 step 4, ui.h): sends the first
     * panel screen immediately, over the same confirmed panel_send_byte()
     * (resident 1F2D) / display-text framing Phase 1 proved, now carrying
     * real model/frequency/lock data instead of a fixed test string. */
    ui_init(model, rx_locked);
    dbg_puts("RUNNING: SCREENS AND KEYS FOLLOW");
    dbg_report_stack(0);   /* "STACK USED nn OF 92 BYTES" */

    for (;;) {
        wdt_poll();

        if (panel_rx_available()) {
            /* Drain the queue -- and now also record each byte (see
             * ui.h) so a future, evidence-based Sys/Group key dispatcher
             * has something real to build on. Still no key-dispatch
             * table (hardware doc: full key press/release/repeat
             * mapping is still [OPEN]) -- decoding a byte we don't
             * have a confirmed protocol for would be fabricating one,
             * same reasoning as squelch (section 14.3). */
            ui_note_panel_byte(panel_recv_byte());
        }

        ui_poll();
        if (++stack_check >= 1000) {   /* every 1000 passes: the scan is slow */
            stack_check = 0;
            dbg_report_stack(1);   /* again, only when it goes higher */
        }
    }
}
