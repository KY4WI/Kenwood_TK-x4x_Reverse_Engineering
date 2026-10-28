; SPDX-License-Identifier: GPL-3.0-only
; Copyright (c) 2026 Dave, KY4WI
; =====================================================================
; TK-840 replacement-firmware: panelprobe crt0.
;
; RESOLVED (2026-10-16): dbg_puts("BOOT") showed nothing, and later the
; display showed "PQRSTUVW", for the same reason -- a vbcc bug (fixed in
; KY4WI/vbcc 2945d2e) that passed a string's first two bytes instead of
; its address. Nothing in the debug channel or the panel link was wrong.
;
; WHY THIS FILE EXISTS: after 13 betas of debugging crt0/panelprobe.c's own
; bolted-on bit-bang debug-serial channel (debug_serial_asm.s, P15 +
; Timer1, invented purely so WE could see what was happening on the
; bench) without ever resolving why panelprobe.c's first dbg_puts("BOOT")
; never shows even its first letter, this image deliberately regroups:
; it strips out EVERY line of that custom instrumentation and tests the
; ACTUAL stated goal -- display + buttons -- using ONLY the resident
; mask-ROM's own, already-silicon-proven panel link (1F2D send, 1F30/
; 1F33 interrupt-driven receive/shift). That link is not something this
; project wrote or bit-banged at all; it is exactly the same kind of
; thin register-marshaling shim as every other resident_api.s call
; (eeprom_byte_read, bu4094_commit, ...), called by every stock TK-840
; successfully. If this image gets anything onto the display, the panel
; link itself is proven fine on this physical unit, and the entire
; multi-session debug-serial mystery was contained in OUR OWN bolted-on
; code, safe to set aside. If it doesn't, the problem is upstream of (or
; inside) the panel link itself -- a different, more fundamental
; question (vector wiring, physical connector, interrupt baseline) --
; independent of everything theorized so far about watchdog starvation
; and UART framing.
;
; Boot sequence below: DI, SP=0xFE20, the mask ROM's own system/clock
; config subroutine (0x9E, BETA_20), the three confirmed resident
; startup calls (1F0F/1F0C/1F12, hardware doc 4.3), TMIC1 re-mask
; (design rule 11), BU4094 safe-state commit, the mask ROM's own
; startup wait (0x144, BETA_20), model-detect (1F15, BETA_20), ONE
; watchdog kick (design rule 13), EXIC0 unmask (design rule 12,
; panel-link INTE0, required for 1F2D to ever complete), EI, call
; _main. The CRF11 vector wiring (0x1f33, confirmed from the mask ROM's
; own real vector table and crf11_handler's body -- see selftest/
; crt0.s's header for the full citation) is carried over unchanged; it
; is what lets 1F2D's busy-wait ever actually clear.
;
; BETA_20: the previous version of this file matched selftest/phase1's
; crt0.s byte-for-byte, which in turn matched only the PANEL-SPECIFIC
; slice of the mask ROM's own boot sequence (1F0F/1F0C/1F12/outexp/EXIC0
; unmask). BETA_17/18/19 fixed the real stack overflow and then chased
; two further hypotheses (inter-byte settle delay; a PM1/interrupt-
; priority/RAM-flag sequence from sub_118d) that real-hardware testing
; disproved -- the second one, re-examined, turned out to be mistargeted
; anyway: that PM1/CA/CC/FEDE block in sub_118d runs AFTER sub_118d's own
; panel sends, not before (likely CLONE-mode flash-programming pin prep,
; unrelated to the panel link), so replicating it could never have
; helped. Re-reading reset_handler's *entire* boot sequence end-to-end,
; not just its panel-adjacent lines, found two more mask-ROM
; subroutines -- 0x9E (a system/clock config routine touching ~20 SFRs,
; including timer control registers this project's own debug-serial
; code also uses) and 0x144 (a fixed startup wait timed off the timer
; 0x9E configures) -- that BOTH reset_handler and sub_118d call before
; ever sending a byte to the panel, and that this project has never
; called anywhere. See panel.c/panelprobe.c for the removal of the disproven
; sub_118d-based fix, and this file's new calls below for the real
; candidate.
; =====================================================================

	.global	_main
	.global	_wdt_service
	.global	_dbg_init
	.global	dbg_mark
	.global	_model_detect

	.include	"tkx4x_maskrom_symbols.inc"	; mask ROM names (trampolines, resident RAM)

	; BETA_20: NOT resident-API trampolines (those live at 0x1Fxx and are
	; a project naming convention, not a hardware distinction) -- these
	; are two more mask-ROM subroutines at their OWN fixed low addresses,
	; called exactly the same way (plain CALL to a fixed internal-ROM
	; address, always resolvable regardless of what's on the external
	; bus -- same mechanism already proven for every trampoline_* call
	; below). Found by re-reading reset_handler's (0x0040) own boot
	; sequence end-to-end in the private mask-ROM analysis listing, rather
	; than only the panel-specific slice of it: BOTH reset_handler and
	; sub_118d (the CLONE-mode re-entry routine, also traced this
	; project) call these same two subroutines, in this same relative
	; order, before EVER touching the panel link -- and this file has
	; never called either one.
	.equ	sub_sysclk_init, 0x009e
	.equ	sub_startup_wait, 0x0144

