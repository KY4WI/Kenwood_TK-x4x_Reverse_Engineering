; SPDX-License-Identifier: GPL-3.0-only
; Copyright (c) 2026 Dave, KY4WI
; =====================================================================
; TK-x4x replacement firmware: resident-ROM API shim layer.
;
; PURPOSE: let ordinary vbcc78k3-compiled C code call the mask ROM's own
; resident service routines directly, as normal C function calls, e.g.
;   eeprom_seq_read(0xF4, buf, 2);
; instead of every caller hand-rolling register setup + a bare CALL.
;
; WHY THIS FILE HAS TO EXIST (confirmed, not assumed): the
; mask ROM's resident routines use a per-function, hand-designed calling
; convention (some arguments in registers, some via fixed global RAM
; variables) -- confirmed by disassembling real call sites in the
; captured original application (TK-840_firmware_8355.bin) and, for the
; functions below, independently re-confirmed by disassembling the
; RESIDENT IMPLEMENTATIONS THEMSELVES in the mask ROM (see the evidence
; note on each function below). This is NOT the same convention
; vbcc78k3 uses for C-to-C calls -- vbcc78k3's own ABI is a self-invented
; stack-based scheme (see machine.c's own comment: NEC's real compiler
; ABI was never available to build against), confirmed by
; compiling representative multi-argument C functions and reading the
; real generated code. The two conventions are genuinely different, so a
; bare function-pointer-cast call from C is not correct for these
; functions -- each one needs a thin, hand-written shim that reads
; vbcc's C arguments off the stack and re-marshals them into whatever
; the resident routine actually expects.
;
; CONFIRMED vbcc78k3 C CALLING CONVENTION (empirical, from compiling and
; reading real output, not from documentation):
;   - Arguments are pushed by the CALLER into a stack-resident argument-
;     build area directly above the 2-byte return address CALL pushes.
;   - Every argument occupies a full 2-byte slot, regardless of its C
;     type's actual width (a u8 argument is zero-extended to fill the
;     slot).
;   - The FIRST (leftmost) parameter sits at the LOWEST address, i.e.
;     closest to the return address / SP.
;   - The callee's own prologue (if it has local variables) subtracts
;     its locals size from SP; every shim below has ZERO local
;     variables, so it needs no prologue/epilogue at all -- arguments
;     are read straight off [sp+2], [sp+4], [sp+6], ... (offset 0-1 is
;     the return address CALL just pushed).
;   - The return value comes back in AX.
;   - vbcc78k3 mangles a C function name `foo` to the assembly label
;     `_foo` -- every shim below is named to match a C prototype in
;     resident_api.h exactly this way, so an ordinary C call compiles
;     straight to "call _foo" and just works.
;
; Every function below was checked BOTH ways: (1) real call sites to the
; same resident address in the captured original firmware, and (2) the
; resident routine's own disassembly in the mask ROM, confirming it
; actually consumes the registers/RAM this shim sets up. Anything not
; checked this thoroughly is left out of this file rather than guessed.
; =====================================================================

; Mask ROM names (trampoline entries, resident RAM):
; annotations/maskrom/tkx4x_maskrom_symbols.inc in the RE
; repository (the custom firmware keeps a copy, refreshed by its
; update_maskrom_symbols.sh).
	.include "tkx4x_maskrom_symbols.inc"

	.text

; ---------------------------------------------------------------------
; void eeprom_seq_read(u8 addr, void *dest, u8 count);
;
; Reads `count` bytes from the AT24C02 EEPROM starting at `addr` into
; the buffer at `dest`.
;
; Evidence: [RE, cross-checked against mask ROM]. Caller-side convention
; (HL=dest pointer, B=count, 0xFE29=EEPROM start address) confirmed from
; six independent real call sites in TK-840_firmware_8355.bin, all
; identical in shape. Independently re-confirmed by disassembling the
; resident implementation itself (mask ROM 0x1F21 -> 0x0729): its body
; is "mov [hl+],a / dec b / bne ..." -- HL genuinely is an
; auto-incrementing destination pointer and B genuinely is a decrementing
; byte counter, exactly as the caller-side evidence implied.
;
; No confirmed status/error return -- callers in the original firmware
; simply read the buffer back afterward. If EEPROM ops can fail, that
; return path has not been identified yet; treat this as best-effort.
; ---------------------------------------------------------------------
	.global	_eeprom_seq_read
_eeprom_seq_read:
	mov	a,[sp+2]
	mov	eeprom_addr,a
	mov	a,[sp+4]
	mov	l,a
	mov	a,[sp+5]
	mov	h,a
	mov	a,[sp+6]
	mov	b,a
	call	!trampoline_eeprom_seq_read
	ret

