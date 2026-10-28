; SPDX-License-Identifier: GPL-3.0-only
; Copyright (c) 2026 Dave, KY4WI
; =====================================================================
; TK-840 replacement-firmware: baudsweep crt0.
;
; WHY THIS FILE EXISTS: follow-on to uartprobe, which bench-confirmed
; the on-chip hardware UART (P24/TXD, P25/RXD -- hardware doc section
; 12.4) works at all -- it just turned out to be running at 1200 baud,
; not the originally-guessed 9600 (see uart.h for the now-confirmed
; register formula). This image cycles through a ladder of bit rates
; so the actual best/fastest usable rate can be found on the bench by
; just changing the terminal program's speed, not by rebuilding.
;
; Boot sequence is identical to uartprobe/crt0.s, itself panelprobe's
; own proven sequence with the panel-specific pieces removed: DI, SP=
; 0xFE20, mask ROM's sub_sysclk_init (0x9E), the three confirmed resident
; startup calls 1F0F/1F0C/1F12, TMIC1 re-mask, BU4094 safe-state commit,
; mask ROM's own startup wait (0x144), model-detect (1F15), one watchdog
; kick, EI, call _main. No EXIC0 unmask, and EXIF0/CRF11 left at the
; shared 0x8200 catch-all -- this image, like uartprobe, never calls
; panel_send_byte()/1F2D and installs no ISR at all (pure polling
; throughout, same as uartprobe).
; =====================================================================

	.global	_main
	.global	_wdt_service
	.global	_dbg_init
	.global	dbg_mark
	.global	_model_detect

	.include	"tkx4x_maskrom_symbols.inc"	; mask ROM names (trampolines, resident RAM)

	.equ	sub_sysclk_init, 0x009e
	.equ	sub_startup_wait, 0x0144

; =====================================================================
; 0x8000-0x803F: external vector table. Only RESET/NMI are meaningful
; here -- every other vector points at the shared do-nothing stub,
; since this image installs no ISR at all (pure polling throughout).
; =====================================================================
	.section	vectbl,"a"
	; 0x00 RESET
	.word	0x8200
	; 0x02 NMI
	.word	0x8200
	; 0x04 EXIF0
	.word	0x8200
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
	; 0x20 CRF11
	.word	0x8200
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
; 0x8040-0x807F: external CALLT table -- identical to every other image
; in this project (no resident EEPROM/DAC calls used here either, but
; 0x4C is kept pointed at the required real callback stub regardless).
; =====================================================================
	.section	callttbl,"a"
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	; 0x4C (EEPROM coop callback, preserved)
	.word	0x8235
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210
	.word	0x8210

; =====================================================================
; 0x8080: crt0 entry.
; =====================================================================
	.section	crt0boot,"a"
crt0:
	di
	movw	sfr[0xfc].w,#0xfe20

	call	sub_sysclk_init
	call	trampoline_gpio_init
	call	trampoline_clear_resident_ram
	call	trampoline_irq_periph_init

	; re-mask TMIC1 back to hardware reset default -- no ISR installed
	; at vector 0010H in this image either.
	mov	sfr[0xd0],#0x47

	; known-safe BU4094 shadow + commit (hardware doc reset value) --
	; generic board-safety, not panel-link-specific, kept here same as
	; every other image.
	mov	outexp_shadow,#0x25
	call	trampoline_outexp_commit

	call	sub_startup_wait
	call	_model_detect

	; kick the watchdog before EI/calling _main.
	call	_wdt_service

	; bit-banged P15 debug channel init, kept as an independent
	; breadcrumb -- see baudsweep.c's header for why.
	call	_dbg_init
	mov	a,#'0'
	call	dbg_mark
	call	_wdt_service

	; no EXIC0 unmask here -- see header. Nothing in this image needs
	; any interrupt; every peripheral used (P15 bit-bang, hardware
	; UART) is driven by plain polling.

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
; 0x8235: required real code at this fixed address, same as every other
; image in this project (hardware doc 2.4/4.2/17.4).
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
