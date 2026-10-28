/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* =====================================================================
 * TK-840 replacement-firmware: resident-ROM API.
 *
 * C-callable wrappers around the mask ROM's resident service routines.
 * Each one is implemented as a hand-written assembly shim in
 * resident_api.s -- see that file for why a shim is necessary (the
 * resident ROM's calling convention and vbcc78k3's C calling convention
 * are genuinely different, confirmed) and for the real
 * evidence behind each function (real call sites in the captured
 * original firmware, cross-checked against the resident implementation
 * itself in the mask ROM disassembly).
 *
 * Only functions with real, cross-checked evidence are declared here.
 * This file grows as more resident routines get the same treatment --
 * see docs/hardware/ (the hardware reference) for the running list of what's still
 * open (DAC channel mapping, panel link, PLL, etc.).
 * ===================================================================== */

#ifndef RESIDENT_API_H
#define RESIDENT_API_H

#ifndef U8_TYPEDEF_DEFINED
#define U8_TYPEDEF_DEFINED
typedef unsigned char u8;
#endif

/* Read `count` bytes from the AT24C02 EEPROM starting at `addr` into
 * `dest`. No confirmed status/error return -- best effort. */
void eeprom_seq_read(u8 addr, void *dest, u8 count);

/* Write `count` bytes from `src` to the AT24C02 EEPROM starting at
 * `addr`. Same caveat as eeprom_seq_read. */
void eeprom_page_write(u8 addr, const void *src, u8 count);

/* Shift the current output-expander shadow byte (RAM 0xFE28) out to the
 * BU4094. Writes the WHOLE expander state -- read-modify-write the
 * shadow yourself (e.g. via SFR8(0xFE28)) before calling this if you
 * only want to change one bit. */
void bu4094_commit(void);

/* Convenience: set the output-expander shadow byte to `value` and
 * commit it in one call. Replaces the whole byte, same as writing
 * SFR8(0xFE28) directly then calling bu4094_commit(). */
void bu4094_write(u8 value);

/* ---------------------------------------------------------------------
 * Added for Phase 1 firmware integration. Same evidence bar
 * and shim-necessity reasoning as above -- see resident_api.s for each
 * one's specific justification.
 * --------------------------------------------------------------------- */

/* Service (kick) the watchdog timer: MOV WDM,#0x00 then MOV WDM,#0x86.
 * MUST be a hand-written shim, never SFR8(0xFF42)=...  from C -- WDM is
 * a protected register (datasheet p.275): "MOV WDM,#byte" is a special
 * instruction form that also writes a one's-complement guard byte the
 * CPU checks in hardware, which the generic SFR8() write path has no
 * way to produce; a naive C write would trigger an OPE code trap
 * instead of servicing the watchdog. Call this early in crt0 and
 * periodically thereafter -- see hardware.h's WDT_KICK_LOW/HIGH. */
void wdt_service(void);

/* Read a single byte from the AT24C02 EEPROM at `addr` (resident 1F1B,
 * "random single-byte read"). Companion to eeprom_seq_read/
 * eeprom_page_write above, for the common single-byte case.
 *
 * Evidence: [RE against mask ROM]. 1F1B's own body (mask ROM) sets
 * 0xFE29=addr, calls the shared EEPROM engine, and leaves the result in
 * both A and RAM 0xFE2A -- confirmed by disassembling 1F1B itself. A is
 * exactly vbcc78k3's own u8 return-value register, so this shim needs
 * no explicit "load into return register" step at all -- the value is
 * already where the caller expects it when 1F1B returns. */
u8 eeprom_byte_read(u8 addr);

/* Run the resident hardware/model-detect routine (1F15). Converts ADC
 * inputs AN2/AN3 into three RAM flag bits -- read them afterward via
 * hardware.h's MODEL_*_FLAG_ADDR/_BIT (a plain RAM8() read, no shim
 * needed for that part). No arguments, no return value; call once at
 * startup before relying on the model flags. */
void model_detect(void);

/* Write `value` to M62363 DAC channel `channel` (1-8). The ORIGINAL
 * firmware reaches 1F24 only via CALLT slot 19 (table address 0x66 ->
 * stock 0x808F -> BR 0x1F24, 21 confirmed real call sites, never a
 * direct "call 0x1f24") -- but that's a 1-byte-vs-3-byte code-size
 * choice at THEIR call sites, not a functional requirement: 1F24 is a
 * fixed resident address either way. This shim calls it directly
 * ("call 0x1f24") rather than reconstructing that CALLT slot, which
 * would add a dependency on crt0.s having populated it correctly first
 * for no functional benefit here.
 *
 * Evidence: [RE against mask ROM + real call sites]. 1F24 takes A=
 * channel, X=value (confirmed from the resident routine's own body and
 * cross-checked against real call-site register setup). NEVER invent a
 * calibration value here -- always source it from AT24C02 EEPROM data
 * per the hardware doc's explicit warning (wrong DAC values can cause
 * frequency error, deviation error, wrong TX power, or bad audio). */
void dac_write(u8 channel, u8 value);

/* Send one byte to the front panel over the bit-bang link (resident
 * 1F2D). NO TIMEOUT if the panel ISR machinery isn't actually running
 * and the previous transfer's busy flag never clears -- confirmed from
 * 1F2D's own body, per the hardware doc's explicit warning. Only safe
 * to call after panel interrupts are actually serviced (EXIC0 unmasked,
 * 1F30/1F33 wired into the vector table) -- see crt0.s. */
void panel_send_byte(u8 b);

/* Resident 1F2A: panel-link recovery poke. No arguments, no return
 * value -- confirmed from 1F2A's own tiny body (mask ROM 0x170A): if
 * neither RAM 0xFED9.7 nor sfr[0xFF02].1 is already set, it force-sets
 * EXIC0's IF bit. Found tracing the mask ROM's own reset_handler,
 * which calls this as part of a panel sync handshake (send 0xB0,
 * await 0xB1) performed right after EI, before every stock boot ever
 * dispatches into application code -- see panel.c's panel_handshake().
 */
void panel_rx_recover(void);

#endif
