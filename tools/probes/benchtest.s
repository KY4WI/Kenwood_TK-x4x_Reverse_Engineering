; SPDX-License-Identifier: GPL-3.0-only
; Copyright (c) 2026 Dave, KY4WI
; benchtest.s - TK-x4x front-panel bench test image, all assembly.
;
; Flash image for 8000H-E5FFH (26112 bytes). No C, no CALLT, CCW TPF left
; clear, so the mask ROM's own vectors run the panel link (EXIF0, CRF11)
; exactly as in the mask ROM's own modes.
;
; Keys (press codes from the bench key table, hardware reference 11.8):
;
;   Sys UP  (80H)  text through OUR loop: 02H then 8 characters, one
;                  CALL 1F2DH per character inside a DBNZ loop. "LOOP  OK"
;   Sys DN  (82H)  text through the mask ROM's own unrolled routine,
;                  panel_text8 (15C8H). "ROM   OK"
;   Grp UP  (88H)  beep number + 1 (01H-0DH, wraps), shown as "BEEP nn"
;   Grp DN  (8AH)  beep number - 1
;   Scan    (98H)  unmute the audio, show "PLAY nn", send PANEL_BEEP (B3H)
;                  and the number
;   Aux     (9CH)  send PANEL_BEEP_STOP (B4H), mute the audio again, "STOP B4"
;   Vol UP  (90H)  character table: shows "C xx-yy" for a second, then the
;                  8 codes xx..yy as text. Each press moves on 8 codes,
;                  20H-7FH, then wraps.
;   Vol DN  (92H)  shows "PNL 84" for a second, then sends panel byte 84H
;                  alone; the next press does 87H, and so on.
;
; Every other byte from the panel (not one of the eight keys' press or
; release codes) is shown as hex, the last four received, newest on the
; right: "FFFF55A0" after 55H then A0H. FFH means empty (the panel link
; never queues FFH). Use this to see what the panel sends when the PC types
; PROGRAM at 1200 baud into the mic jack.
;
; Before each key's screen, 8 spaces are shown for 0.3 s, so a repeated
; press visibly redraws.
;
; Mask ROM routines used (addresses are the same in every TK-x4x):
;   1F0CH clear resident RAM, 1F0FH ports, 1F12H interrupt controls,
;   1F15H model detect, 1F24H DAC write, 1F27H output expander,
;   1F2AH panel receive recovery, 1F2DH send one panel byte,
;   15C8H panel_text8, 1138H 10 ms wait on timer 1 (TMIC1.7 polled).

	.include "tkx4x_maskrom_symbols.inc"	; mask ROM names; includes uPD78312A.inc
	.include "panel_codes.inc"		; panel link bytes (include/panel_codes.h)

; ---- Mask ROM entries not in the symbols file ----
	.equ	wait_10ms, 0x1138	; TMC1=0, MD1=2710H, run, poll TMIC1.7. No WDT kick.

; ---- Resident RAM used by the mask ROM's panel link ----
	.equ	panel_q0, 0xFE2C	; oldest received byte (valid while RXF.2)
	.equ	panel_q1, 0xFEBC	; next three (valid while RXQ.0-2)
	.equ	panel_q2, 0xFEBD
	.equ	panel_q3, 0xFEBE
	.equ	RXF, 0xFEDA		; bit 1 link busy, bit 2 panel_q0 holds a byte
	.equ	RXQ, 0xFEDD		; bits 0-2: panel_q1..q3 hold a byte

; ---- Our RAM, FE40H-FE4FH (cleared by 1F0CH at start) ----
	.equ	beepno, 0xFE40		; selected beep number 01H-0DH
	.equ	chrcode, 0xFE41		; first code of the next character page
	.equ	pnlbyte, 0xFE42		; next panel byte for Vol DN (84H/87H)
	.equ	echo0, 0xFE43		; last four unknown panel bytes, oldest first
	.equ	echo1, 0xFE44
	.equ	echo2, 0xFE45
	.equ	echo3, 0xFE46
	.equ	tbuf, 0xFE48		; 8-byte text buffer (FE48H-FE4FH)

