/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "panel.h"
#include "resident_api.h"
#include "watchdog.h"
#include "debug_serial.h"

/* BETA_19's panel_link_arm() (PM1/EXIC-CA/EXIC-CC/RAM-0xFEDE, replicating
 * what was believed to be sub_118d's pre-panel-send setup) is REMOVED as
 * of BETA_20: real-hardware testing disproved it, and a closer re-read of
 * sub_118d's disassembly showed that sequence actually runs AFTER
 * sub_118d's own panel sends, not before -- it was never going to be the
 * fix. See crt0.s's BETA_20 note for the real candidate (two mask-ROM
 * subroutines, 0x9E and 0x144, that this project had never called).
 *
 * Bounded wait for the resident link to go idle before calling into
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
 * updating -- see panel.h's header comment.
 *
 * BETA_17: this was a real `static u8 panel_send_byte_safe(u8)`
 * function until the stack-overflow postmortem below. It is now a
 * macro (same logic, textually expanded at each of its two call
 * sites) so it costs ONE shared frame merged into the caller's own
 * stack slots instead of a whole separate CALLed function's frame
 * stacked on top of the caller's. See panelprobe.c's header for the full
 * writeup of why every byte of CALL depth matters on this chip; the
 * short version is that main()'s own vbcc-generated frame (32 bytes in
 * this image) already came within 2 bytes of underflowing the actual
 * 30-byte budget (SP starts at 0xFE20, loses 2 to `call _main`'s own
 * return address, and 0xFE00 and below is NOT spare RAM -- it is the
 * external codeplug/configuration flash, a completely different bus
 * device), and panel_handshake() -> panel_send_byte_safe() ->
 * panel_send_byte() used to add another full CALL+frame layer (10
 * bytes of its own frame plus a 2-byte return address) on top of that
 * for no behavioral reason -- it's a tiny, always-inlined-in-spirit
 * helper, not a real separate subroutine. OUT_PARAM receives 1 on
 * success, 0 on timeout; callers check it with their own `if`
 * immediately after, exactly like the old `if (!panel_send_byte_safe(b))`
 * did. */
#define PANEL_SEND_MAX_WAIT 2000u

/* BETA_18: real-hardware test of BETA_17 (the stack fix) got further
 * than this project has ever gotten -- the panel actually displayed
 * something for the first time ever. But not the right thing: sending
 * "PANEL OK" showed "PQRSTUVW" (P is "PANEL OK"[0], then +1 per
 * position), and the KEY-echo path showed "KLMNOPQR" (K is its own
 * first byte, then +1 per position) when a button was pressed. Theory
 * at the time: a tight back-to-back burst of 9 sends with no inter-byte
 * margin might be too fast for the real panel hardware even though our
 * busy-bit check is satisfied. Added a deliberate, generous,
 * watchdog-serviced settle delay after every successful send to test
 * that theory directly.
 *
 * DISPROVEN: real-hardware retest with BOTH the settle delay AND a
 * fixed-pattern diagnostic payload (panelprobe.c, "55555555" -- 8 identical
 * bytes, so any increment could only come from the transmission itself,
 * never from our data) showed the exact same increment artifact
 * ("56789"), unchanged by the added delay. The panel hardware is not
 * timing-starved between bytes; something else is wrong. Reverted the
 * settle delay (removed below) rather than carry dead, misleading code
 * into the next test. See panelprobe.c's BETA_19 note and panel_link_arm()
 * above for the new leading hypothesis, found by direct comparison
 * against the real mask-ROM display routine (sub_118d/sub_15c8). */

#define PANEL_SEND_BYTE_SAFE(b, out_param) \
    do { \
        u16 _psbs_i; \
        (out_param) = 0; \
        for (_psbs_i = 0; _psbs_i < PANEL_SEND_MAX_WAIT; _psbs_i++) { \
            if (!(PANEL_STATE_FEDA & PANEL_TX_BUSY_BIT)) { \
                panel_send_byte((u8)(b)); \
                (out_param) = 1; \
                break; \
            } \
            wdt_poll(); \
        } \
    } while (0)

