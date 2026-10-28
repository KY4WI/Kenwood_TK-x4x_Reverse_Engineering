; SPDX-License-Identifier: GPL-3.0-only
; Copyright (c) 2026 Dave, KY4WI
; =====================================================================
; TK-840 replacement-firmware Phase 1: crt0.
;
; Same confirmed mask-ROM handoff contract as stage1.s (see that file's
; header for the full evidence trail on $8080/8083/8086/8089, the
; vector/CALLT reconstruction requirement, and the E580 signature gate)
; -- this is the Phase 1 equivalent, extended with the interrupt-baseline
; handling stage1.s didn't need (it never called EI) and split across
; several linker-placed sections instead of one flat .org'd file, because
; the real C-compiled application code (~2.6KB, measured) does not fit in
; the 437-byte gap between the fixed entry point ($8080) and the fixed
; EEPROM-callback stub ($8235) the way stage1.s's hand-tuned asm did.
; See phase1.ld for the placement this file's sections rely on.
;
; Layout (see phase1.ld for the MEMORY regions backing each of these):
;   0x8000-0x803F  vectbl   -- external vector table (32 words)
;   0x8040-0x807F  callttbl -- external CALLT table (32 words), reconstructed
;                    per hardware doc section 17.1, same as stage1.s
;   0x8080-0x8234  crt0boot -- this file's real boot code, plus int_trap/
;                    callt_stub fixed-address targets (budget: 437 bytes)
;   0x8235         eepromcb -- app_eeprom_bg_callback, required fixed
;                    address per hardware doc 2.4/4.2/17.4
;   0x8240-0xE57F  (all C modules' .text/.data land here via phase1.ld's
;                    generic *(.text)/*(.data) rule -- not part of this
;                    file)
;   0xE580         kwsig    -- required "KENWOOD" signature (doc J.13)
;   0xE5FF         padbyte  -- forces the final image to exactly 26112
;                    (0x6600) bytes, the full 0x8000-0xE5FF app flash region
;
; Boot sequence (crt0boot, below): DI, SP=0xFE20, then the three confirmed
; resident startup calls in the confirmed order (1F0F/1F0C/1F12, hardware
; doc 4.3) -- then explicit interrupt-baseline cleanup 1F12 itself does not
; do, per design rules 11/12 (section 23):
;   - re-mask TMIC1 (Timer1, FFD0H): 1F12 deliberately leaves this one
;     source unmasked at highest priority (doc 19.2). This project installs
;     no ISR at vector 0010H, so it is re-masked back to the hardware reset
;     default (0x47) rather than left live into an unhandled EI.
;   - unmask EXIC0 (FFC8H): 1F12's baseline masks the panel-link INTE0
;     source along with everything else; this file deliberately unmasks it
;     (clearing only the MK bit, keeping 1F12's own priority) because the
;     vector table below wires EXIF0 to a real target (resident 1F30).
; Then: known-safe BU4094 shadow + commit (hardware doc reset value,
; 0x25), an initial watchdog kick (design rule 13), EI, and a call into
; the real C-compiled application (_main, phase1.c).
;
; FORMERLY A KNOWN OPEN GAP, NOW RESOLVED: which vector feeds the other
; panel ISR, resident 1F33 ("timed panel-link ISR/state machine"), was
; unconfirmed for most of this project -- TMF0 was the leading guess
; ("both are timed") but deliberately left unwired rather than guessed.
; Resolved directly from the mask ROM disassembly: 1F33's
; trampoline is "br crf11_handler" (0x1726), and the mask ROM's own
; real vector table has its CRF11 slot (address 0x0020) populated with
; that exact same address -- i.e. 1F33 *is* crf11_handler, reached via
; CRF11 (Up/down counter 1, CRIC11 @ sfr[0xc6]), not TMF0. See the
; vector table below for the real citation and what crf11_handler's own
; body does (clears the panel TX busy flag 1F2D waits on). Wiring it
; below is what lets 1F2D (panel_send_byte)/panel_display_text() in
; phase1.c ever actually complete instead of hitting panel.c's bounded-
; wait give-up every time.
; =====================================================================

	.global	_main
	.global	_wdt_service

	.include	"tkx4x_maskrom_symbols.inc"	; mask ROM names (trampolines, resident RAM)
	.include	"panel_codes.inc"		; PANEL_BEEP, BEEP_...
	.equ	wait_10ms, 0x1138			; mask ROM: 10 ms on timer 1, no watchdog kick

; =====================================================================
; 0x8000-0x803F: external vector table. Every slot points at int_trap
; (0x8200, a bare RETI, placed below within crt0boot) except EXIF0, which
; is confirmed (hardware doc 19.1) to be the panel-link's own INTE0
; source, wired directly to resident 1F30 -- 1F30 is itself documented as
; a complete "RETI-only interrupt handler" (doc section 18), i.e. it is
; meant to be entered directly from a vector, not called as a subroutine,
; so no wrapper is needed here, just the direct address.
; =====================================================================
	.section	vectbl,"a"
	; 0x00 RESET (not really used this way; harmless)
	.word	0x8200
	; 0x02 NMI (non-maskable -- the one real one here, DI doesn't block it)
	.word	0x8200
	; 0x04 EXIF0 -- CONFIRMED: panel-link INTE0, wired to resident 1F30
	.word	trampoline_panel_inte0_isr
	; 0x06 EXIF1
	.word	0x8200
	; 0x08 EXIF2
	.word	0x8200
	; 0x0A WDT (nonmaskable, no IC register -- real harmless target anyway)
	.word	0x8200
	; 0x0C TBF
	.word	0x8200
	; 0x0E TMF0
	.word	0x8200
	; 0x10 TMF1 -- Timer1; TMIC1 is explicitly re-masked below (design
	; rule 11), so this should never fire, but a real target is kept here
	; anyway as belt-and-suspenders, same reasoning as stage1.s's NMI slot.
	.word	0x8200
	; 0x12 TMF2
	.word	0x8200
	; 0x14
	.word	0x8200
	; 0x16
	.word	0x8200
	; 0x18
	.word	0x8200
	; 0x1A CRF00
	.word	0x8200
	; 0x1C CRF01
	.word	0x8200
	; 0x1E CRF10
	.word	0x8200
	; 0x20 CRF11 -- CONFIRMED (mask ROM disassembly): the real silicon
	; vector at mask-ROM address 0x0020 is itself populated with
	; "crf11_handler" (0x1726), and resident 1F33's trampoline is
	; "br crf11_handler" -- i.e. 1F33 *is* crf11_handler, reached via
	; CRF11 (Up/down counter 1, CRIC11 @ sfr[0xc6]), not TMF0 as this
	; file used to guess. crf11_handler's own body ends by clearing
	; 0xfeda.1 (the panel TX busy flag 1F2D waits on) and 0xfed9.7 --
	; this is the real bit-shifting state machine 1F2D's send and the
	; receive queue both depend on. CRIC11 itself is left masked by
	; 1F12's baseline (sfr[0xc6]=0x41) and is dynamically un/re-masked
	; by exif0_handler (1F30, at the start of each transfer) and by
	; crf11_handler itself (at the end) -- not something this file
	; needs to touch at boot, only wire the vector for.
	.word	trampoline_panel_timed_isr
	; 0x22 SEF
	.word	0x8200
	; 0x24 SRF
	.word	0x8200
	; 0x26 STF
	.word	0x8200
	; 0x28 ADF
	.word	0x8200
	; 0x2A
	.word	0x8200
	; 0x2C
	.word	0x8200
	; 0x2E
	.word	0x8200
	; 0x30
	.word	0x8200
	; 0x32
	.word	0x8200
	; 0x34
	.word	0x8200
	; 0x36
	.word	0x8200
	; 0x38
	.word	0x8200
	; 0x3A
	.word	0x8200
	; 0x3C
	.word	0x8200
	; 0x3E BRK
	.word	0x8200

; =====================================================================
; 0x8040-0x807F: external CALLT table, reconstructed per hardware doc
; section 17.1, identical mapping to stage1.s. No C code in Phase 1 uses
; the __callt_slotN attribute (dac_write's shim calls 1F24 directly, a
; deliberate choice -- see resident_api.h), so nothing here is populated
; by the C build; this table exists purely so nothing that assumes the
; original ABI lands on undefined flash, per design rule 2/doc 2.3's
; "preserve/reconstruct this area intentionally" guidance. Slot 6
; (0x804C) preserves the real original value (0x8235, our own fixed
; eepromcb address) rather than pointing at the generic stub.
; =====================================================================
	.section	callttbl,"a"
	; 0x40  (orig 0x8F22)
	.word	0x8210
	; 0x42  (orig 0xA43F)
	.word	0x8210
	; 0x44  (orig 0xD85A)
	.word	0x8210
	; 0x46  (orig 0xD919)
	.word	0x8210
	; 0x48  (orig 0xA506)
	.word	0x8210
	; 0x4A  (orig 0xADA3)
	.word	0x8210
	; 0x4C (orig 0x8235 -- EEPROM coop callback, preserved)
	.word	0x8235
	; 0x4E  (orig 0x8311)
	.word	0x8210
	; 0x50  (orig 0x8238)
	.word	0x8210
	; 0x52  (orig 0xC33A)
	.word	0x8210
	; 0x54  (orig 0xC329)
	.word	0x8210
	; 0x56  (orig 0xC41E)
	.word	0x8210
	; 0x58  (orig 0xC6D5)
	.word	0x8210
	; 0x5A  (orig 0x1F1B -- resident, do not repoint)
	.word	0x8210
	; 0x5C  (orig 0x1F1E -- resident, do not repoint)
	.word	0x8210
	; 0x5E  (orig 0xC729)
	.word	0x8210
	; 0x60  (orig 0xE082)
	.word	0x8210
	; 0x62  (orig 0xDAB5)
	.word	0x8210
	; 0x64  (orig 0x808C)
	.word	0x8210
	; 0x66  (orig 0x808F)
	.word	0x8210
	; 0x68  (orig 0xA886)
	.word	0x8210
	; 0x6A  (orig 0xC784)
	.word	0x8210
	; 0x6C  (orig 0x8EDD)
	.word	0x8210
	; 0x6E  (orig 0x8178)
	.word	0x8210
	; 0x70  (orig 0xA88D)
	.word	0x8210
	; 0x72  (orig 0x818A)
	.word	0x8210
	; 0x74  (orig 0xCF13)
	.word	0x8210
	; 0x76  (orig 0x897A)
	.word	0x8210
	; 0x78  (orig 0xE101)
	.word	0x8210
	; 0x7A  (orig 0xD851)
	.word	0x8210
	; 0x7C  (orig 0xFFFF -- already unused)
	.word	0x8210
	; 0x7E  (orig 0xFFFF -- already unused)
	.word	0x8210

; =====================================================================
; 0x8080: crt0 entry (the confirmed mask-ROM default-case handoff addr),
; plus the int_trap/callt_stub fixed targets the tables above reference
; by literal address (vasm's .word directive only accepts constant
; numeric expressions, not forward symbol references -- same limitation
; stage1.s already documented and worked around the same way).
; =====================================================================
	.section	crt0boot,"a"
crt0:
	di
				; belt-and-suspenders, same as stage1.s: block
				; every maskable source before our own vector
				; table's contents are guaranteed correct.
	movw	sfr[0xfc].w,#0xfe20
				; SP: reuse the mask ROM's own confirmed-good
				; stack area/value (real evidence, stage1.s).

	; --- BETA_06 bring-up instrumentation, BETA_09 watchdog fix -----
	; BETA_05 added a debug-serial channel but only phase1.c used it
	; (dbg_init()+dbg_puts("BOOT") as main()'s first lines) -- on real
	; hardware that build sent nothing but a recurring ~500ms <break>,
	; never any text, which means (a UART break is "no framed data
	; sent", not "misdecoded data") that this image never reaches
	; main() at all. The only way to localize that is markers *before*
	; main() -- so dbg_init() moves here, to the very first thing
	; after SP is set, and one character goes out after each of the
	; three resident boot calls below. Uses dbg_mark (debug_serial_asm.s),
	; not dbg_putc: crt0boot's budget is exactly 437 bytes with very
	; little headroom, and dbg_mark's register-arg ABI (char in A, no
	; stack write) is the only way four markers fit.
	;   '0' = dbg_init() returned, UART pin/timer configured
	;   '1' = 1F0F (gpio_init) returned
	;   '2' = 1F0C (clear_resident_ram) returned
	;   '3' = 1F12 (irq_periph_init) returned
	; Seeing fewer than all four == dying inside that call or the one
	; right after it; seeing all four but no "BOOT" from phase1.c ==
	; dying somewhere in this file's own remaining cleanup, between
	; here and the call to _main.
	;
	; BETA_09: real hardware showed marker '3' always followed by '0'
	; again -- a genuine reset loop, not a hang. Root cause: the
	; watchdog doc (19.3) is explicit that an unserviced WDM resets the
	; CPU via the nonmaskable vector 5.5-349.3ms after the last kick,
	; and this file previously had exactly ONE _wdt_service() call,
	; placed after 1F27 -- fine when crt0 was just register writes and
	; fast resident calls, but each dbg_mark transmission here is a
	; real ~1ms busy-wait (9600 baud, 10 bits/char), and dbg_wait_tick's
	; own poll loop never kicks the watchdog either. Four markers plus
	; the resident calls' own time is enough real elapsed time to hit
	; even the watchdog's SHORTEST interval (5.5ms) before ever
	; reaching that one late kick -- so a watchdog reset was landing
	; somewhere around/after marker '3', before marker '4' ever got a
	; chance to go out. Fixed by kicking it after every marker below,
	; not just once at the end.
	call	_wdt_service

	; 1F0F -- confirmed called 1st
	call	trampoline_gpio_init
	; CONFIRMED (mask ROM disassembly, sub_00ed at 0x00ed, the real
	; target of this trampoline): 1F0F's bulk GPIO baseline includes
	; "mov sfr[0x21],#0x7b" -- sfr[0x21] is PM1, and bit 5 of 0x7b is 1
	; (input), which is P15, our debug-serial TX pin. That silently
	; undoes dbg_init()'s "mov sfr[0x21],a / and a,#0xdf" (output) from
	; just above -- exactly the P15/PM1 conflict stage1.s's own header
	; comment flagged as unconfirmed-but-plausible (it dodged the whole
	; question by setting up P15 *after* calling 1F0F; this file's
	; marker '0' wants dbg_init() called *before*, to prove crt0 itself
	; is alive, so instead: re-run dbg_init() right here to put P15
	; back into output mode before any marker past '0' tries to use
	; it. Neither 1F0C nor 1F12 (below) touch sfr[0x21]/PM1, so this is
	; the only re-init needed.
	call	_wdt_service
	; 1F0C -- confirmed called 2nd
	call	trampoline_clear_resident_ram
	call	_wdt_service
	; 1F12 -- confirmed called 3rd (leaves TMIC1 unmasked, see below)
	call	trampoline_irq_periph_init

	; design rule 11: re-mask TMIC1 (Timer1) explicitly -- 1F12's own
	; baseline (doc 19.2) leaves exactly this one source unmasked, at
	; highest priority, by design. This file installs no ISR at vector
	; 0010H, so restore the hardware reset default (0x47: masked,
	; priority 7) before EI rather than leave it live into an
	; unhandled interrupt. Done HERE, before marker '3', not after:
	; CONFIRMED (mask ROM disassembly, sub_0116 at 0x0116) that 1F12
	; itself writes sfr[0xd0] (TMIC1) = 0x80, which sets the same IF
	; bit dbg_wait_tick polls for "one Timer1 bit-period elapsed" --
	; left in place across marker '3''s transmission, that stale set
	; flag would make its very first poll return immediately instead
	; of waiting a real bit-period, garbling '3''s bit timing.
	mov	sfr[0xd0],#0x47
	call	_wdt_service

	; BU4094 output-expander shadow byte: reuse the exact known-safe
	; value the mask ROM's own boot sequence writes here (hardware.h's
	; BU4094_RESET_VALUE / 0x25), same as stage1.s.
	mov	outexp_shadow,#0x25
	; 1F27 -- commit it
	call	trampoline_outexp_commit

	; design rule 13: service the watchdog before doing anything else
	; that could conceivably take a while (unmasking interrupts,
	; calling into the real application).
	call	_wdt_service

	; design rule 12: unmask EXIC0 (FFC8H) deliberately -- 1F12's
	; baseline masks it (0x41) along with every other source; clear
	; only the MK bit (bit6), keep 1F12's own priority (1) -> 0x01.
	; Safe to do now because the vector table above already wires
	; EXIF0 to a real target (resident 1F30).
	mov	sfr[0xc8],#0x01

	; Move the stack. The start-up calls above ran with SP = FE20h (as the
	; mask ROM and stock firmware do); 1F0Ch clears FE20h-FEEFh, so the
	; stack could not live there until it had run. FE60h-FEBBh is free
	; while our code runs (the mask ROM uses it only in its loader and KPG
	; monitor, which reset SP themselves): 92 bytes, against 30 at FE20h,
	; which the C code overflowed. Fill it with 55h first, so the C code
	; can measure how much was used (dbg_report_stack()).
	movw	hl,#0xfe60
	mov	c,#0x5c				; 92 bytes, FE60h-FEBBh
	mov	a,#0x55
fill_stack:
	mov	[hl+],a
	dbnz	c,fill_stack
	movw	sfr[0xfc].w,#0xfebc		; SP = FEBCh; grows down to FE60h
	ei
	; Boot beep, so a reset is audible: the mask ROM's start has no beep of
	; its own, and the stock firmware's power-on beep is BEEP_POWER_ON, so
	; ours is a different one, BEEP_LOW_HIGH_LOW. Unmute first (expander
	; AM and AC off, as every stock beep routine does), let it play for
	; about a second, then put the expander back to its reset value.
	clr1	outexp_shadow.0			; AM off
	clr1	outexp_shadow.1			; AC off
	call	trampoline_outexp_commit
	mov	a,#0x05				; DAC 5 = volume; it is 0 after reset,
	mov	x,#0x5B				; so set what benchtest uses (entry 20 of
	call	trampoline_dac_write		; the mask ROM's volume table, 0D14h)
	mov	a,#PANEL_BEEP
	call	trampoline_panel_tx_queue
	mov	a,#BEEP_LOW_HIGH_LOW
	call	trampoline_panel_tx_queue
	mov	b,#100				; 100 x 10 ms
boot_beep_wait:
	call	wait_10ms
	call	_wdt_service
	dbnz	b,boot_beep_wait
	mov	outexp_shadow,#0x25		; expander reset value (AM set: muted)
	call	trampoline_outexp_commit
				; From here: Timer1 is masked (re-masked
				; above), EXIC0/panel-link is live and wired,
				; the watchdog has been kicked once, and
				; _main() (phase1.c) will keep kicking it every
				; loop iteration.

	; '4' = everything above this file's own cleanup completed; about
	; to call _main. If '4' appears but phase1.c's own "BOOT" doesn't,
	; the problem is in the call/entry to _main itself, not crt0boot.

	call	_main

crt0_halt:
	; _main() is an infinite loop and never returns -- this is a
	; defensive fallback only, so control never runs off crt0boot's own
	; end into whatever real code (or fill byte) happens to follow it.
	br	crt0_halt

; ---------------------------------------------------------------------
; Shared trap targets. Fixed literal addresses (0x8200/0x8210, same as
; stage1.s) for the reason given above: .word can't reference labels.
; Both comfortably fit inside crt0boot's 437-byte budget (0x8080-0x8234)
; -- real code above ends well before offset 0x180; the .org fill (0xff)
; below pads the gap, same convention as stage1.s.
; ---------------------------------------------------------------------
	.org	0x180,0xff
int_trap:
	reti

	.org	0x190,0xff
callt_stub:
	ret

; =====================================================================
; 0x8235: required real code at this fixed address per hardware doc
; section 2.4/4.2/17.4 (resident cooperative EEPROM entries 1F42/1F45
; call this address directly, by hardcoded absolute address, while
; waiting on EEPROM activity). Phase 1's eeprom.c uses only the blocking
; EEPROM routines (1F1B/1F18/1F1E/1F21 via resident_api.h), which the
; doc confirms do NOT require this callback -- but CALLT slot 6 above
; still preserves the stock 0x8235 value, and design rule 8 ("preserve
; the resident recovery path... contract") argues for providing the
; real ABI-minimum stub regardless of whether this build currently
; exercises the cooperative path, same reasoning as stage1.s.
; =====================================================================
	.section	eepromcb,"a"
app_eeprom_bg_callback:
	ret

; =====================================================================
; 0xE580: REQUIRED "KENWOOD" application signature -- see stage1.s's
; header for the full real-evidence trail (mask ROM's own reset handler
; compares 7 bytes here against its stored string before ever reaching
; the 0x8080/83/86/89 dispatch; a mismatch diverts to an internal
; panel-error/recovery routine and this image is never reached at all).
; =====================================================================
	.section	kwsig,"a"
app_signature:
	.byte	'K','E','N','W','O','O','D'

; =====================================================================
; 0xE5FF: final padding byte, forcing the linked image out to the full,
; required output size of exactly 26112 (0x6600) bytes -- same
; requirement and same "emit one real byte, don't rely on .org alone"
; reasoning as stage1.s.
; =====================================================================
	.section	padbyte,"a"
final_pad_byte:
	.byte	0xff
