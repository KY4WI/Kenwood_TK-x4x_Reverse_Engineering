/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* TK-840 replacement-firmware: minimal panel-link probe.
 *
 * See crt0.s's header for why this image exists: a deliberate regroup
 * after 13 betas of debugging our OWN bolted-on bit-bang debug-serial
 * channel without resolving why its first transmission never shows a
 * letter. This image contains NO debug-serial code at all -- no
 * debug_serial_asm.s, no P15/Timer1 bit-banging, nothing we invented.
 * It tests exactly the two things the project actually wants (display +
 * buttons) using only the resident mask-ROM's own panel link, already
 * proven on real silicon in every stock TK-840:
 *
 *   - panel_display_text() (resident 1F2D send) writes a fixed string
 *     to the 8-character display field once at boot.
 *   - the main loop echoes every raw byte the panel sends back (resident
 *     1F30/1F33 interrupt-driven receive) as two hex digits on the same
 *     display field -- a raw echo, not a decoded key map, same reasoning
 *     as the earlier selftest diagnostic (the key-code mapping is still [OPEN]).
 *
 * BETA_15: added the real panel sync handshake (panel_handshake(),
 * panel.c), traced directly from the mask ROM's own reset_handler
 * (0x0040) -- every stock boot sends 0xB0 and waits for the panel to
 * echo 0xB1 back immediately after EI, before ever sending a display-
 * text command. BETA_14's first cut of this image skipped that and
 * went straight to panel_display_text(), same as every build before
 * it; if the panel genuinely requires the handshake first, that alone
 * would explain a display that's never shown anything on any build.
 *
 * If "PANEL OK" (or "NO SYNC", or anything at all) appears on the real
 * LCD, the panel link itself is proven fine on this physical unit, and
 * the entire debug-serial mystery was contained in code this project
 * wrote, not in the resident link. If nothing appears even now, the
 * problem is upstream of (or inside) the panel link itself -- vector
 * wiring, the physical panel connector, interrupt baseline -- a
 * different question than anything the serial/watchdog debugging has
 * been chasing. Either answer is useful.
 *
 * BETA_16: real-hardware testing of BETA_15 came back with a blank
 * screen and nothing else to go on. panel_handshake() and
 * panel_display_text() are both bounded (2000-iteration waits,
 * watchdog-serviced throughout) -- they cannot hang forever, so they
 * ALWAYS return. With zero debug output anywhere in this image,
 * "blank screen" was ambiguous between "never reached main() at all"
 * and "ran every step fine, the panel genuinely isn't answering".
 * Added back a few characters of the already-proven (BETA_11/12)
 * debug_serial channel -- not the fragile multi-marker crt0
 * instrumentation that caused 13 betas of confusion in selftest/
 * phase1, just plain breadcrumbs on this image's much simpler path:
 * '0' (crt0.s, after every boot step, before calling here), '1'
 * (main() entered), '2' or '3' (panel_handshake()'s actual result),
 * '4' (panel_display_text() returned). Terminal: 2400 8N2, same as
 * selftest/phase1.
 *
 * BETA_17: BETA_16's "0000000" report (marker '0' from crt0 forever,
 * marker '1' -- the first statement INSIDE main() -- never once) traced
 * to a real stack-frame overflow, not a timing/framing bug. SP starts
 * at 0xFE20 (crt0.s); `call _main` spends 2 of those bytes on its own
 * return address, leaving exactly 0x1E (30) bytes before SP hits
 * 0xFE00 -- and 0xFE00 and below is NOT spare RAM, it's the external
 * codeplug/configuration flash (hardware.h), a different bus device
 * entirely. vbcc78k3's frame-size calculation for the previous version
 * of this function (one ternary feeding dbg_putc, one feeding
 * panel_display_text, plus an inline 9-byte hex-dump buffer with two
 * hex_nibble() calls) came out to 0x20 (32) bytes -- main() alone blew
 * the 30-byte budget by 2 bytes, before even reaching its own first
 * statement, every single boot. (selftest's equivalent main() needed
 * 0x2c/44 bytes -- same bug, worse, since it has more locals.) This
 * backend's codegen also doesn't reuse/coalesce dead expression-temp
 * stack slots within one function (confirmed by disassembly: every
 * intermediate value -- even a ternary's materialized result, copied
 * yet again into the call's own argument-staging slot -- gets its own
 * permanent byte(s) for the rest of the function), so the fix is
 * structural, not cosmetic: (1) no ternaries feeding a call argument
 * (each one cost a dedicated 2-byte temp on top of the argument slot
 * it was only ever going to be copied into); (2) the hex-dump buffer
 * and its hex_nibble() calls moved into their own small function
 * (display_key(), below) so they're off main()'s own frame entirely,
 * charged only transiently while display_key() is actually running,
 * never simultaneously with the handshake path above it. See panel.c's
 * BETA_17 note for the matching fix to panel_send_byte_safe() (was a
 * real function stacking its own 10-byte frame plus a 2-byte return
 * address on top of every caller in the chain -- now a macro, so that
 * whole extra CALL layer is gone from panel_handshake()'s and
 * panel_display_text()'s own worst-case depth too).
 *
 * BETA_18/19: real-hardware test of BETA_17 confirmed the stack fix
 * itself works -- main() finally runs, the handshake succeeds ('2'
 * marker), and the panel displays something for the first time ever.
 * But "PANEL OK" showed as "PQRSTUVW" (first char right, then +1 per
 * remaining position) and a key press echoed as "KLMNOPQR" (same
 * pattern). BETA_18's hypothesis -- the real panel needs more settle
 * time between back-to-back byte sends than our busy-bit check alone
 * gives it -- was tested directly with a generous watchdog-serviced
 * delay after every send, AND with this diagnostic payload (8 identical
 * '5' bytes, so any increment can only come from the transmission
 * itself, never from our data). Result: identical "56789" increment,
 * settle delay or not -- DISPROVEN, and reverted (panel.c).
 *
 * BETA_19: asked directly "why are we not doing exactly what the mask
 * ROM does" -- traced the real, confirmed-in-silicon CLONE-mode display
 * routine (sub_118d -> sub_15c8, the private mask-ROM analysis listing) and
 * found it does NOT pre-check the busy bit at all (relies entirely on
 * resident 1F2D's own internal wait -- ruling out any busy-check
 * discrepancy), but DOES run a PM1 (port-1 pin mode)/sfr[0xCA]/
 * sfr[0xCC] (interrupt-priority)/RAM-0xFEDE setup sequence immediately
 * after EI and before its first panel send -- a sequence this project
 * had never replicated anywhere. Added panel_link_arm() (panel.c/
 * panel.h/hardware.h) to do exactly that, called here before the
 * handshake, matching sub_118d's own placement. Kept the "55555555"
 * diagnostic payload for this round rather than restoring the real key
 * encoding, so this stays a clean single-variable test against BETA_18:
 * does the increment artifact go away now, or not.
 *
 * BETA_20: real-hardware test of BETA_19 came back identical to BETA_18
 * ("PQRSTUVW", "56789" on keypress) -- DISPROVEN. Re-reading sub_118d's
 * disassembly more carefully (not just the slice quoted in the BETA_19
 * note above) showed the PM1/CA/CC/FEDE sequence actually executes
 * AFTER sub_118d's own two panel sends (the 0x8E byte and the "CLONE"
 * text via sub_15c8), not before -- it was misread the first time. That
 * sequence is far more likely CLONE-mode's own prep for what it does
 * NEXT (flash programming), unrelated to the panel link, which is
 * consistent with it having made zero observable difference.
 * panel_link_arm() is removed (panel.c/panel.h/hardware.h).
 *
 * Re-reading reset_handler's (0x0040) ENTIRE boot sequence end-to-end,
 * rather than only its panel-adjacent lines, found two more mask-ROM
 * subroutines (0x9E, 0x144) that BOTH reset_handler and sub_118d
 * actually do call before EVER touching the panel, every single time,
 * that this project has never called anywhere -- see crt0.s's BETA_20
 * note for the full writeup and the actual fix, which lives entirely in
 * crt0.s (nothing for main() to call). Kept the "55555555" diagnostic
 * payload once more, so this is still a clean single-variable test.
 *
 * BETA_21: real-hardware test of BETA_20 came back IDENTICAL to BETA_19
 * and BETA_18 ("PQRSTUVW", "56789") -- DISPROVEN again. Three fixes in a
 * row (an inter-byte delay, a setup sequence at the wrong point anyway,
 * two missing boot-time calls) at three different points all around the
 * send, none changing the symptom at all, is itself informative: it
 * points at the send LOOP, not its surroundings. panel_display_text()
 * now matches sub_15C8 (mask ROM, section 11.14) structurally: no
 * per-byte busy-bit pre-check, just 9 flat calls to panel_send_byte(),
 * exactly like stock. See panel.c's BETA_21 note for the full reasoning.
 * Still "55555555" for one more clean comparison.
 *
 * BETA_22: real-hardware test of BETA_21 came back IDENTICAL yet again
 * ("PQRSTUVW", "56789") -- DISPROVEN. Four fixes in a row now, at four
 * different points at or around the send, all with zero effect,
 * reframes the question: maybe `0x02` alone was never a valid "accept
 * 8 bytes" command in the first place. sub_118D (section 11.14) sends
 * ONE other byte, `0x8E` (PANEL_CMD_CLONE_ANNOUNCE, hardware.h), before
 * its own only 8-byte burst -- never replicated here, and never
 * actually tested as a prerequisite rather than just "the CLONE
 * announcement's own first byte". Added panel_announce_text_mode()
 * (panel.c/panel.h), called once below, right after the handshake and
 * before the first panel_display_text() call -- matching sub_118D's
 * own placement (once, not per-burst, since that's all stock ever
 * does). If `0x8E` genuinely switches the panel into "accept full
 * 8-character writes" mode, this should fix BOTH the initial "PANEL OK"
 * display and the key-echo loop's later bursts, since both go through
 * panel_display_text() after this one call. Still "55555555" for one
 * more clean comparison.
 *
 * BETA_23: real-hardware test of BETA_22 came back identical again --
 * "PQRSTUVW", "56789". DISPROVEN. Five fixes/sequence changes in a row
 * now (BETA_18-22), all with zero effect on the display symptom. User
 * observation worth recording: real PROGRAM mode flashes "PROGRAM" on
 * the LCD by (apparently) clearing the screen and resending it
 * repeatedly, suggesting the panel may have its own distinct clear/
 * redraw command separate from the raw 0x02+8-byte write -- plausible,
 * but the external stock application firmware (where PROGRAM mode's
 * actual command bytes would live) is not in this project's current
 * environment to disassemble and confirm, so this isn't something to
 * guess a byte value for yet (see hardware doc section 26's source-set
 * list and section 22 item 5 for the same "don't have that image"
 * situation elsewhere in this project).
 *
 * This round is instrumentation, not another fix guess: added
 * hex_nibble() back and an unconditional serial echo of every raw
 * received panel byte (two hex digits, on the existing debug UART),
 * independent of whatever the display shows -- see the block below.
 * The point is to get a real key-press -> raw-code map AND confirm
 * panel_recv_byte() itself is returning sane, distinct values per key,
 * while the display payload stays the known "55555555" baseline.
 *
 * BETA_24: real-hardware test of BETA_23 gave the project its first new
 * forward progress in several betas -- the serial echo worked, and the
 * user bench-mapped all 8 physical buttons (hardware doc section 11.8,
 * press=N/release=N+1 confirmed for all eight). Separately, the tester
 * also located and re-supplied this project's three previously-lost
 * reference firmware images, most importantly the external TK-840
 * application firmware this document had been citing but could not
 * access -- see hardware doc section 1.1.1/17.5-17.7. Disassembling it
 * directly confirmed (a) the real byte-send primitive and the real
 * 8-character display-burst routine are both structurally IDENTICAL to
 * what this project already does (ruling out the send mechanics as the
 * display bug's cause for a sixth time), and (b) a real, disassembly-
 * confirmed list of TX-direction (main-to-panel) command bytes the
 * stock application actually sends, including the strongest-confirmed
 * pair (0x96/0x97, very likely TX on/off).
 *
 * This round replaces the fixed "55555555" diagnostic payload with 8
 * independent, button-triggered experiments -- one per bench-confirmed
 * key (panel.c/panel.h's BETA_24 functions) -- so several hypotheses,
 * including a requested search for a beep/annunciator command, can
 * be tried in one sitting instead of one rebuild/reflash per guess. The
 * serial hex echo from BETA_23 stays, unconditionally, for every raw
 * received byte -- it's still the ground truth for which button was
 * actually pressed, independent of whatever the display does.
 *
 * BETA_25: real-bench use of BETA_24 found the project's first clearly
 * visible protocol success -- the Sys UP cycling button changes real
 * LCD annunciators (AUX, TX, the telephone symbol, and others), some
 * flashing, with flashing state tracked independently per-icon (strong
 * evidence the panel MCU, not the main CPU, owns the blink timing --
 * hardware doc section 11.1's "panel drives its own LCD" architecture,
 * now bench-confirmed, not just schematic). No beep yet from that cycle.
 * Requested at the bench: replace the plain key-code echo with real
 * diagnostic content -- which byte each experiment actually sent --
 * since the key codes themselves are old news now. See panel.c's
 * BETA_25 header for what each experiment reports and why; the raw
 * hex-code echo is now only the fallback for anything NOT one of the 8
 * recognized experiments (so nothing received is ever silently
 * dropped). Also widened the secondary-frame experiment (0x8B family)
 * into a real sweep instead of two fixed values, since that command
 * family has had zero systematic testing so far and beeps remain
 * unconfirmed.
 *
 * BETA_26: full real-bench report on BETA_25 analyzed in detail (see
 * the hardware doc's 2026-10-08 addendum 6 for the complete writeup).
 * Three results worth a build change:
 *
 * 1. The Sys UP cycle produced a clean, fully consistent icon
 *    dictionary (TA/TEL/BUSY/CALL/S/triangle/OPT/AUX, each with an
 *    on/off byte, TEL and CALL additionally with a flash byte) -- no
 *    code change needed, this is pure analysis of BETA_25's own output.
 * 2. Two of BETA_24/25's garbling-detector tests used sequential
 *    content ('A','B' and '1'-'9') that is indistinguishable from the
 *    known garbling pattern's own output -- fixed in panel.c (see its
 *    BETA_26 notes on panel_test_short_burst()/
 *    panel_test_nine_byte_burst()).
 * 3. Requested at the bench: try the debug serial channel at real 9600 instead
 *    of the BETA_10-era 2400 -- a full bench session's worth of
 *    multi-byte traffic at 2400 (BETA_23-25) came through with zero
 *    glitches, and 2400's original justification predates BETA_17's
 *    stack-overflow fix (debug_serial_asm.s's dbg_wait_tick_x4 header
 *    has the full reasoning). Cheap, reversible experiment -- revert
 *    that one function if corruption reappears. Set the bench terminal
 *    to 9600 8N2 for this build.
 *
 * BETA_27: real bench report on BETA_26 came back with two results:
 *
 * 1. 9600 was garbage on the real bench (unlike every multi-byte test at
 *    2400 this whole project). Requested at the bench: revert to 2400,
 *    "play with serial later" -- debug_serial_asm.s's dbg_wait_tick_x4
 *    is back to its BETA_10-25 quartered-tick body. Set the bench
 *    terminal back to 2400 8N2 for this build.
 * 2. The fixed nine-byte-burst test (Aux) came back "ZMFTBQHW" -- the
 *    first 8 of the 9 sent bytes, in the EXACT right order, NOT the
 *    byte[0]+i garbling every other burst test still shows. Everything
 *    else was reported unchanged (still garbled). See panel.c's
 *    panel_test_nine_byte_burst() header and hardware doc addendum 7 for
 *    the full writeup -- this is the strongest, cleanest lead the
 *    display bug has ever produced, and the natural next experiment
 *    (not yet built: needs its own unambiguous test, not a reuse of
 *    this one) is whether appending any 9th byte to the real 8-
 *    character command fixes panel_display_text() generally.
 *
 * Also, per direct request: every panel_test_*() report (panel.c) is
 * reformatted this round from a terse 2-3 character tag into a full,
 * self-contained "<BUTTON> <what was sent> - " line starting on its own
 * fresh line (CRLF) -- meant to be copied straight out of a terminal
 * into a text file and filled in after the dash, no manual reformatting
 * needed. This file's own fallback case (below, for anything not one of
 * the 8 recognized press codes) now matches that same shape.
 *
 * BETA_28: real-bench test of BETA_27 came back with the debug serial
 * channel itself producing long runs of garbage/unprintable bytes --
 * the first time in 27 betas THIS channel has shown that symptom, and
 * confirmed not a baud issue (recognizable structure like "0123456789"
 * survived, meaning timing was right but the data was wrong). The one
 * new thing BETA_27 added was panel.c's dbg_send_str(), a loop. Lined
 * up against the panel display bug's whole history, every send this
 * project has ever seen garble is a loop re-reading its position from
 * the stack across several calls; every send that's ever come back
 * clean is a single byte or a flat, fully unrolled sequence of calls --
 * see panel.c's BETA_28 header and hardware doc section 11.18/addendum
 * 8 for the full writeup, including two new not-yet-built tests meant
 * to find out whether this is the SAME root cause as the central
 * display bug. dbg_send_str() is removed; every panel_test_*() report
 * is back to one dbg_putc() call per character (BETA_23-26's pattern),
 * same visible line format, no loop underneath.
 *
 * BETA_29: real-bench confirmation that BETA_28 fixed the serial
 * regression ("display works like before. Serial problems have been
 * fixed."), followed by a direct complaint about the icon-mapping
 * workflow itself: the old Sys UP candidate-cycle test required one
 * button press per candidate byte, with the tester manually watching the
 * panel and writing each result down -- "What is it actually sending
 * to the display? How am I supposed to evaluate this? Send 10 million
 * button presses ... Be smarter about our probing." Sys UP now runs
 * panel_test_icon_sweep() (panel.c) instead of
 * panel_test_beep_candidate_cycle(): one press sweeps 85 values
 * (0xAB-0xFF, the unexplored range above the already-confirmed icon
 * dictionary) automatically, ~1.3s apart (new dbg_long_delay(),
 * debug_serial_asm.s), logging each one over serial as it goes. The
 * user presses once and watches, instead of pressing once per value.
 *
 * BETA_30: real-bench test of BETA_29 found the blank-screen-around-
 * 0xAF/0xB0 result (likely PANEL_HANDSHAKE_SYNC_BYTE, 0xB0, dropping
 * the panel into a stuck "waiting for the rest of the handshake"
 * state -- see panel.c's BETA_30 header), but the tester pushed back
 * directly on the auto-sweep approach itself: "Your fast scan is too
 * fast... Grp up/dn to adjust the code displayed to the serial, and
 * aux or scan to submit the byte. With that, I could literally send
 * anything, and record what it does." Grp Up/Grp Dn/Scan now run a
 * real-time, single-byte picker (panel.c) instead of the timed sweep:
 * increment, decrement, send -- no pacing, no candidate list, entirely
 * at the tester's own pace. Scan submits rather than Aux specifically so
 * panel_test_nine_byte_burst() (this project's best lead on the main
 * display bug, section 11.17) stays on its own button, untouched.
 * panel_test_real_text()/announce_then_burst()/stock_string() are
 * retired -- each had already given its evidence, and the three
 * buttons they used are what the picker needed.
 *
 * BETA_31: the tester's first real bench session with the picker (BETA_30)
 * reported 0xB0 cleanly clearing the screen (not stuck handshake --
 * see hardware doc section 11.21, correcting the BETA_30 guess above)
 * and 0xB3 locking up the panel. Direct disassembly of the real stock
 * application image (hardware doc section 17.7) found why: 0xB3 is
 * ALWAYS followed by one more standalone byte at every one of its 20
 * confirmed call sites (sub-codes 0x01-0x0D observed) -- a two-byte
 * command, not a dangerous single value; the "lockup" is just the
 * panel waiting for a second byte that never came in time. Sys UP
 * (0x80, panel_test_icon_sweep() since BETA_29) is retired -- its own
 * 0xAB-0xFF sweep range unconditionally included 0xB3, so the sweep
 * itself was a latent, unattended lockup risk now that this is known
 * -- and replaced by panel_test_b3_subcode() (panel.c), which always
 * sends 0xB3 and the picker's current candidate together, back-to-
 * back, matching stock's own shape.
 *
 * RESOLVED (2026-10-16): the display garbling was a compiler bug, not
 * the panel link and not loops. vbcc78k3 never handled the address of a
 * static object (vbcc's VARADR flag): panel_display_text("PANEL OK") was
 * passed the string's first two bytes, 4150h ("PA"), as the pointer. A
 * read of that unmapped address on the multiplexed bus returns its own
 * low byte, so the text read 50h 51h 52h ... = "PQRSTUVW", and an
 * all-'5' string gave 3535h and "56789". Sends that built their bytes
 * on the stack or as constants (the 9-byte burst, single bytes) were
 * never affected, which is why the loop-vs-unrolled pattern looked
 * real; BETA_28's dbg_send_str() garbage was the same bug. Fixed in
 * KY4WI/vbcc 2945d2e; this image is rebuilt with it, with no source
 * change. */

#include "hardware.h"
#include "watchdog.h"
#include "panel.h"
#include "debug_serial.h"

/* BETA_23: hex_nibble() is back -- requested directly: echo every raw
 * received panel byte to the serial debug channel (as two hex digits),
 * in addition to the existing (still-diagnostic, still-"55555555")
 * display update. This is instrumentation, not a new fix hypothesis --
 * four fixes in a row (BETA_18-21) at different points around the send
 * and one protocol-sequence addition (BETA_22, the 0x8E mode-announce
 * byte) have all left the display symptom unchanged, so this round
 * adds visibility instead of another guess. The serial channel is
 * independent of whatever is wrong with the display burst (it's our
 * own bit-banged debug UART, nothing to do with the panel link), so it
 * shows the TRUE byte panel_recv_byte() actually returns for each key,
 * letting a real key-press -> raw-code map be read directly off a
 * terminal regardless of what the display shows. */
static char hex_nibble(u8 v)
{
    return (char)((v < 10) ? ('0' + v) : ('A' + v - 10));
}

void main(void)
{
    u8 synced;

    wdt_poll();

    /* BETA_16 breadcrumb: main() entered. dbg_init()/P15 were already
     * set up by crt0.s's own marker '0' (after 1F0F's PM1 clobber is
     * already behind us there), so no re-init needed here. */
    dbg_putc('1');

    /* BETA_15: perform the real panel sync handshake FIRST -- traced
     * directly from the mask ROM's own reset_handler
     * (0x0040), which sends this exact 0xB0/0xB1 exchange immediately
     * after EI and BEFORE ever sending a display-text command, on
     * every single stock boot. No build of this project has ever done
     * this; every previous attempt (including BETA_14's first cut of
     * this very image) went straight to panel_display_text(). If the
     * panel genuinely requires this handshake before it'll accept
     * anything else, that alone would explain a display that's never
     * shown anything across 14 betas regardless of what else was
     * wrong. Show the result either way -- a real negative answer
     * here ("NO SYNC") is as useful as a positive one.
     *
     * BETA_17: no more ternaries here (see header) -- plain if/else,
     * each branch a single call with a literal argument.
     *
     * BETA_20: panel_link_arm() (BETA_19) removed -- see this file's
     * header and crt0.s's BETA_20 note. Re-reading sub_118d's
     * disassembly more carefully showed its PM1/CA/CC/FEDE sequence
     * actually runs AFTER sub_118d's own panel sends, not before, so
     * it was never a plausible fix for THIS problem; replicating it
     * (BETA_19) correctly made no difference on real hardware. The new
     * candidate -- crt0.s's sub_sysclk_init()/sub_startup_wait() calls,
     * which both of the mask ROM's own panel-sending entry points DO
     * run before their first send -- lives entirely in crt0.s, with
     * nothing for main() to call directly.
     *
     * BETA_22: panel_announce_text_mode() (0x8E) sent once here, right
     * after the handshake and before the first panel_display_text()
     * call -- matching sub_118D's own one-time placement. See this
     * file's header and panel.c/panel.h/hardware.h for the full
     * writeup. */
    synced = panel_handshake();
    panel_announce_text_mode();
    if (synced) {
        dbg_putc('2');
        panel_display_text("PANEL OK");
    } else {
        dbg_putc('3');
        panel_display_text("NO SYNC");
    }
    dbg_putc('4');

    /* BETA_30: one-time init for the new byte picker (panel.c) -- sets
     * its starting candidate to 0x80 via a plain runtime assignment,
     * not a static initializer (same flash-vs-RAM rule as everywhere
     * else in this project). */
    panel_picker_reset();

    for (;;) {
        wdt_poll();

        if (panel_rx_available()) {
            u8 b = panel_recv_byte();

            /* BETA_25: requested at the bench -- "the serial display
             * just reports the key press ... why not give some more
             * diagnostic info instead of the key codes? We know what
             * the codes are now. ... sending XX to display, or
             * something like that." BETA_23/24's unconditional raw-hex
             * echo is now the FALLBACK only (anything not one of the 8
             * recognized press codes below -- release codes, or a
             * genuinely unexpected byte, still gets reported so nothing
             * is silently dropped). Each of the 8 recognized press
             * codes instead runs its experiment, which reports what it
             * actually SENT (panel.c's BETA_25 header) -- e.g. " C:A4"
             * for the cycling test's byte, not just the key code that
             * triggered it. That's the information actually needed
             * right now: which candidate byte produced which icon
             * change, without counting button presses by hand. */
            switch (b) {
                case 0x80: panel_test_b3_subcode();           break;
                case 0x82: panel_test_tx_toggle();            break;
                case 0x88: panel_test_picker_inc();           break;
                case 0x8A: panel_test_picker_dec();           break;
                case 0x90: panel_test_short_burst();          break;
                case 0x92: panel_test_secondary_frame();      break;
                case 0x98: panel_test_picker_send();          break;
                case 0x9C: panel_test_nine_byte_burst();      break;
                default:
                    /* BETA_27: same "fresh line, trailing dash" shape
                     * as every panel_test_*() report now uses (panel.c)
                     * -- a bare "RAW 0xNN - " line, not just two loose
                     * hex digits, so a release code or anything
                     * unrecognized pastes into a notes file the same
                     * way the 8 recognized ones do. */
                    dbg_putc('\r');
                    dbg_putc('\n');
                    dbg_putc('R');
                    dbg_putc('A');
                    dbg_putc('W');
                    dbg_putc(' ');
                    dbg_putc('0');
                    dbg_putc('x');
                    dbg_putc(hex_nibble((u8)(b >> 4)));
                    dbg_putc(hex_nibble((u8)(b & 0x0F)));
                    dbg_putc(' ');
                    dbg_putc('-');
                    dbg_putc(' ');
                    break;
            }
        }
    }
}
