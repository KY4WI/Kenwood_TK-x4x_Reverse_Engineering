; SPDX-License-Identifier: GPL-3.0-only
; Copyright (c) 2026 Dave, KY4WI
; =====================================================================
; TK-840 replacement-firmware Stage 1: boot without touching RF.
;
; Per the confirmed mask-ROM handoff contract (traced in the mask ROM
; and cross-checked against the hardware reference, sections
; 15/16/20): the mask ROM, after its own boot
; sequence, dispatches to exactly one of four FIXED flash addresses
; depending on which panel keys are held at power-on:
;   $8080 (default/no key), $8083, $8086, $8089
; This image targets the default case ($8080) and is a real jump
; (BR ax), not a call -- there is no return path, and no vector-table
; entry to satisfy for the initial handoff itself.
;
; Every .org below is written as a REAL flash/CPU address minus the
; FLASH_BASE equ (0x8000, matching vlink's -Ttext base in build.sh), not
; as a hand-computed raw file offset -- vasm folds this to a plain
; constant at assemble time (confirmed). This is a direct
; fix for the class of bug that earlier put the KENWOOD signature at the
; wrong address (a manual-subtraction mistake, 0x580 instead of 0x6580):
; the arithmetic is now the assembler's job, not a human's.
;
; The final linked image must be exactly 26112 (0x6600) bytes -- the
; full 0x8000-0xE5FF application flash region -- per the real
; device/programmer's requirement, confirmed on the bench. See the
; final_pad_byte block near the end of this file.
;
; Layout (linked at base 0x8000, see build.sh):
;   0x8000-0x803F  external vector table (32 words) -- all point at a
;                  harmless RETI trap. Never actually load-bearing:
;                  crt0's first instruction is DI, and DI blocks every
;                  maskable source from ever vectoring. Provided only
;                  because 17.6 of the hardware doc warns not to assume
;                  this region is passively unused, and because NMI is
;                  non-maskable (DI does not block it) -- so a real,
;                  harmless target matters here even though we expect
;                  it to never actually fire in Stage 1.
;   0x8040-0x807F  external CALLT table (32 words), reconstructed per
;                  hardware doc section 15. All slots point at a bare
;                  RET stub except slot 6 (0x804C), which preserves the
;                  original ABI value 0x8235 (the EEPROM cooperative-API
;                  background callback, doc section 4.2) -- we are not
;                  using the CALLT instruction ourselves yet, but the
;                  doc's own guidance is not to leave this table to
;                  linker accident.
;   0x8080         crt0 entry (the confirmed default-case handoff addr)
;   0x8235         app_eeprom_bg_callback: required real code at this
;                  fixed address per hardware doc 4.2, RET-compatible
;                  stub (Stage 1 does not use the cooperative EEPROM
;                  API, so background servicing is not required yet).
;
; crt0 reuses the resident mask-ROM startup routines in the exact order
; the original application used them (hardware doc 4.3, and directly
; confirmed by tracing the mask ROM's own reset handler),
; then brings up a from-scratch, POLLED (not interrupt-driven) bit-bang
; serial transmitter and sends "hello\r\n" forever on P15 (sfr[0xFF01]
; bit 5), matching the pin the mask ROM's OWN KPG-monitor bit-bang
; routine drives (confirmed by tracing routines at
; 0x0f76/0x0fa4), at 9600 8N1, using the exact reload value the mask
; ROM itself uses for its 9600 baud mode (movw sfr[0x8e].w,#0x0067).
;
; This is Stage 1 only: prove the CPU actually runs our own code, at
; the address the mask ROM actually hands off to, with no unverified
; assumption about hardware access. It is NOT yet a validated
; production bit-bang UART -- see the "what this does not prove" note
; at the end of this file before relying on it for anything beyond a
; bench smoke test.
;
; NOTE ON THE ASSEMBLER'S REAL COVERAGE (see cpu.c's own header
; comment): MOV1/SET1/CLR1/BT/BF (the bit-addressed instructions the
; mask ROM's OWN bit-bang code uses) are NOT implemented by this
; project's vasm backend yet. Every place below that needs to test or
; drive a single SFR bit therefore does it the long way -- byte-wide
; read/AND-or-OR/write through the accumulator -- instead of the
; shorter real instruction the mask ROM uses. Confirmed by checking
; mnemonics_gen.c directly before writing this file, not assumed.
; =====================================================================

	.text

; --- link-time base address of this image. vlink is invoked with
; "-Ttext 0x8000" (see build.sh) -- this .equ exists so every .org below
; can be written as a REAL flash/CPU address minus this base, instead of
; a hand-subtracted raw file offset. vasm folds "<realaddr>-FLASH_BASE"
; to a plain constant at assemble time (confirmed via a
; standalone test: three .org lines written as 0x8000-FLASH_BASE,
; 0x8235-FLASH_BASE, and 0xe580-FLASH_BASE all landed at the correct
; file offsets, byte-verified). This is a real, directly-attributable
; fix for the exact class of bug that put the KENWOOD signature at the
; wrong address earlier in this project (0x580 vs 0x6580, a manual
; subtraction mistake) -- the assembler does the arithmetic now, not a
; human reading a comment. ---
	.equ	FLASH_BASE, 0x8000

; --- resident mask-ROM entry points this file calls into: named in
; annotations/maskrom/tkx4x_maskrom_symbols.inc ---
	.include	"tkx4x_maskrom_symbols.inc"	; mask ROM names (trampolines, resident RAM)

; =====================================================================
; 0x8000-0x803F: external vector table. TPF is confirmed set by the
; time the mask ROM hands off control (its own CALLT table already
; decodes correctly at 0x8040-0x807F, which only happens when TPF=1),
; so this is the table the hardware will actually use if anything ever
; vectors. Every slot points at int_trap.
; =====================================================================
	.org	0x8000-FLASH_BASE,0xff
	; 0x00 RESET (not really used this way; harmless)
	.word	0x8200
	; 0x02 NMI (non-maskable -- the one real one here)
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
	; 0x10 TMF1
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
; 0x8040-0x807F: external CALLT table, reconstructed per hardware doc
; section 15. We do not issue CALLT ourselves in Stage 1; this exists
; so nothing that assumes the original ABI (mask ROM or otherwise)
; lands on undefined flash. Slot 6 (0x804C) preserves the real
; original value (0x8235) rather than pointing at the generic stub.
; =====================================================================
	.org	0x8040-FLASH_BASE,0xff
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
; 0x8080: crt0. The confirmed mask-ROM default-case handoff address.
; =====================================================================
	.org	0x8080-FLASH_BASE,0xff
crt0:
	; belt-and-suspenders: mask ROM's own trace shows
	di
				; EI before this handoff, and we don't want any
				; peripheral (Timer0/Timer1 are already running
				; per the mask ROM's own 0x9E init routine) to
				; vector anywhere until we've set up our own
				; vector table's contents deliberately, above.
	; SP: reuse the mask ROM's own confirmed-good
	movw	sfr[0xfc].w,#0xfe20
				; stack area/value (real evidence, not a guess).

	; 1F0F -- confirmed called 1st
	call	trampoline_gpio_init
	; 1F0C -- confirmed called 2nd
	call	trampoline_clear_resident_ram
	; 1F12 -- confirmed called 3rd
	call	trampoline_irq_periph_init

	; BU4094 output-expander shadow byte: reuse the
	mov	outexp_shadow,#0x25
				; exact known-safe value the mask ROM's own boot
				; sequence writes here (real evidence), per
				; Stage-1 doc guidance to "preserve a known-safe
				; 4094 shadow state."
	; 1F27 -- commit it
	call	trampoline_outexp_commit

	; --- bring up P15 (sfr[0xFF01] bit 5) as our bit-bang TX pin,
	; idle-high, output. Preload the output latch high BEFORE
	; switching direction, so there is no glitch on the pin the
	; instant it becomes an output. ---
	mov	a,sfr[0x01]
	or	a,#0x20
	mov	sfr[0x01],a
	mov	a,sfr[0x21]
	and	a,#0xdf
	mov	sfr[0x21],a

	; --- bring up Timer1 (TMC1=sfr[0x82], MD1=sfr[0x8e].w, flag
	; TMIC1.7=sfr[0xd0].7) as our bit-clock, in POLLED mode --
	; confirmed real evidence: mask ROM's own routine at 0x0f76 uses
	; this exact reload value (0x0067) for its 9600-baud mode, and
	; mask ROM's own routine at sub_0144 (0x144) polls exactly this
	; flag/register in exactly this way (bf sfr[0xd0].7 / clr1
	; sfr[0xd0].7) as a hardware-timed delay -- we reuse the same
	; timer and the same flag, just for our own reload value and our
	; own (byte-wide, since this assembler has no bit ops) polling
	; sequence. ---
	; stop timer1 first (it may already be
	mov	sfr[0x82],#0x00
					; running from the mask ROM's own 0x9E
					; init, with a different reload)
	; MD1 = 0x0067 (real mask-ROM 9600 value)
	movw	sfr[0x8e].w,#0x0067
	mov	a,sfr[0xd0]
	; clear any stale pending flag (bit 7),
	and	a,#0x7f
	; leave MK/PR bits (0-6) untouched
	mov	sfr[0xd0],a
	; start timer1
	mov	sfr[0x82],#0x80

main_loop:
	mov	a,#'h'
	call	send_byte
	mov	a,#'e'
	call	send_byte
	mov	a,#'l'
	call	send_byte
	mov	a,#'l'
	call	send_byte
	mov	a,#'o'
	call	send_byte
	mov	a,#0x0d
	call	send_byte
	mov	a,#0x0a
	call	send_byte
	br	main_loop

; ---------------------------------------------------------------------
; wait_tick: block until one Timer1 bit-period has elapsed, then
; consume (clear) the flag. Byte-wide equivalent of the mask ROM's own
; "bf sfr[0xd0].7,$ / clr1 sfr[0xd0].7" idiom (real evidence, sub_0144),
; since this assembler build has no bit-test/bit-clear instructions.
; Clobbers: a.
; ---------------------------------------------------------------------
wait_tick:
	mov	a,sfr[0xd0]
	and	a,#0x80
	; loop while flag bit (7) reads as 0
	be	wait_tick
	mov	a,sfr[0xd0]
	; clear flag, preserve MK/PR
	and	a,#0x7f
	mov	sfr[0xd0],a
	ret

; ---------------------------------------------------------------------
; send_byte: transmit A as one 8N1 UART frame (LSB first) on P15
; (sfr[0xFF01] bit 5), busy-waiting one Timer1 tick per bit. Returns
; with the pin idle-high. Clobbers: a, x, c.
; ---------------------------------------------------------------------
send_byte:
	; stash the byte to send (need A free
	mov	x,a
					; for SFR access)
	; --- start bit (always 0) ---
	mov	a,sfr[0x01]
	and	a,#0xdf
	mov	sfr[0x01],a
	call	wait_tick
	; restore byte to send
	mov	a,x

	mov	c,#8
send_byte_bitloop:
	; cy = next data bit (lsb first),
	rorc	a,1
					; a = remaining bits
	; stash remainder (A needed for SFR access)
	mov	x,a
	bnl	send_byte_bit0
	mov	a,sfr[0x01]
	or	a,#0x20
	br	send_byte_bitdrive
send_byte_bit0:
	mov	a,sfr[0x01]
	and	a,#0xdf
send_byte_bitdrive:
	mov	sfr[0x01],a
	call	wait_tick
	; restore remainder for next rotate
	mov	a,x
	dbnz	c,send_byte_bitloop

	; --- stop bit (always 1 = idle) ---
	mov	a,sfr[0x01]
	or	a,#0x20
	mov	sfr[0x01],a
	call	wait_tick
	ret

; ---------------------------------------------------------------------
; Shared stubs.
;
; NOTE: vasm's .word directive in this build only accepts constant
; numeric expressions, not forward/backward symbol references (confirmed
; by direct experiment -- .word <label> fails to assemble, with or
; without the label already defined earlier in the file; a real gap in
; this project's vasm backend's data-directive operand parsing, separate
; from the already-fixed/confirmed-working INSTRUCTION-operand branch
; case). So int_trap and callt_stub are pinned to fixed offsets with
; .org below, and the vector/CALLT tables above reference them as plain
; literal addresses (0x8200/0x8210) instead of by symbol -- not because
; the addresses are meaningful, only because they had to be picked and
; hardcoded somewhere. Real CALL/BR references elsewhere in this file
; (trampoline_*, main_loop, send_byte, wait_tick, ...) are INSTRUCTION
; operands, which DO support real symbolic relocation -- that path is
; unaffected and this file still uses labels for all of those.
; ---------------------------------------------------------------------
	.org	0x8200-FLASH_BASE,0xff
int_trap:
	reti

	.org	0x8210-FLASH_BASE,0xff
callt_stub:
	ret

; =====================================================================
; 0x8235: required real code at this fixed address per hardware doc
; section 4.2 (mask ROM's cooperative EEPROM routines 1F42/1F45 branch
; here directly, by hardcoded absolute address, while waiting on EEPROM
; activity). Stage 1 does not use that API, so a bare RET-compatible
; stub is the documented minimum-ABI-safe implementation.
; =====================================================================
	.org	0x8235-FLASH_BASE,0xff
app_eeprom_bg_callback:
	ret

; =====================================================================
; 0xE580: REQUIRED "KENWOOD" application signature.
;
; Confirmed real evidence (traced directly, this address had been mis-
; described in an earlier session summary as "RAM" -- it is not; it is
; inside the application flash region, 0x7F bytes before the flash
; region's own documented end at 0xE5FF): the mask ROM's own reset
; handler, right after EI and the panel handshake, does this BEFORE it
; ever reaches the 0x8080/83/86/89 dispatch:
;
;   call 0x018b   ; movw de,#0xe580 / movw hl,#0x08d7 / mov c,#0x07
;                 ; cmpbke [de+],[hl+]  -- compares 7 bytes at flash
;                 ; 0xE580 against the mask ROM's own stored string at
;                 ; 0x08D7, "KENWOOD Corporation LTR Mobile" (only the
;                 ; first 7 bytes, "KENWOOD", are ever compared)
;   be   0x0088   ; MATCH -> proceed to the real key-scan/0x8080 table
;   ...           ; MISMATCH falls through instead: sets a "signature
;                 ; bad" RAM flag (0xfedd.3) and branches to mask-ROM
;                 ; address 0x0273 -- an internal panel error/recovery
;                 ; routine -- and NEVER reaches 0x8080 at all.
;
; Without this signature, this entire image is unreachable on real
; hardware, regardless of how correct the rest of it is. This is not
; optional or cosmetic -- it is a hard gate the mask ROM itself enforces
; before it will run ANY application image.
; =====================================================================
	.org	0xe580-FLASH_BASE,0xff
app_signature:
	.byte	'K','E','N','W','O','O','D'

; =====================================================================
; 0xE5FF: final padding byte, forcing the linked image out to the full,
; required output size of exactly 26112 (0x6600) bytes -- confirmed on
; the bench against the real device/programmer. This is not an arbitrary
; number: 0x8000 (this image's base) through 0xE5FF inclusive is exactly
; 0x6600 = 26112 bytes, which matches this file's own documented end of
; the application flash region (see the app_signature block above, "0x7F
; bytes before the flash region's own documented end at 0xE5FF"). So
; padding out to 0xE5FF is padding out to the full flash region, not an
; independently-chosen size.
;
; A bare .org with nothing emitted after it does not by itself guarantee
; the linker writes bytes all the way out to that reach point (raw-bin
; output size is driven by the highest REAL emitted byte, not by a
; location-counter move with no atom attached) -- so this deliberately
; emits one real 0xff byte at the target address, rather than relying on
; .org alone, to guarantee the file is exactly 26112 bytes long. Verified
; this session: after this change, the built stage1.bin size was checked
; directly.
; =====================================================================
	.org	0xe5ff-FLASH_BASE,0xff
final_pad_byte:
	.byte	0xff

; =====================================================================
; What this DOES prove, once it assembles/links/disassembles clean:
;   - the exact byte layout the mask ROM hands off to is real, valid
;     78K/III machine code, not an assumption
;   - the control-flow/branch encoding is correct (this project has
;     twice found real encoding bugs by disassembling before trusting
;     assembly text, per machine.c's own PUSH/ASSIGN task notes and the
;     .text-vs-.org ABSLABEL finding above) -- so verification here
;     means disassembling the linked binary and hand-checking it, not
;     just eyeballing this source
;   - the same is done here below, before this is handed back
;
; What this does NOT yet prove (needs real hardware):
;   - that the Timer1 reload value 0x0067 actually yields ~9600 Hz on
;     THIS silicon with the prescaler this project inferred only from
;     matching the mask ROM's own 1200/1200*8 baud-ratio arithmetic,
;     not from a located datasheet prescaler-selection bit description
;   - that trampoline_gpio_init doesn't itself reconfigure P15/PM1 in a
;     way that fights this file's own P15 setup (should be harmless
;     either way since we set it up AFTER calling it, but not bench
;     confirmed)
;   - real UART framing/voltage levels on a scope or a real terminal
; This is exactly what Stage 1 bring-up on real hardware is for.
; =====================================================================