; ---- Constants ----
	.equ	VOLUME, 0x5B		; DAC 5 value: entry 20 of the mask ROM's volume table (0D14H)
	.equ	OUTEXP_RESET, 0x25	; mask ROM reset value of the expander shadow (AM set)

	.macro	KICK
	mov	wdm, #0x00
	mov	wdm, #0x86
	.endm

; ======================================================================
; 8000H: interrupt vectors. Not used while TPF is clear; filled anyway so
; a stray TPF set lands on a RETI instead of erased flash.
; ======================================================================
	.org	0x8000
	.rept	32
	.word	int_reti
	.endr

; 8040H: CALLT table. Not used (no CALLT here); every slot is a RET.
	.rept	32
	.word	callt_ret
	.endr

; 8080H: the mask ROM jumps to 8080H + power-on key mode: 8080H normal,
; 8083H dealer, 8086H tuning, 8089H clone. All but clone start the test.
	br	!start			; 8080H
	br	!start			; 8083H
	br	!start			; 8086H
	br	!trampoline_clone_mode	; 8089H

int_reti:
	reti
callt_ret:
	ret

; 8235H: the mask ROM's cooperative EEPROM routines call the application
; here while they wait. Not used by this image; a RET keeps it safe.
	.space	0x8235-$, 0xFF
	ret

; ======================================================================
; Start-up. Same order as the stock application's (hardware reference
; 6.2 of the panel link notes): the mask ROM has already done the panel
; handshake before jumping to 8080H.
; ======================================================================
start:
	di
	movw	sfr[SP], #0xFE20	; stack FE1FH down; 32 bytes
	KICK
	call	!trampoline_clear_resident_ram	; FE20H-FEEFH = 0
	call	!trampoline_gpio_init		; ports; P0.4 high
	call	!trampoline_irq_periph_init	; interrupt controls (TMIC1 = 80H: unmasked!)
	set1	sfr[TMIC1].6		; mask timer 1: wait_10ms polls its flag
	call	!trampoline_model_rf_detect
	mov	outexp_shadow, #OUTEXP_RESET
	call	!trampoline_outexp_commit
	mov	a, #0x05		; DAC 5: volume (stock tuning command 1DH)
	mov	x, #VOLUME
	call	!trampoline_dac_write
	mov	beepno, #BEEP_FIRST
	mov	chrcode, #0x20
	mov	pnlbyte, #0x84
	mov	echo0, #0xFF
	mov	echo1, #0xFF
	mov	echo2, #0xFF
	mov	echo3, #0xFF
	clr1	sfr[EXIC0].7		; drop a stale panel start edge
	clr1	sfr[EXIC0].6		; unmask EXIF0: the panel link runs
	ei
	movw	hl, #s_boot		; "BENCH  1" through the mask ROM routine
	call	!panel_text8

; ======================================================================
; Main loop: kick the watchdog, take panel bytes, run the key's test.
; ======================================================================
main:
	KICK
	call	!trampoline_panel_rx_recover	; catch a missed start edge
	bf	RXF.2, main		; nothing received
	call	!rx_pop			; A = oldest byte
	call	!dispatch
	br	main

; rx_pop: A = panel_q0; move the queue up. Interrupts off while moving,
; since the tick handler adds to the queue.
rx_pop:
	di
	mov	a, panel_q0
	mov	x, a
	clr1	RXF.2
	bf	RXQ.0, rxp1
	set1	RXF.2
rxp1:	clr1	RXQ.0
	bf	RXQ.1, rxp2
	set1	RXQ.0
rxp2:	clr1	RXQ.1
	bf	RXQ.2, rxp3
	set1	RXQ.1
rxp3:	clr1	RXQ.2
	mov	panel_q0, panel_q1
	mov	panel_q1, panel_q2
	mov	panel_q2, panel_q3
	mov	panel_q3, #0xFF
	ei
	mov	a, x
	ret

