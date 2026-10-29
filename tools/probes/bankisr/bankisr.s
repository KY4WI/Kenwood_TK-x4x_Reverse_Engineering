; SPDX-License-Identifier: GPL-3.0-only
; Copyright (c) 2026 Dave, KY4WI
; =====================================================================
; TK-840 replacement-firmware: bankisr probe.
;
; WHY THIS FILE EXISTS: tests whether a periodic interrupt handler can
; use SEL RBn (switch to a dedicated, otherwise-unused register bank)
; instead of PUSH/POP to preserve the interrupted code's AX across an
; interrupt, and still return cleanly without corrupting mainline
; state. This is the exact mechanism the real low-speed-signaling DDS
; tick handler (sub_8B36, hardware doc 13.3.2) would benefit from: it
; currently pays for PUSH {ax}/POP {ax} around nine separate exit
; points on every tick.
;
; Timer source: the SAME real hardware mechanism sub_B05E/loc_B0CC uses
; in stock firmware (hardware doc 13.3.2, traced and corrected this
; session) -- TMC0=0x80 (interval timer mode, free-running, no
; external trigger), TM0 clocked at fCLK/6 = 1 MHz at this radio's
; 6 MHz CPU clock, MD0=0x005C (92 decimal) -> a 92 us/10.87 kHz TMF0
; tick, vector 0x800E. Confirmed against the real vector table (not the
; ADF slot, 0x8028, which is a different, unrelated routine -- see the
; hardware doc for the full correction). Nothing here runs the mask
; ROM's own tone-generation code; this ISR does comparable (not
; identical) work of its own -- a register-pair increment and a PWM0
; write -- so it is a representative stand-in, not a frivolous empty
; handler.
;
; THE TEST: mainline runs a tight loop that continuously RE-WRITES a
; known 16-bit pattern into AX and checks it back immediately (not just
; holding it steady -- a corruption that only shows up mid-write needs
; to be caught mid-write), 65536 times between each status line. Bank 7
; (RB7, RAM FE80h-FE8Fh) is dedicated to the ISR -- picked because it
; is architecturally unused by stock firmware (confirmed: zero SEL
; instructions anywhere in the 26 KB mask ROM) and does not overlap
; this probe's own working set. No PUSH/POP of AX anywhere in the ISR;
; entry is "SEL RB7", exit is "SEL RB0", full stop.
;
; A live heartbeat (ISR fire count) and the canary failure count are
; printed on the bit-banged debug channel (P15, mic jack, 2400 8N2;
; common/debug_serial_asm.s, bench-proven) every ~65536-iteration pass,
; as plain decimal-free hex with real values, not a cryptic code, per
; this project's test-image convention. FAILS should read 00 forever;
; TICKS climbing confirms the ISR is actually firing throughout.
;
; Boot sequence is panelprobe/crt0.s's own proven sequence, exactly as
; uartprobe/crt0.s already reuses it (DI, SP=0xFE20, mask ROM's
; sub_sysclk_init, the three confirmed resident startup calls
; 1F0F/1F0C/1F12, TMIC1 re-mask, BU4094 safe-state commit, mask ROM's
; own startup wait (0x144), model-detect (1F15), one watchdog kick) --
; with the PANEL-SPECIFIC pieces removed, same as uartprobe. This image
; additionally arms Timer 0/TMIC0 (above) before EI, and never calls
; _main -- mainline is the canary loop right here instead of a separate
; C file, since the whole point under test is register-level behavior
; across a real interrupt, which is clearer to audit as one small,
; fully hand-written file than split across a C/asm boundary.
; =====================================================================

	.global	_wdt_service
	.global	_dbg_init
	.global	dbg_mark
	.global	_model_detect

	.include	"tkx4x_maskrom_symbols.inc"	; mask ROM names (trampolines, resident RAM, SFRs)

	.equ	sub_sysclk_init, 0x009e
	.equ	sub_startup_wait, 0x0144