; =====================================================================
; 0x8000-0x803F: external vector table -- identical to the earlier selftest startup code.
; =====================================================================
	.section	vectbl,"a"
	; 0x00 RESET
	.word	0x8200
	; 0x02 NMI
	.word	0x8200
	; 0x04 EXIF0 -- panel-link INTE0, resident 1F30
	.word	trampoline_panel_inte0_isr
	; 0x06 EXIF1
	.word	0x8200
	; 0x08 EXIF2
	.word	0x8200
	; 0x0A WDT
	.word	0x8200
	; 0x0C TBF
	.word	0x8200
	; 0x0E TMF0
	.word	0x8200
	; 0x10 TMF1 (TMIC1 re-masked below; never fires)
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
	; 0x20 CRF11 -- CONFIRMED: resident 1F33 (crf11_handler)
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
; 0x8040-0x807F: external CALLT table -- identical to the earlier selftest startup code.
; =====================================================================
	.section	callttbl,"a"
	; 0x40
	.word	0x8210
	; 0x42
	.word	0x8210
	; 0x44
	.word	0x8210
	; 0x46
	.word	0x8210
	; 0x48
	.word	0x8210
	; 0x4A
	.word	0x8210
	; 0x4C (EEPROM coop callback, preserved)
	.word	0x8235
	; 0x4E
	.word	0x8210
	; 0x50
	.word	0x8210
	; 0x52
	.word	0x8210
	; 0x54
	.word	0x8210
	; 0x56
	.word	0x8210
	; 0x58
	.word	0x8210
	; 0x5A (orig 0x1F1B -- resident, do not repoint)
	.word	0x8210
	; 0x5C (orig 0x1F1E -- resident, do not repoint)
	.word	0x8210
	; 0x5E
	.word	0x8210
	; 0x60
	.word	0x8210
	; 0x62
	.word	0x8210
	; 0x64
	.word	0x8210
	; 0x66
	.word	0x8210
	; 0x68
	.word	0x8210
	; 0x6A
	.word	0x8210
	; 0x6C
	.word	0x8210
	; 0x6E
	.word	0x8210
	; 0x70
	.word	0x8210
	; 0x72
	.word	0x8210
	; 0x74
	.word	0x8210
	; 0x76
	.word	0x8210
	; 0x78
	.word	0x8210
	; 0x7A
	.word	0x8210
	; 0x7C
	.word	0x8210
	; 0x7E
	.word	0x8210

; =====================================================================
; 0x8080: crt0 entry. No debug-serial anywhere in this file -- see
; header. Just the confirmed boot sequence, then straight to _main.
; =====================================================================
	.section	crt0boot,"a"
