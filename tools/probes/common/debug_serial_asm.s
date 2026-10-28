; SPDX-License-Identifier: GPL-3.0-only
; Copyright (c) 2026 Dave, KY4WI
; NOTE (2026-10-16): the "later characters corrupted" results below,
; which led to 2400 and 8N2, were the vbcc string bug (fixed in
; KY4WI/vbcc 2945d2e), not timing. 2400 8N2 is bench-proven; 9600 is
; worth trying again.
;
; debug_serial.s -- polled bit-bang serial debug channel (see
; debug_serial.h). Directly ported from stage1.s's proven-on-real-
; hardware send_byte/wait_tick (P15/sfr[0xFF01] bit 5, Timer1 reload
; 0x0067, stage1's own confirmed 9600-baud tick) -- same pins, same
; reload value, same polling idiom, just renamed for this module and
; reusing the project's standard "[sp+2] = first byte argument" ABI
; (see resident_api.s's _panel_send_byte for the same convention).
;
; BETA_10: EFFECTIVE BIT RATE IS 2400, NOT 9600. Real-hardware testing
; of every build through BETA_09 showed markers past the first
; character or two consistently corrupted (a different glitch each
; time, never a clean break) -- consistent with a small real clock
; mismatch that's within noise for one bit but accumulates over a
; byte. dbg_wait_tick_x4 (below) spends four of stage1's own proven
; 9600-rate ticks per bit instead of one, quartering the effective
; rate without deriving a new/unverified MD1 reload value.
;
; BETA_12: FRAMING IS NOW 8N2, NOT 8N1. Flagged at the bench
; ("the Kenwood standard is 8N2") and deliberately deferred at the
; time ("don't change anything now") while the fast reset-loop bugs
; were the priority. Those are fixed now (BETA_09/11), and real-
; hardware testing since then keeps showing the SAME shape of
; corruption this whole project -- early character(s) in a string
; fine, later ones glitched -- that a receiver expecting 2 stop bits
; but only ever getting 1 would produce (it would sample the next
; byte's start bit one bit-period too early, misframing everything
; after). Cheap to try, trivially reversible, and directly addresses
; a real, named discrepancy between this module and the far end
; rather than more blind timing tweaks. Set the bench terminal to
; 2400 8N2 (not 8N1) to read this channel from here on.
;
; BETA_26: BIT RATE WAS BRIEFLY 9600, STILL 8N2. See dbg_wait_tick_x4's
; own header below for the full reasoning -- in short, the 2400 rate's
; original justification (BETA_10) predates BETA_17's stack-overflow
; fix and was never independently confirmed as a real clock-mismatch
; issue rather than a symptom of that bug.
;
; BETA_27: REVERTED BACK TO 2400, STILL 8N2, on direct real-bench
; evidence -- BETA_26's 9600 build came back garbage on the
; bench ("serial output is garbage"), unlike every multi-byte test this
; whole project has run at 2400. So the original BETA_10 clock-mismatch
; diagnosis may have been real after all (or some other real constraint
; specific to this bit-banged channel/this bench setup); either way,
; 9600 is disproven by direct test, not just theory, so this is no
; longer an open experiment to revisit casually -- see
; dbg_wait_tick_x4's own header for the mechanics of the revert. User-
; requested directly: "play with serial later." Set the bench terminal
; back to 2400 8N2 to read this channel from here on.
;
; dbg_init() additionally stops/reconfigures Timer1 the same defensive
; way stage1.s does (it may already be running from the mask ROM's own
; 1F12 init with a different reload) -- safe because crt0.s's design
; rule 11 already re-masks TMIC1's *interrupt* for exactly this reason
; (no ISR installed), and this module never relies on the interrupt
; firing, only on reading the raw overflow flag bit (sfr[0xd0].7)
; directly, same as stage1.s.

	.global	_dbg_init
	.global	_dbg_putc
	.global	dbg_mark
	.global	_wdt_service
	.global	_dbg_long_delay

	.section .text

; ---------------------------------------------------------------------
; void dbg_init(void);
; Bring up P15 as output, idle-high, then Timer1 at the confirmed 9600
; reload value. Clobbers: a.
; ---------------------------------------------------------------------
_dbg_init:
	; preload output latch high BEFORE switching direction (no glitch)
	mov	a,sfr[0x01]
	or	a,#0x20
	mov	sfr[0x01],a
	mov	a,sfr[0x21]
	and	a,#0xdf
	mov	sfr[0x21],a

	; stop timer1 first (may already be running, different reload)
	mov	sfr[0x82],#0x00
	; MD1 = 0x0067 (confirmed real mask-ROM 9600 value, stage1.s)
	movw	sfr[0x8e].w,#0x0067
	mov	a,sfr[0xd0]
	; clear stale pending flag, leave MK/PR bits (0-6) untouched
	and	a,#0x7f
	mov	sfr[0xd0],a
	; start timer1
	mov	sfr[0x82],#0x80
	ret

; ---------------------------------------------------------------------
; dbg_wait_tick: block until one Timer1 bit-period elapses, then clear
; the flag. Byte-wide equivalent of the mask ROM's own bit-test/clear
; idiom (no bit instructions in this assembler build). Clobbers: a.
; ---------------------------------------------------------------------
dbg_wait_tick:
	mov	a,sfr[0xd0]
	and	a,#0x80
	be	dbg_wait_tick
	mov	a,sfr[0xd0]
	and	a,#0x7f
	mov	sfr[0xd0],a
	ret

; ---------------------------------------------------------------------
; dbg_wait_tick_x4: FOUR real Timer1 bit-periods (effective 2400,
; restored in panelprobe BETA_27 -- see this file's header for the
; direct bench result that brought it back).
;
; History: BETA_10 quartered this to 2400 (four dbg_wait_tick calls)
; after real-hardware testing of every build through BETA_09 showed
; markers past the first character or two consistently corrupted, read
; at the time as a small real Timer1/receiver clock mismatch that
; accumulates bit-to-bit within a byte. That diagnosis was never
; independently confirmed, though -- every one of those builds (BETA_09
; and earlier) predates BETA_17's stack-frame-overflow fix, the single
; largest source of corrupted/undefined real-hardware behavior this
; project has ever found (main() was overrunning its own stack before
; reaching its first statement). BETA_23 through BETA_25, all firmly
; post-BETA_17, have put far more serial traffic through this same
; module than BETA_09 ever did (repeated multi-byte diagnostics on
; every button press, over a full bench session) with the bench
; reporting zero glitches at the already-reduced 2400 rate -- read at
; the time as evidence the original corruption may have been the stack
; bug, not a genuine clock-rate problem.
;
; BETA_26 tried real 9600 (one dbg_wait_tick call) directly on that
; theory. BETA_27's real-bench result: garbage, immediately, on the very
; next test session -- disproving the "it was always just the stack
; bug" theory just as directly as it was proposed. Reverted back to
; this four-call body (true 2400) on that direct evidence, and parked
; here, not re-tried, until the serial-speed question is picked back
; up on its own. Set the bench terminal to 2400 8N2 to
; read this channel now.
;
; Still services the watchdog every bit period (BETA_11's fix, unaffected
; by this change -- four _wdt_service calls per bit is already far more
; frequent than the watchdog's shortest documented interval, section
; 19.3, needs). Clobbers: a (via dbg_wait_tick and _wdt_service).
; ---------------------------------------------------------------------
dbg_wait_tick_x4:
	call	dbg_wait_tick
	call	_wdt_service
	call	dbg_wait_tick
	call	_wdt_service
	call	dbg_wait_tick
	call	_wdt_service
	call	dbg_wait_tick
	call	_wdt_service
	ret

; ---------------------------------------------------------------------
; void dbg_long_delay(void);
; BETA_29: busy-wait roughly 1.3 seconds -- 4 x 200 = 800 calls to
; dbg_wait_tick_x4 (each ~1.667ms at the confirmed 2400-baud Timer1
; reload 0x0067: 4 x one 1/2400s bit-period), watchdog-serviced
; throughout (inherited from dbg_wait_tick_x4's own per-call service,
; already proven everywhere else in this file -- this adds no new
; watchdog risk). For panel.c's panel_test_icon_sweep(): pacing an
; automatic multi-value sweep so the tester can just watch the panel
; instead of pressing a button once per candidate value.
;
; Nested dbnz b/c loop (outer b=4, inner c=200) rather than a single
; 16-bit counter: this backend's DBNZ instruction only supports the B
; and C registers (shared/Opcodes.h's OT_R2, "a couple of C/B-only
; instructions") -- the same proven idiom dbg_putc_core's own bitloop
; below already uses for c, just nested once for a bigger count.
; dbg_wait_tick_x4/dbg_wait_tick/_wdt_service clobber only `a` (read by
; inspection), so b/c survive every call here untouched. Clobbers: a,
; b, c.
; ---------------------------------------------------------------------
_dbg_long_delay:
	mov	b,#4
dbg_long_delay_outer:
	mov	c,#200
dbg_long_delay_inner:
	call	dbg_wait_tick_x4
	dbnz	c,dbg_long_delay_inner
	dbnz	b,dbg_long_delay_outer
	ret

; ---------------------------------------------------------------------
; void dbg_putc(u8 c);
; Transmit one byte as an 8N1 frame (LSB first) on P15, busy-waiting
; one Timer1 tick per bit. Returns with the pin idle-high.
; Clobbers: a, x, c.
; ---------------------------------------------------------------------
_dbg_putc:
	mov	a,[sp+2]
	mov	x,a
	br	dbg_putc_core

; ---------------------------------------------------------------------
; dbg_mark: crt0.s-only entry point, used for pre-main() boot-stage
; breadcrumbs (crt0boot's 437-byte budget is too tight to afford the
; usual "write arg to [sp+0], call _dbg_putc" 3-instruction sequence at
; every marker site -- see crt0.s). Same transmit, but the character
; comes in register A directly instead of via the stack-arg ABI, so
; each call site in crt0.s only costs "mov a,#'X' / call dbg_mark"
; (2 instructions) instead of 3. Not part of debug_serial.h's C-visible
; API -- no leading underscore, not prototyped, hand-called from crt0.s
; only. Clobbers: a, x, c.
; ---------------------------------------------------------------------
dbg_mark:
	mov	x,a
dbg_putc_core:
	; --- start bit (always 0) ---
	mov	a,sfr[0x01]
	and	a,#0xdf
	mov	sfr[0x01],a
	call	dbg_wait_tick_x4
	mov	a,x

	mov	c,#8
dbg_putc_bitloop:
	rorc	a,1
	mov	x,a
	bnl	dbg_putc_bit0
	mov	a,sfr[0x01]
	or	a,#0x20
	br	dbg_putc_bitdrive
dbg_putc_bit0:
	mov	a,sfr[0x01]
	and	a,#0xdf
dbg_putc_bitdrive:
	mov	sfr[0x01],a
	call	dbg_wait_tick_x4
	mov	a,x
	dbnz	c,dbg_putc_bitloop

	; --- stop bits (BETA_12: 8N2, two full idle-high bit periods --
	; see file header comment. Line is already idle-high from here
	; on, so this is just a second dbg_wait_tick_x4 call -- also
	; four more free watchdog kicks, same as every other bit period
	; in this routine) ---
	mov	a,sfr[0x01]
	or	a,#0x20
	mov	sfr[0x01],a
	call	dbg_wait_tick_x4
	call	dbg_wait_tick_x4
	ret