; Two bytes of our own scratch RAM, same spare region every other
; probe's .bss lands in (0xFE40-0xFE5F) -- picked as fixed .equ
; addresses rather than a linker-relocated .bss symbol, matching this
; project's own convention for hand-written-asm RAM names (every
; ram_FExx in tkx4x_maskrom_symbols.inc is a plain .equ, not a linked
; symbol): the short-direct (SADDR) addressing INC/MOV1/CLR1 etc. use
; needs a real, known-at-assemble-time address, not something vlink
; only resolves after the fact.
	.equ	isr_fires, 0xfe40
	.equ	canary_fails, 0xfe41

; =====================================================================
; 0x8000-0x803F: external vector table. Only RESET and TMF0 (0x0E) are
; meaningful here -- TMF0 is this image's entire subject; every other
; vector points at the shared do-nothing stub, same as every other
; no-extra-ISR probe in this project.
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
	; 0x0E TMF0 -- the real subject of this probe.
	.word	isr_tmf0_bankswitch
	; 0x10 TMF1 (TMIC1 re-masked below; never fires -- debug_serial_asm
	; owns Timer 1 for the bit-bang debug channel, left alone)
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

	; re-mask TMIC1 back to hardware reset default -- debug_serial_asm
	; owns Timer 1 for the bit-bang debug channel below, not an ISR.
	mov	sfr[0xd0],#0x47

	; known-safe BU4094 shadow + commit (hardware doc reset value) --
	; generic board-safety, kept here same as every other image.
	mov	outexp_shadow,#0x25
	call	trampoline_outexp_commit

	call	sub_startup_wait
	call	_model_detect

	; our own two scratch bytes aren't inside whatever range
	; trampoline_clear_resident_ram actually covers (undocumented) --
	; zero them explicitly rather than assume.
	mov	isr_fires,#0x00
	mov	canary_fails,#0x00

	; kick the watchdog before arming Timer 0 / EI.
	call	_wdt_service

	; bit-banged P15 debug channel init (2400 8N2, Timer 1) -- our
	; only reporting channel in this image.
	call	_dbg_init
	mov	a,#'0'
	call	dbg_mark
	call	_wdt_service

	; ---------------------------------------------------------------
	; Arm Timer 0 as the real DDS-ISR-rate source, the same real
	; hardware mechanism stock firmware's sub_B05E/loc_B0CC uses
	; (hardware doc 13.3.2): interval-timer mode, free-running, TM0
	; clocked at fCLK/6 = 1 MHz at this radio's 6 MHz CPU clock,
	; MD0=0x005C (92 decimal) -> 92 us/10.87 kHz. TMIC0=0x42 (mask
	; asserted, priority 2) then IF/MK cleared is stock's own
	; sequencing (sub_B05E), reused here unchanged; TMC0 is written
	; last, only once MD0/TM0/TMIC0 are already correct, so the first
	; real TM0 count starts from a known-good configuration.
	; ---------------------------------------------------------------
	movw	sfr[MD0],#0x005c
	movw	sfr[TM0],#0x0000
	mov	sfr[TMIC0],#0x42
	clr1	sfr[TMIC0].7
	clr1	sfr[TMIC0].6
	mov	sfr[TMC0],#0x80

	mov	a,#'1'
	call	dbg_mark

	ei
	br	!main_loop	; jump clear of the int_trap/callt_stub padding
				; immediately below before it's ever reached --
				; "!" (absolute, not PC-relative): main_loop is
				; in the far-away .text section, outside an
				; 8-bit relative branch's reach

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
; Mainline: the canary stress loop. No call to _main -- this whole
; image's subject is register-level behavior across a real interrupt,
; so it is one small, fully hand-written loop instead of a separate C
; file. Past the fixed crt0boot addresses above, so it gets the roomy
; apptext region (0x8240-0xE580) instead of crt0boot's tight 437-byte
; budget.
; =====================================================================
	.section	.text,"ax"
main_loop:
	call	_wdt_service

	mov	b,#0x00			; outer: 256 passes
canary_outer:
	mov	c,#0x00			; inner: 256 passes -- 65536 total