/* BETA_22: see panel.h/hardware.h. One plain, unchecked send -- exactly
 * matching sub_118D's own "mov a,#0x8e; call trampoline_panel_tx_queue"
 * with nothing before or after it, not even this project's own usual
 * busy-bit pre-check (sub_118D's caller context has nothing busy yet at
 * this point in boot, so there was never anything to wait for there
 * either). */
void panel_announce_text_mode(void)
{
    panel_send_byte(PANEL_CMD_CLONE_ANNOUNCE);
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

/* BETA_21: the busy-bit PRE-check (PANEL_SEND_BYTE_SAFE) is gone from
 * THIS function. Real-hardware results were identical across BETA_18
 * (added an inter-byte delay), BETA_19 (replicated a setup sequence
 * that turned out to run at the wrong point anyway), and BETA_20
 * (replicated two missing boot-time calls) -- three different fixes at
 * three different points in the boot/setup path, all with zero effect
 * on the symptom. That pattern -- same wrong output no matter what
 * changes around the edges of the send -- points at the send loop
 * ITSELF, not setup. Re-compared byte for byte against sub_15C8 (the
 * mask ROM's own "0x02 + 8 bytes" routine, section 11.14 of the
 * hardware doc): it is a FLAT sequence of 9 plain calls to 1F2D, with
 * NO busy-bit check of its own anywhere -- it relies entirely on 1F2D's
 * (sub_1714's) internal busy-wait. This project's panel_display_text()
 * has never matched that: PANEL_SEND_BYTE_SAFE adds a whole extra,
 * redundant C-level busy-wait loop (with its own wdt_poll() call) IN
 * FRONT of every single byte, including the 8 back-to-back bytes of
 * one burst -- work and timing sub_15C8 never does, between every byte
 * sub_15C8 sends with zero gap beyond 1F2D's own internal wait. If that
 * extra per-byte overhead (or the watchdog-service call inside it)
 * perturbs the panel-side state machine in a way a tight back-to-back
 * burst doesn't tolerate, this would be it -- and it is now the only
 * un-tried structural difference between our send path and stock's.
 * panel_handshake() (single, infrequent sends) is UNCHANGED and still
 * uses the safe macro below -- it has worked since BETA_17, so it's
 * left alone; only the burst path changes this round. */
void panel_display_text(const char *text)
{
    u8 i;
    u8 done = 0;
    const char *p = text;

    for (i = 0; i < 9; i++) {
        u8 c;
        if (i == 0) {
            c = PANEL_CMD_DISPLAY_TEXT;
        } else if (!done) {
            c = (u8)*p;
            p++;
            if (c == 0) { done = 1; c = ' '; }
        } else {
            c = ' ';
        }
        panel_send_byte(c);
    }
}

/* See panel.h -- real sequence confirmed from the mask ROM's own
 * reset_handler (sub_0159 at 0x0159): send 0xB0, then poll the same
 * RX-pending flag panel_recv_byte() uses (FEDA.2) for a response,
 * checking whether the byte the resident link actually received
 * (RAM 0xFE26 -- NOT panel_recv_byte()'s FE2C queue slot; the
 * handshake response lands in a different, separate RAM byte) is the
 * expected ack 0xB1. sub_0159 retries up to 5 times, calling 1F2A
 * (panel_rx_recover) between attempts -- replicated here with our own
 * already-proven bounded wait (panel.c's own PANEL_SEND_MAX_WAIT
 * pattern) rather than reconstructing sub_0159's exact internal timer
 * bookkeeping, which depends on sfr[0xd2] state this project has not
 * independently traced. */
#define PANEL_HANDSHAKE_RETRIES 5u

u8 panel_handshake(void)
{
    u8 retry;
    u8 ok;

    for (retry = 0; retry < PANEL_HANDSHAKE_RETRIES; retry++) {
        u16 i;

        PANEL_SEND_BYTE_SAFE(PANEL_HANDSHAKE_SYNC_BYTE, ok);
        if (!ok) return 0;

        for (i = 0; i < PANEL_SEND_MAX_WAIT; i++) {
            wdt_poll();
            if (PANEL_STATE_FEDA & PANEL_RX_PENDING_BIT) {
                PANEL_STATE_FEDA &= (u8)~PANEL_RX_PENDING_BIT;
                if (PANEL_HANDSHAKE_RESPONSE == PANEL_HANDSHAKE_ACK_BYTE)
                    return 1;
                break; /* got a response, but not the ack -- retry */
            }
        }

        panel_rx_recover();
    }
    return 0;
}

/* BETA_25: small serial-diagnostic helpers, local to this module (panelprobe.c
 * has its own identical hex_nibble() for the fallback echo of anything
 * these 8 experiments don't claim -- duplicated rather than shared
 * across a header, since each is static and tiny). Requested at
 * the bench: "the serial display just reports the key press ... why not
 * give some more diagnostic info instead of the key codes? ... sending
 * XX to display, or something like that." Each panel_test_*() below
 * now reports WHAT it actually sent, not just which button fired --
 * e.g. " C:A4" (cycle test, byte 0xA4) instead of a bare key code,
 * so a candidate byte can be matched to an observed icon change
 * without the tester having to count button presses by hand.
 *
 * BETA_27: that terse " C:A4"-style tag is gone -- requested at
 * the bench: "format the serial output, where I can just copy/paste it
 * into my text editor, then fill in the details of what changes." Every
 * report below now starts a FRESH line (CRLF) and reads as
 * "<BUTTON> <exactly what was sent> - ", e.g. "AUX ZMFTBQHWD - " --
 * one self-contained line per press, in the same "code - observation"
 * shape the tester's own bench notes already use, so a whole session's
 * terminal scrollback can be pasted straight into a text file and
 * filled in after the dash without any reformatting by hand.
 *
 * BETA_27 shipped a dbg_send_str() helper here (loop over a literal
 * string, one dbg_putc() per character) to build these lines without
 * repeating a dbg_putc() call per character at every site. BETA_28:
 * REMOVED, on real-bench evidence -- the very next test session came
 * back with long runs of garbage/unprintable bytes on the debug serial
 * line itself (not the panel display), the first time THIS channel has
 * ever shown that symptom in 27 betas. The one thing dbg_send_str()'s
 * loop has in common with THE central unsolved bug (panel_display_text(),
 * section 11.9/11.17 of the hardware doc) and nothing else in this file
 * has: it is a loop that re-reads a pointer/counter back OUT OF STACK
 * MEMORY on every iteration to decide what to send next, across
 * multiple back-to-back CALLs within one activation. Every OTHER
 * multi-byte send in this file (including the one burst that's ever
 * displayed correctly without garbling, panel_test_nine_byte_burst())
 * is fully unrolled -- a flat sequence of calls with compile-time-fixed
 * arguments, no shared mutable state read back from the stack between
 * them. Static inspection of both loops' generated code (panel.s) found
 * nothing wrong with either in isolation -- both correctly save/reload
 * their loop pointer across the `call` instruction -- so whatever is
 * going wrong is a real-hardware effect (most likely a timing/interrupt
 * collision given the already-tight stack margin, section 27) that a
 * fully unrolled, stack-light sequence is simply not exposed to, not a
 * logic bug visible from the source. Every panel_test_*() below is
 * un-looped back to one dbg_putc() call per character (matching
 * BETA_23-26's long-proven-safe pattern) until/unless this gets a real
 * explanation. See hardware doc addendum 8 for the full writeup --
 * this may be the same underlying mechanism as the central display bug,
 * not a new, unrelated one. */
static char hex_nibble(u8 v)
{
    return (char)((v < 10) ? ('0' + v) : ('A' + v - 10));
}

static void dbg_hex2(u8 v)
{
    dbg_putc((u8)hex_nibble((u8)(v >> 4)));
    dbg_putc((u8)hex_nibble((u8)(v & 0x0F)));
}

/* BETA_24: eight independent, button-triggered panel-protocol
 * experiments, enabled by two unrelated results landing in the same
 * session -- BETA_23's full 8-button bench map (hardware doc section
 * 11.8) gives a reliable on-demand trigger for each one, and the
 * recovered external stock application firmware (hardware doc section
 * 17.5-17.7) gives real, disassembly-confirmed command bytes to try
 * instead of more guesses. Each function below is self-contained (no
 * arguments, no return value) so main()'s own dispatch switch costs it
 * nothing beyond a plain CALL -- see panelprobe.c's BETA_17/23 notes on why
 * that matters on this chip. None of these touch panel_handshake() or
 * the boot-time panel_announce_text_mode() call; they are all later,
 * optional, repeatable probes.
 *
 * panel_test_beep_candidate_cycle() (BETA_24-28): sent ONE byte per
 * call, from the hardware doc's section 17.7 TX-direction candidate
 * list, advancing one step further each time this button was pressed
 * -- 18 candidates, 18 separate button presses to see them all, one at
 * a time, each requiring the tester to press, look at the panel, and
 * write it down before the next press. Flagged at the bench as
 * unworkable for actually mapping the display out: "What is it
 * actually sending to the display? How am I supposed to evaluate this?
 * Send 10 million button presses ... Be smarter about our probing."
 *
 * panel_test_icon_sweep() (BETA_29) replaces it: ONE press now runs an
 * entire sweep by itself. 85 values (0xAB-0xFF, the whole unexplored
 * range above the already-confirmed icon dictionary, section 11.15 --
 * no longer the narrow 18-byte candidate list, which has nothing left
 * to teach us now that it's fully mapped), one `panel_send_byte()`
 * every ~1.3 seconds (`dbg_long_delay()`, debug_serial_asm.s), each one
 * logged over serial as it goes. The tester presses this button once,
 * then just watches the panel for about two minutes -- no further
 * button presses needed, and the serial log (if captured) gives the
 * exact value and send order to cross-reference afterward. No more
 * cross-call state needed (no static index) since the whole sweep now
 * runs inside one button-press's single call. `0x96`/`0x97` (the
 * strongest-confirmed pair, already named PANEL_CMD_TX_ON/OFF) stay out
 * of range -- see panel_test_tx_toggle() below for those, on their own
 * button. */
/* RETIRED BETA_31: the icon dictionary this sweep existed to complete
 * is now done (hardware doc section 11.21 -- every icon has a directly
 * -observed on/off/flash triplet; nothing left in 0xAB-0xFF worth
 * sweeping blind). More importantly, this sweep's own range included
 * 0xB3 -- which BETA_30's bench session separately found LOCKS UP the
 * panel when sent alone (see panel_test_b3_subcode() below for why).
 * Leaving this function live meant one button press could hang the
 * panel unattended, partway through an otherwise-automatic sweep, with
 * no one necessarily watching when it happened. Replaced below by a
 * function that investigates 0xB3 safely instead of walking past it. */

/* Sanity check: toggle the strongest-confirmed TX-direction pair
 * (0x96/0x97, hardware doc section 11.13/17.7). If this visibly
 * changes the TX annunciator on the real panel, it proves single-byte
 * command control works end-to-end on THIS bench unit for a byte
 * outside the already-proven 0xB0/0xB1 handshake -- useful context for
 * interpreting every other experiment here, independent of the still-
 * unresolved multi-byte burst bug. */
void panel_test_tx_toggle(void)
{
    /* See panel_test_beep_candidate_cycle() above -- no explicit
     * initializer, for the same .data-vs-.bss reason. */
    static u8 on;

    u8 sent;

    if (on) {
        sent = PANEL_CMD_TX_OFF;
        on = 0;
    } else {
        sent = PANEL_CMD_TX_ON;
        on = 1;
    }
    panel_send_byte(sent);

    dbg_putc('\r'); dbg_putc('\n'); dbg_putc('S'); dbg_putc('Y'); dbg_putc('S');
    dbg_putc('D'); dbg_putc('N'); dbg_putc(' '); dbg_putc('T'); dbg_putc('X');
    dbg_putc('='); dbg_putc('0'); dbg_putc('x');
    dbg_hex2(sent);
    dbg_putc(' '); dbg_putc('-'); dbg_putc(' ');
}

/* BETA_30: panel_test_real_text() ("TESTABCD") and
 * panel_test_announce_then_burst() (0x8E + "REBURST ") retired --
 * requested at the bench , replaced by the byte picker below. Both had
 * already given their evidence (another confirmation of the same
 * byte[0]+i garbling signature, section 11.16-11.18); re-running either
 * again wasn't going to teach anything new, and the tester wanted these
 * two buttons for real-time manual control instead. If the dedicated
 * loop-vs-unrolled/trailing-byte A/B tests from section 11.18 get built
 * later, they'll be fresh functions, not a revival of these two. */

/* BETA_30: real-time, single-byte picker -- requested at the bench,
 * replacing the automatic timed sweep (panel_test_icon_sweep(),
 * BETA_29) for exploratory work: "Your fast scan is too fast... Grp
 * up/dn to adjust the code displayed to the serial, and aux or scan to
 * submit the byte. With that, I could literally send anything, and
 * record what it does." No pacing, no candidate list -- the tester picks
 * the byte, the tester decides when to send it, and can single-step
 * through any specific region (e.g. the 0xB0 "handshake sync byte"
 * neighborhood, section 11.18's "screen goes blank around AF/B0"
 * report -- 0xB0 is PANEL_HANDSHAKE_SYNC_BYTE; sending it outside the
 * real handshake sequence likely drops the panel into "waiting for the
 * rest of the handshake", not a crash -- power-cycling the radio should
 * clear it).
 *
 * picker_candidate (static, no initializer -- same flash-vs-RAM rule as
 * every other static in this file) holds the pending byte, plain u8 so
 * increment/decrement wrap naturally at the 0x00/0xFF boundary (well-
 * defined unsigned arithmetic, no loop or pointer involved -- a much
 * simpler, lower-risk pattern than anything in section 11.18's history).
 * panel_picker_reset() sets it to a useful starting point (0x80, the
 * start of the whole known command-byte neighborhood: 0x8B/0x8E/0x90-
 * 0xAA/0x96-0x97/0xB0-0xB1) via a plain runtime assignment from main()
 * at boot -- NOT a static initializer, which would silently land in
 * flash (the same rule, applied to startup state instead of a fixed
 * zero). Scan submits rather than Aux so panel_test_nine_byte_burst()
 * -- this project's best lead on the main display bug (section 11.17)
 * -- stays untouched on its own button. */
static u8 picker_candidate;

void panel_picker_reset(void)
{
    picker_candidate = 0x80;
}

void panel_test_picker_inc(void)
{
    picker_candidate++;

    dbg_putc('\r'); dbg_putc('\n'); dbg_putc('C'); dbg_putc('A'); dbg_putc('N');
    dbg_putc('D'); dbg_putc(' '); dbg_putc('0'); dbg_putc('x');
    dbg_hex2(picker_candidate);
    dbg_putc(' '); dbg_putc('-'); dbg_putc(' ');
}

void panel_test_picker_dec(void)
{
    picker_candidate--;

    dbg_putc('\r'); dbg_putc('\n'); dbg_putc('C'); dbg_putc('A'); dbg_putc('N');
    dbg_putc('D'); dbg_putc(' '); dbg_putc('0'); dbg_putc('x');
    dbg_hex2(picker_candidate);
    dbg_putc(' '); dbg_putc('-'); dbg_putc(' ');
}

void panel_test_picker_send(void)
{
    panel_send_byte(picker_candidate);

    dbg_putc('\r'); dbg_putc('\n'); dbg_putc('S'); dbg_putc('E'); dbg_putc('N');
    dbg_putc('T'); dbg_putc(' '); dbg_putc('0'); dbg_putc('x');
    dbg_hex2(picker_candidate);
    dbg_putc(' '); dbg_putc('-'); dbg_putc(' ');
}

/* BETA_31: investigates PANEL_CMD_B3_PREFIX (0xB3) now that direct
 * disassembly of the real stock application image (hardware doc
 * section 17.7) shows it is always followed by exactly one more
 * standalone byte at every one of its 20 confirmed call sites (sub-
 * codes 0x01 through 0x0D observed so far) -- a two-byte command, not
 * a single dangerous value. BETA_30's bench report that 0xB3 alone
 * "locks up" the panel is consistent with sending only the first half
 * of that command and then never supplying the second half fast
 * enough (human button-press speed, not back-to-back calls) for the
 * panel's own wait-for-second-byte state to see it arrive.
 *
 * Sends 0xB3 then whatever byte Grp Up/Dn has set as the picker's
 * current candidate (picker_candidate, shared with the picker
 * functions above) -- two flat, back-to-back panel_send_byte() calls,
 * no loop, no delay in between, matching both this project's own
 * loop-vs-unrolled safety discipline (hardware doc section 11.18) AND
 * the real stock code's own shape at every 0xB3 call site. Replaces
 * panel_test_icon_sweep() (see its retirement note above) on this
 * button -- set a candidate with Grp Up/Dn first (e.g. 0x01), then
 * press this button to send 0xB3 + that candidate as one pair; step
 * the candidate and repeat to work through 0x01-0x0D one pair at a
 * time, same real-time, user-paced control the picker already gives
 * every other byte. Never calls panel_send_byte(0xB3) without an
 * immediate second byte -- if a future change ever needs to, it must
 * keep them back-to-back like this, never split across two separate
 * button presses.
 *
 * BETA_32: bench-confirmed -- sub-code 0x0E (one past the highest
 * value, 0x0D, stock code was ever seen sending) ALSO locks up the
 * panel, the same way 0xB3 alone did. hardware.h's PANEL_B3_SUBCODE_MIN/
 * MAX now bound this to the exact range stock code itself uses; this
 * function refuses to send anything outside it (a distinct "SKIP"
 * report, not a silent no-op, so an out-of-range candidate is obvious
 * in the serial log rather than looking like the command did nothing).
 * Do NOT widen this range without new bench evidence -- hardware doc
 * section 11.23.
 *
 * BETA_33: BETA_32's guard above was WRONG -- not a C-logic mistake,
 * a genuine vbcc78k3 CODE-GENERATOR BUG, fully root-caused: see
 * KY4WI/vbcc machines/78k3/BUGS.md (bug #1) for the complete writeup. In one
 * sentence, `if (sub < MIN || sub > MAX)` compiled to a SIGNED 8-bit
 * branch instruction for an UNSIGNED comparison, so every candidate
 * with the top bit set (0x80-0xFF -- 0x8E, 0x8F, 0x9E, 0xAE, 0xCD,
 * 0xCE all bench-confirmed) was silently treated as "less than or
 * equal to 13" and SENT anyway, exactly the opposite of the guard's
 * purpose. BETA_32 shipped with this guard genuinely untested against
 * any candidate at or above 0x80 -- an oversight on top of the
 * compiler bug itself, since by that point several bench-confirmed
 * "dangerous above 0x80" bytes (0xB0 region, 0xB3 itself) were
 * already documented and should have been the first things tried
 * against the new guard, not assumed safe from source-reading alone.
 *
 * This rewrite avoids the bug entirely rather than working around one
 * specific symptom of it: b3_subcode_valid() below uses a `switch`
 * over the 13 literal values instead of a `<`/`<=`/`>`/`>=` range
 * check. Confirmed by direct inspection of the compiled assembly
 * (both in isolation and in this exact function) that a `switch` over
 * small integer cases compiles to a chain of plain equality compares
 * (`cmp`/`be`/`bne`) -- the ONE class of comparison this bug cannot
 * touch, since BEQ/BNE's hardware condition is defined directly from
 * the zero flag, with no signed/unsigned distinction to get wrong in
 * the first place (see cond_branch_mnemonic()'s own comment in
 * vbcc78k3's machine.c). */
static u8 b3_subcode_valid(u8 sub)
{
    switch (sub) {
        case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
        case 0x06: case 0x07: case 0x08: case 0x09: case 0x0A:
        case 0x0B: case 0x0C: case 0x0D:
            return 1;
        default:
            return 0;
    }
}

void panel_test_b3_subcode(void)
{
    u8 sub = picker_candidate;

    if (!b3_subcode_valid(sub)) {
        dbg_putc('\r'); dbg_putc('\n'); dbg_putc('B'); dbg_putc('3');
        dbg_putc(' '); dbg_putc('S'); dbg_putc('K'); dbg_putc('I'); dbg_putc('P');
        dbg_putc(' '); dbg_putc('0'); dbg_putc('x');
        dbg_hex2(sub);
        dbg_putc(' '); dbg_putc('-'); dbg_putc(' ');
        return;
    }

    panel_send_byte(PANEL_CMD_B3_PREFIX);
    panel_send_byte(sub);

    dbg_putc('\r'); dbg_putc('\n'); dbg_putc('B'); dbg_putc('3'); dbg_putc('+');
    dbg_putc('0'); dbg_putc('x');
    dbg_hex2(sub);
    dbg_putc(' '); dbg_putc('-'); dbg_putc(' ');
}

/* A short, 2-character burst (0x02 + 2 bytes instead of + 8) -- tests
 * whether the garbling is burst-LENGTH dependent rather than inherent
 * to the 0x02 command itself.
 *
 * BETA_26: payload changed from 'A','B' to 'M','7' -- a BETA_24/25
 * bench round showed the garbling bug's own signature (byte[i] becomes
 * byte[0]+i, independent of actual content) is INDISTINGUISHABLE from
 * correct output whenever the test payload is itself sequential
 * ('A','B' with 'B' == 'A'+1 is exactly what the bug would also
 * produce). 'M' (0x4D) and '7' (0x37) differ by more than one in
 * either direction, so a garbled result ('M','N') and a correct one
 * ('M','7') now look different on the display. */
void panel_test_short_burst(void)
{
    panel_send_byte(PANEL_CMD_DISPLAY_TEXT);
    panel_send_byte('M');
    panel_send_byte('7');

    dbg_putc('\r'); dbg_putc('\n'); dbg_putc('V'); dbg_putc('O'); dbg_putc('L');
    dbg_putc('U'); dbg_putc('P'); dbg_putc(' '); dbg_putc('M'); dbg_putc('7');
    dbg_putc(' '); dbg_putc('-'); dbg_putc(' ');
}

/* The stock app's own secondary/status 3-byte frame (hardware doc
 * section 17.6, 0xA531): PANEL_CMD_SECONDARY_FRAME + 2 data bytes. A
 * completely different command family from 0x02, sent over the
 * identical transport -- tests whether short, non-text multi-byte
 * frames fare any differently from the 8-character text burst, AND
 * (BETA_25) sweeps the first data byte 0x00-0x1F one step per press --
 * a command family BETA_24 only ever sent two fixed, arbitrary values
 * for. The real 0xA531 code (section 17.6) compares its own equivalent
 * of this first data byte against small constants (0x09, 0x21), so a
 * 0x00-0x1F range is a real, evidence-based sweep bound, not an
 * arbitrary one. Second data byte stays fixed at 0x00 for now -- a
 * 32x32 full sweep is for later if this range alone doesn't turn up
 * anything. */
void panel_test_secondary_frame(void)
{
    static u8 idx;
    u8 b1 = idx;

    panel_send_byte(PANEL_CMD_SECONDARY_FRAME);
    panel_send_byte(b1);
    panel_send_byte(0x00);

    if (idx >= 0x1F) idx = 0; else idx++;

    dbg_putc('\r'); dbg_putc('\n'); dbg_putc('V'); dbg_putc('O'); dbg_putc('L');
    dbg_putc('D'); dbg_putc('N'); dbg_putc(' '); dbg_putc('8'); dbg_putc('B');
    dbg_putc(','); dbg_putc('0'); dbg_putc('x');
    dbg_hex2(b1);
    dbg_putc(','); dbg_putc('0'); dbg_putc('0'); dbg_putc(' '); dbg_putc('-');
    dbg_putc(' ');
}

/* BETA_30: panel_test_stock_string() ("AUTO TEL", the real stock ROM's
 * own display string) retired -- Scan is now the byte picker's submit
 * button (panel_test_picker_send(), above). Like real_text/
 * announce_then_burst, it had already given its evidence (the same
 * garbling signature, this time with real stock content, ruling out
 * "maybe our own test strings are special" -- section 11.16); nothing
 * new left to learn by re-running it. */

/* One byte MORE than the documented 8-character field (cmd + 9 data
 * bytes instead of cmd + 8) -- tests whether the panel actually wants
 * a 9th byte (a terminator?) this project has never sent.
 *
 * BETA_27 RESULT (the first clean positive result this specific bug has
 * ever gotten): with the ambiguity fixed below, the real bench result
 * was "ZMFTBQHW" -- the first 8 of the 9 sent bytes, in the EXACT
 * correct order, not the byte[0]+i garbling signature every other
 * burst test still shows. This is a fundamentally different result
 * shape than panel_test_real_text()'s "TUVWXYZ[" from the identical-
 * length-content "TESTABCD" (cmd + 8 data bytes, no trailing 9th byte):
 * that one is still garbled, this one is not. The one structural
 * difference between the two calls is the 9th data byte sent here and
 * never sent there -- real, bench-confirmed evidence (not yet a fix)
 * that the display burst may need one byte PAST the 8 visible
 * characters to actually commit/flush the last real character into the
 * display, consistent with this function's own original "a
 * terminator?" guess above. NOT yet turned into a change to
 * panel_display_text() itself -- that needs its own dedicated,
 * unambiguous-content test (send exactly "TESTABCD" + one extra
 * trailing byte and confirm it, rather than reusing this fixed 9-char
 * literal) before touching the function every other display call in
 * this project relies on. See hardware doc addendum 7 for the full
 * writeup.
 *
 * BETA_26: payload changed from '1'-'9' to a deliberately non-
 * sequential 9-character set, same reasoning as panel_test_short_burst()
 * above -- '1'..'9' is exactly what the known garbling pattern would
 * also produce from a correct '1' alone, so BETA_24/25's result here
 * ("12345678" on the real display) could not actually distinguish
 * "sent correctly" from "garbled as usual." No two consecutive bytes
 * below are adjacent ASCII values in either direction. */
void panel_test_nine_byte_burst(void)
{
    panel_send_byte(PANEL_CMD_DISPLAY_TEXT);
    panel_send_byte('Z');
    panel_send_byte('M');
    panel_send_byte('F');
    panel_send_byte('T');
    panel_send_byte('B');
    panel_send_byte('Q');
    panel_send_byte('H');
    panel_send_byte('W');
    panel_send_byte('D');

    dbg_putc('\r'); dbg_putc('\n'); dbg_putc('A'); dbg_putc('U'); dbg_putc('X');
    dbg_putc(' '); dbg_putc('Z'); dbg_putc('M'); dbg_putc('F'); dbg_putc('T');
    dbg_putc('B'); dbg_putc('Q'); dbg_putc('H'); dbg_putc('W'); dbg_putc('D');
    dbg_putc(' '); dbg_putc('-'); dbg_putc(' ');
}