; dispatch: run the test for a key press; ignore key releases; show
; anything else as hex.
dispatch:
	cmp	a, #PANEL_KEY_SYS_UP
	bne	dsp1
	br	!k_sysup
dsp1:	cmp	a, #PANEL_KEY_SYS_DN
	bne	dsp2
	br	!k_sysdn
dsp2:	cmp	a, #PANEL_KEY_GRP_UP
	bne	dsp3
	br	!k_grpup
dsp3:	cmp	a, #PANEL_KEY_GRP_DN
	bne	dsp4
	br	!k_grpdn
dsp4:	cmp	a, #PANEL_KEY_SCAN
	bne	dsp5
	br	!k_scan
dsp5:	cmp	a, #PANEL_KEY_AUX
	bne	dsp6
	br	!k_aux
dsp6:	cmp	a, #PANEL_KEY_VOL_UP
	bne	dsp7
	br	!k_volup
dsp7:	cmp	a, #PANEL_KEY_VOL_DN
	bne	dsp8
	br	!k_voldn
dsp8:	cmp	a, #PANEL_KEY_SYS_UP_REL	; the eight releases
	be	dsp_ret
	cmp	a, #PANEL_KEY_SYS_DN_REL
	be	dsp_ret
	cmp	a, #PANEL_KEY_GRP_UP_REL
	be	dsp_ret
	cmp	a, #PANEL_KEY_GRP_DN_REL
	be	dsp_ret
	cmp	a, #PANEL_KEY_VOL_UP_REL
	be	dsp_ret
	cmp	a, #PANEL_KEY_VOL_DN_REL
	be	dsp_ret
	cmp	a, #PANEL_KEY_SCAN_REL
	be	dsp_ret
	cmp	a, #PANEL_KEY_AUX_REL
	be	dsp_ret
	br	!k_echo
dsp_ret:
	ret

; ======================================================================
; The tests. Each one ends in RET back to the main loop.
; ======================================================================

; Sys UP: text through our own loop.
k_sysup:
	call	!blank
	movw	hl, #s_loop
	br	!text8_loop

; Sys DN: text through the mask ROM's unrolled panel_text8.
k_sysdn:
	call	!blank
	movw	hl, #s_rom
	br	!panel_text8

; Grp UP / Grp DN: choose the beep number.
k_grpup:
	inc	beepno
	cmp	beepno, #BEEP_LAST+1
	bne	show_beep
	mov	beepno, #BEEP_FIRST
	br	show_beep
k_grpdn:
	dec	beepno
	bne	show_beep
	mov	beepno, #BEEP_LAST
show_beep:
	call	!blank
	movw	hl, #s_beep		; "BEEP nn "
	mov	a, beepno
	br	!text_with_hex

; Scan: unmute (expander AM and AC clear, as every stock beep routine
; does), show the number, then B3H and the number.
k_scan:
	clr1	outexp_shadow.0		; AM off
	clr1	outexp_shadow.1		; AC off
	call	!trampoline_outexp_commit
	call	!blank
	movw	hl, #s_play		; "PLAY nn "
	mov	a, beepno
	call	!text_with_hex
	mov	a, #PANEL_BEEP
	call	!trampoline_panel_tx_queue
	mov	a, beepno
	br	!trampoline_panel_tx_queue