; ---------------------------------------------------------------------
; void eeprom_page_write(u8 addr, const void *src, u8 count);
;
; Writes `count` bytes from the buffer at `src` to the AT24C02 EEPROM
; starting at `addr`.
;
; Evidence: [RE, cross-checked against mask ROM]. Same caller-side
; pattern as eeprom_seq_read (HL=pointer, B=count, 0xFE29=address),
; confirmed from two real call sites. Independently re-confirmed by
; disassembling the resident implementation (mask ROM 0x1F18 -> 0x0773):
; its body reads "mov a,0xfe29" directly (address) and "mov a,[hl+]"
; (auto-incrementing SOURCE pointer, read direction) with a "dec b"
; counter loop -- exactly matching this shim's setup, just consumed in
; the opposite data direction from eeprom_seq_read.
;
; Same caveat as eeprom_seq_read: no confirmed status/error return.
; ---------------------------------------------------------------------
	.global	_eeprom_page_write
_eeprom_page_write:
	mov	a,[sp+2]
	mov	eeprom_addr,a
	mov	a,[sp+4]
	mov	l,a
	mov	a,[sp+5]
	mov	h,a
	mov	a,[sp+6]
	mov	b,a
	call	!trampoline_eeprom_page_write
	ret

; ---------------------------------------------------------------------
; void bu4094_commit(void);
;
; Shifts the current 8-bit output-expander shadow byte (RAM 0xFE28) out
; to the BU4094 over P01(clock)/P02(data)/P03(strobe). Writes the WHOLE
; expander state every call -- callers must read-modify-write the
; 0xFE28 shadow themselves (via bu4094_write() below, or directly) and
; never invent an arbitrary byte, per the hardware doc's own warning.
;
; Evidence: [RE, cross-checked against mask ROM]. Caller-side: zero
; register setup immediately before the call in the one confirmed real
; call site (TK-840_firmware_8355.bin, crt0-equivalent boot sequence --
; the same "mov 0xfe28,#<val> / call 0x1f27" pattern our own Stage-1
; crt0 already uses). Independently re-confirmed by disassembling the
; resident implementation (mask ROM 0x1F27 -> 0x06F4): it opens with
; "mov a,0xfe28" (reads the shadow byte directly, no register argument)
; and bit-bangs it out via sfr[0xff00] bits .1/.2/.3, matching the
; hardware doc's documented clock/data/strobe pin roles exactly.
; ---------------------------------------------------------------------
	.global	_bu4094_commit
_bu4094_commit:
	call	!trampoline_outexp_commit
	ret

; ---------------------------------------------------------------------
; void bu4094_write(u8 value);
;
; Convenience wrapper: sets the output-expander shadow byte to `value`
; and commits it. NOT a read-modify-write -- this replaces the entire
; expander state, same as writing 0xFE28 directly. Provided because
; "set the whole byte and commit" is the common case; for a single-bit
; change, read SFR8(0xFE28), modify the bit, then call this (or
; bu4094_commit() after writing SFR8(0xFE28) yourself).
; ---------------------------------------------------------------------
	.global	_bu4094_write
_bu4094_write:
	mov	a,[sp+2]
	mov	outexp_shadow,a
	call	!trampoline_outexp_commit
	ret

; ---------------------------------------------------------------------
; Added for Phase 1 firmware integration. Same conventions
; and evidence bar as above.
; ---------------------------------------------------------------------

; ---------------------------------------------------------------------
; void wdt_service(void);
;
; Kicks the watchdog: MOV WDM,#0x00 then MOV WDM,#0x86. WDM is a
; protected register -- vasm's cpu.c already emits the required
; one's-complement guard byte for this special "MOV WDM,#byte" form
; (confirmed 82/82 against real mask-ROM instances, see cpu.c's own
; comment), so plain "mov wdm,#imm" here is correct as written, NOT a
; generic SFR write. No arguments, no return value, no locals -- this
; needs no prologue/epilogue either, same as bu4094_commit above.
;
; Evidence: [DS p.275 for the protected-write requirement]; [FW] for
; the 0x00-then-0x86 sequence itself -- used identically, back-to-back,
; in every one of 82 mask-ROM and 36 external-firmware watchdog-service
; call sites found in the captured images. This exact value pair, not
; independently derived from the datasheet's 4-interval encoding (that
; bit layout is undocumented in available sources) -- reuse it rather
; than inventing another one.
; ---------------------------------------------------------------------
	.global	_wdt_service
_wdt_service:
	mov	wdm,#0x00
	mov	wdm,#0x86
	ret

; ---------------------------------------------------------------------
; u8 eeprom_byte_read(u8 addr);
;
; Reads one byte from the AT24C02 EEPROM at `addr` (resident 1F1B,
; "random single-byte read").
;
; Evidence: [RE against mask ROM]. 1F1B sets up from 0xFE29=addr (same
; address-register convention as eeprom_seq_read/eeprom_page_write
; above) and leaves its result in BOTH A and RAM 0xFE2A -- confirmed by
; disassembling 1F1B's own body. A is exactly vbcc78k3's u8 return
; register (confirmed empirically, same convention documented at the
; top of this file), so no explicit "move result into place" step is
; needed after the call -- 1F1B already leaves it where the C caller
; expects it.
; ---------------------------------------------------------------------
	.global	_eeprom_byte_read