crt0:
	di
	movw	sfr[0xfc].w,#0xfe20

	; BETA_20: 0x9E -- confirmed called FIRST by BOTH of the mask ROM's
	; own panel-communication entry points (reset_handler at 0x0040 and
	; sub_118d at 0x118d), immediately after SP init and before even
	; 1F0F. Its own body (disassembled, private mask-ROM analysis listing) writes ~20 SFRs this project has never otherwise touched:
	; sfr[0x52]/[0x53] (register-bank select setup), sfr[0x40], sfr
	; [0xff], pswl.5, sfr[0x41], sfr[0x72]/[0x7a]/[0x74]/[0x70], sfr
	; [0xff0e].w/[0xff14].w/[0xff16].w, sfr[0x64]/[0x60]/[0x66]/[0x38],
	; sfr[0x8e].w (a 16-bit timer reload value, 0x0bb8), and sfr[0x80]/
	; [0x82] (timer control -- the same two SFRs this file's own
	; debug_serial_asm.s bit-bangs for an unrelated purpose, strong
	; circumstantial evidence this is genuine timer/clock bring-up, not
	; cosmetic). No previous image in this project has called this.
	call	sub_sysclk_init

	; 1F0F -- confirmed called 1st (of the trampolines)
	call	trampoline_gpio_init
	; 1F0C -- confirmed called 2nd
	call	trampoline_clear_resident_ram
	; 1F12 -- confirmed called 3rd (leaves TMIC1 unmasked)
	call	trampoline_irq_periph_init

	; design rule 11: re-mask TMIC1 back to hardware reset default --
	; this file installs no ISR at vector 0010H. (Deliberate deviation
	; from stock, which leaves TMIC1 unmasked through to EI -- already
	; justified and unchanged by BETA_20. sub_0144 below polls TMIC1's
	; own hardware request flag directly, not through the vectored
	; interrupt, so it still works correctly with MK set here.)
	mov	sfr[0xd0],#0x47

	; known-safe BU4094 shadow + commit (hardware doc reset value)
	mov	outexp_shadow,#0x25
	call	trampoline_outexp_commit

	; BETA_20: 0x144 -- confirmed called by BOTH reset_handler and
	; sub_118d, right after outexp_commit and before model-detect/EXIC0/
	; EI. Busy-waits up to 65 times for TMIC1's own hardware request
	; flag (sfr[0xd0].7) to pulse, servicing WDM every iteration --
	; effectively a fixed startup delay timed off the same Timer1
	; sub_sysclk_init above just configured and started. Never called by
	; any previous image in this project either.
	call	sub_startup_wait

	; BETA_20: resident 1F15 (model_detect, resident_api.s) -- confirmed
	; called by both reset_handler and sub_118d at this exact point,
	; right before the EXIC0 unmask below. Already had a C-callable
	; wrapper (_model_detect) that nothing in this project had ever
	; actually called.
	call	_model_detect

	; design rule 13: kick the watchdog before EI/calling _main.
	call	_wdt_service

	; BETA_16: ONE breadcrumb, added back after the handshake fix
	; (BETA_15) came back from real hardware with a blank screen and
	; NO other symptom to go on -- panel_handshake() and
	; panel_display_text() are both bounded (2000-iteration waits,
	; watchdog-serviced), so they always return SOMETHING; with zero
	; debug output anywhere in this image, "blank screen" could mean
	; "never got here" just as easily as "ran fine, panel truly isn't
	; answering". Placed AFTER 1F0F (so its PM1/P15 clobber, the
	; original BETA_07 finding, is already behind us -- no re-init
	; dance needed here, unlike selftest/phase1's crt0) and after every
	; other boot step, right before handing off to _main. This reuses
	; the SAME debug_serial_asm.s already proven safe as of BETA_11/12
	; (per-tick watchdog service, 2400 8N2) -- not the fragile
	; multi-marker crt0 instrumentation that caused 13 betas of
	; confusion in selftest/phase1, just one character confirming crt0
	; itself completed on THIS specific, much simpler boot path.
	call	_dbg_init
	mov	a,#'0'
	call	dbg_mark
	call	_wdt_service

	; design rule 12: unmask EXIC0 -- required for 1F2D (panel send)
	; to ever complete; the vector table above wires EXIF0 to 1F30.
	mov	sfr[0xc8],#0x01

	ei

	call	_main

crt0_halt:
	; _main() never returns; defensive fallback only.
	br	crt0_halt

	.org	0x180,0xff
int_trap:
	reti

	.org	0x190,0xff
callt_stub:
	ret

; =====================================================================
; 0x8235: required real code at this fixed address, same as selftest/
; phase1 (hardware doc 2.4/4.2/17.4).
; =====================================================================
	.section	eepromcb,"a"
app_eeprom_bg_callback:
	ret

; =====================================================================
; 0xE580: required "KENWOOD" application signature.
; =====================================================================
	.section	kwsig,"a"
app_signature:
	.byte	'K','E','N','W','O','O','D'

; =====================================================================
; 0xE5FF: final padding byte -> exactly 26112 (0x6600) bytes.
; =====================================================================
	.section	padbyte,"a"
final_pad_byte:
	.byte	0xff