canary_inner:
	movw	ax,#0xa55a		; a distinctive, non-zero, non-all-ones
					; pattern -- easy to recognize if ever
					; misread
	nop
	nop
	nop
	nop				; widen the window AX is "live" and
					; unguarded for, so a 92 us tick is
					; essentially certain to land inside
					; this block many times per pass
	cmpw	ax,#0xa55a
	be	canary_ok
	inc	canary_fails
canary_ok:
	dbnz	c,canary_inner
	call	_wdt_service		; serviced every 256 inner iterations,
					; well inside the shortest documented
					; watchdog interval (5.5 ms)
	dbnz	b,canary_outer

	; --- status line: "TICKS=xx FAILS=yy\r\n" ---
	mov	a,#'T'
	call	dbg_mark
	mov	a,#'I'
	call	dbg_mark
	mov	a,#'C'
	call	dbg_mark
	mov	a,#'K'
	call	dbg_mark
	mov	a,#'S'
	call	dbg_mark
	mov	a,#'='
	call	dbg_mark
	mov	a,isr_fires
	call	print_hex8
	mov	a,#' '
	call	dbg_mark
	mov	a,#'F'
	call	dbg_mark
	mov	a,#'A'
	call	dbg_mark
	mov	a,#'I'
	call	dbg_mark
	mov	a,#'L'
	call	dbg_mark
	mov	a,#'S'
	call	dbg_mark
	mov	a,#'='
	call	dbg_mark
	mov	a,canary_fails
	call	print_hex8
	mov	a,#0x0d
	call	dbg_mark
	mov	a,#0x0a
	call	dbg_mark

	br	main_loop

; =====================================================================
; isr_tmf0_bankswitch: the real subject of this probe. Entered on
; every TMF0 tick (92 us/10.87 kHz, armed above). Uses SEL RB7/SEL RB0
; instead of PUSH {ax}/POP {ax} to keep mainline's AX untouched.
;
; Bank 7 is RAM FE80h-FE8Fh -- architecturally the CPU's own register
; bank 7 (general register chapter, manual sec. 3.2.2), confirmed
; unused anywhere in the 26 KB stock mask ROM (zero SEL instructions in
; the whole image). This ISR's own AX therefore lives in FE84h/FE85h
; the whole time, completely separate storage from whatever mainline's
; AX (bank 0, FEF0h-FEF1h) currently holds -- that separation, not any
; save/restore discipline, is what makes skipping PUSH/POP safe here.
;
; Clobbers (from the interrupted code's point of view): nothing. A, X
; and C are all bank-7's own copies while this runs; mainline's A, X, C
; in bank 0 are never touched.
; =====================================================================
isr_tmf0_bankswitch:
	sel	rb7

	; representative DDS-like work: a running register-pair counter
	; and a PWM0 write, enough to be a real, visible, scope-able
	; stand-in for the real ISR's phase-accumulator update -- not
	; identical to it (this probe is about the entry/exit mechanism,
	; not a reimplementation of QT/DQT/LTR). AX, not VP/UP/DE/HL, is
	; the whole point here (it's the register PUSH {ax}/POP {ax} was
	; protecting), and INCW's short single-operand form only reaches
	; VP/UP/DE/HL -- ADDW AX,#1 is the AX-reaching equivalent.
	addw	ax,#1
	movw	PWM0,ax

	inc	isr_fires		; plain RAM, bank-independent address --
					; SEL only changes what the register
					; MNEMONICS point at, never how a named
					; RAM symbol like this is addressed

	clr1	sfr[TMIC0].7		; acknowledge TMF0

	sel	rb0
	reti

; =====================================================================
; print_hex8: print A as two ASCII hex digits, upper-case (A-F), on the
; debug channel. Clobbers: a, x, c.
; =====================================================================
print_hex8:
	mov	x,a
	shr	a,4
	call	print_nibble
	mov	a,x
	and	a,#0x0f
	call	print_nibble
	ret

print_nibble:
	cmp	a,#10
	bl	print_nibble_digit
	add	a,#('A'-10)
	br	print_nibble_out
print_nibble_digit:
	add	a,#'0'
print_nibble_out:
	call	dbg_mark
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