; Aux: B4H (the stock code's beep stop, sub_C3CC), then mute again.
k_aux:
	mov	a, #PANEL_BEEP_STOP
	call	!trampoline_panel_tx_queue
	mov	outexp_shadow, #OUTEXP_RESET
	call	!trampoline_outexp_commit
	call	!blank
	movw	hl, #s_stop
	br	!panel_text8

; Vol UP: one page of the character table.
k_volup:
	call	!blank
	movw	hl, #tbuf		; "C xx-yy "
	mov	a, #'C'
	mov	[hl+], a
	mov	a, #' '
	mov	[hl+], a
	mov	a, chrcode
	call	!hex2
	mov	a, #'-'
	mov	[hl+], a
	mov	a, chrcode
	add	a, #7
	call	!hex2
	mov	a, #' '
	mov	[hl+], a
	movw	hl, #tbuf
	call	!panel_text8
	call	!wait_1s
	movw	hl, #tbuf		; the 8 codes themselves
	mov	a, chrcode
	mov	c, #8
kvu1:	mov	[hl+], a
	inc	a
	dbnz	c, kvu1
	movw	hl, #tbuf
	call	!panel_text8
	add	chrcode, #8		; next page; after 78H-7FH back to 20H
	cmp	chrcode, #0x80
	bne	kvu2
	mov	chrcode, #0x20
kvu2:	ret

; Vol DN: panel byte 84H or 87H alone, alternately.
k_voldn:
	call	!blank
	movw	hl, #s_pnl		; "PNL nn  "
	mov	a, pnlbyte
	call	!text_with_hex
	call	!wait_1s
	mov	a, pnlbyte
	call	!trampoline_panel_tx_queue
	cmp	pnlbyte, #0x84
	bne	kvd1
	mov	pnlbyte, #0x87
	ret
kvd1:	mov	pnlbyte, #0x84
	ret

; Anything else from the panel: shift it into the echo history, show all
; four as hex.
k_echo:
	mov	echo0, echo1
	mov	echo1, echo2
	mov	echo2, echo3
	mov	echo3, a
	movw	hl, #tbuf
	mov	a, echo0
	call	!hex2
	mov	a, echo1
	call	!hex2
	mov	a, echo2
	call	!hex2
	mov	a, echo3
	call	!hex2
	movw	hl, #tbuf
	br	!panel_text8

; ======================================================================
; Helpers
; ======================================================================

; text8_loop: 02H, then the 8 characters at HL, one CALL 1F2DH each, in a
; loop. 1F2DH changes only A.
text8_loop:
	mov	a, #PANEL_TEXT
	call	!trampoline_panel_tx_queue
	mov	c, #8
tl1:	mov	a, [hl+]
	call	!trampoline_panel_tx_queue
	dbnz	c, tl1
	ret

; text_with_hex: copy the 8-character template at HL to tbuf, put A as
; two hex digits at positions 5-6, show tbuf through panel_text8.
text_with_hex:
	push	{ax}
	movw	de, #tbuf
	mov	c, #8
twh1:	mov	a, [hl+]
	mov	[de+], a
	dbnz	c, twh1
	pop	{ax}
	movw	hl, #tbuf+5
	call	!hex2
	movw	hl, #tbuf
	br	!panel_text8

; hex2: A as two upper-case hex digits at HL, HL += 2. Changes A and X.
hex2:
	mov	x, a
	shr	a, 4
	call	!nibble
	mov	[hl+], a
	mov	a, x
	and	a, #0x0F
	call	!nibble
	mov	[hl+], a
	ret

nibble:
	cmp	a, #10
	bl	nib1
	add	a, #7			; 'A'-'9'-1
nib1:	add	a, #'0'
	ret

; blank: 8 spaces, then 0.3 s, so each key press visibly redraws.
blank:
	movw	hl, #s_blank
	call	!panel_text8
	mov	b, #30
	br	wait_b

; wait_1s / wait_b: B x 10 ms, kicking the watchdog every 10 ms.
wait_1s:
	mov	b, #100
wait_b:
	KICK
	call	!wait_10ms
	dbnz	b, wait_b
	ret

; ======================================================================
; Strings (8 characters each)
; ======================================================================
s_boot:		.ascii	"BENCH  1"
s_loop:		.ascii	"LOOP  OK"
s_rom:		.ascii	"ROM   OK"
s_beep:		.ascii	"BEEP    "
s_play:		.ascii	"PLAY    "
s_stop:		.ascii	"STOP B4 "
s_pnl:		.ascii	"PNL     "
s_blank:	.ascii	"        "

; ======================================================================
; E580H: the signature the mask ROM checks at reset (E580H-E586H). The
; stock images carry the full string; only the first 7 bytes are compared.
; ======================================================================
	.space	0xE580-$, 0xFF
	.ascii	"KENWOOD Corporation LTR Mobile"
	.space	0xE600-$, 0xFF