_eeprom_byte_read:
	mov	a,[sp+2]
	mov	eeprom_addr,a
	call	!trampoline_eeprom_random_read
	ret

; ---------------------------------------------------------------------
; void model_detect(void);
;
; Runs the resident hardware/model-detect routine (1F15): reads ADC
; inputs AN2/AN3 and writes three flag bits into fixed resident RAM
; (0xFED3.4, 0xFEDA.0, 0xFEDF.4) -- read those afterward with a plain
; RAM8() access (hardware.h), no shim needed for that half. No
; arguments, no return value.
;
; Evidence: [RE against mask ROM] for 1F15's existence, entry address,
; and its three documented output flag bits (see
; docs/hardware/ (the hardware reference) section on model detection). Not
; independently re-verified against a real call site the way the
; EEPROM/4094 shims were (this project has not yet located one), so
; treat the call convention (no arguments) as [RE, single-source] until
; a real call site is found -- the entry's own body taking no register
; arguments is what's confirmed, not a matching caller.
; ---------------------------------------------------------------------
	.global	_model_detect
_model_detect:
	call	!trampoline_model_rf_detect
	ret

; ---------------------------------------------------------------------
; void dac_write(u8 channel, u8 value);
;
; Writes `value` to M62363 DAC channel `channel` (1-8), via a direct
; call to the resident routine (0x1F24) rather than reconstructing the
; original firmware's CALLT slot 19 -- see resident_api.h's comment on
; why that's a safe, equivalent simplification for Phase 1 (CALLT vs.
; CALL is a code-size choice at the call site, not a functional
; difference; 1F24 is the same fixed resident address either way).
;
; Evidence: [RE against mask ROM + 21 real call sites, all via the
; CALLT path]. A=channel, X=value, confirmed from 1F24's own body
; (reads A to select the shift-clock target, X as the 8-bit value shifted
; out) and cross-checked against real call-site register setup.
;
; NEVER invent a calibration value for this -- always source it from
; AT24C02 EEPROM data (hardware doc's explicit warning: wrong DAC
; values can cause frequency error, excessive deviation, wrong TX
; power, or bad audio).
; ---------------------------------------------------------------------
	.global	_dac_write
_dac_write:
	mov	a,[sp+4]
	mov	x,a
	mov	a,[sp+2]
	call	!trampoline_dac_write
	ret

; ---------------------------------------------------------------------
; void panel_send_byte(u8 b);
;
; Queues/starts sending one byte to the front panel over the bit-bang
; link (resident 1F2D). NO TIMEOUT if the previous transfer's busy flag
; (RAM 0xFEDA.1) never clears -- confirmed from 1F2D's own body. Only
; safe to rely on once the panel ISR machinery is actually running
; (EXIC0/0xFFC8 unmasked, 1F30/1F33 wired into the vector table at
; 0x0004/whichever vector -- see crt0.s); otherwise a call here can
; hang forever, per the hardware doc's explicit warning.
;
; Evidence: [RE against mask ROM]. A=byte to send, confirmed from
; 1F2D's own body (loads A into the TX shift register at 0xFE25) and
; cross-checked against real call-site register setup.
; ---------------------------------------------------------------------
	.global	_panel_send_byte
_panel_send_byte:
	mov	a,[sp+2]
	call	!trampoline_panel_tx_queue
	ret

; ---------------------------------------------------------------------
; void panel_rx_recover(void);
;
; Resident 1F2A. No arguments, no return value, no locals -- confirmed
; from 1F2A's own body (mask ROM 0x1F2A -> 0x170A), which is tiny: if
; neither RAM 0xFED9.7 nor sfr[0xFF02].1 is already set, it force-sets
; EXIC0's IF bit (sfr[0xC8].7) -- a "poke the panel-link interrupt if
; it looks stuck" recovery step, not a data-transfer call.
;
; Evidence: [RE against mask ROM]. Found while tracing the
; mask ROM's OWN reset_handler (0x0040): immediately after EI, before
; validating the app signature or dispatching to application code, the
; real reset handler calls a panel HANDSHAKE routine (sub_0159, not
; itself a stable 0x1Fxx entry so not shimmed here) that sends a sync
; byte (0xB0, via this same 1F2D path) and waits for the panel to echo
; 0xB1 back into RAM 0xFE26, retrying via this exact 1F2A call on
; timeout. Every stock radio performs this handshake before the panel
; will respond to ordinary display-text commands; this project's own
; code never has, on any build through BETA_14 -- see panel.c's
; panel_handshake() (new) for the replicated sequence, built only from
; already-confirmed stable primitives (1F2D here, this call, and the
; documented 0xFEDA.2/0xFE26 RAM protocol), never a raw call to 0x0159
; itself.
; ---------------------------------------------------------------------
	.global	_panel_rx_recover
_panel_rx_recover:
	call	!trampoline_panel_rx_recover
	ret
