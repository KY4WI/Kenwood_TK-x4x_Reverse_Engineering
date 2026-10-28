/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* phase1.ld -- TK-840 replacement-firmware Phase 1 linker script.
 *
 * Why this exists instead of stage1.s's simple .org-only approach: the
 * real C-compiled application code (9 modules, measured .text totalling
 * ~2.6KB) does not fit in the 437-byte gap between the fixed entry point
 * ($8080) and the fixed EEPROM-callback stub ($8235) -- stage1.s could
 * hand-tune a single hello-world file to fit that gap; Phase 1's real
 * building blocks cannot. So the flowing application code is placed in
 * its own region AFTER $8235 instead, where there is ~24KB of room
 * before the next fixed address ($E580).
 *
 * Each fixed-address piece gets its own MEMORY region sized to exactly
 * what it needs, same proven technique as calltbl.ld (checked in
 * alongside this file) -- so an overflow (crt0boot's boot code growing
 * past 437 bytes, say) is a hard link error naming the offending file,
 * never silent corruption of the next fixed region.
 *
 * Section names (vectbl/callttbl/crt0boot/eepromcb/kwsig/padbyte) are
 * emitted by crt0.s (via ".section NAME,\"a\""); see that file for what
 * goes in each. Every compiled C module's .text lands in the "apptext"
 * region via the ordinary *(.text) wildcard.
 *
 * RAM placement (Phase 2 addition): vbcc78k3 (machine.c's gen_var_head())
 * emits a static-storage global with an explicit initializer (even
 * "= 0") to .data, and one with NO initializer (plain "static u8 x;",
 * relying on C's implicit static zero-init) to .bss via ".space", never
 * a literal byte. .data stays mapped into apptext (flash) below --
 * confirmed that's not just mutable globals, but also every string
 * literal (e.g. phase1.c's panel_display_text("PHASE 1 ") argument,
 * compiled as an anonymous initialized char array): those are
 * genuinely read-only at runtime, so flash is the correct, safe place
 * for them, same as .text.
 *
 * THE ACTUAL RULE, enforced by review/comment rather than the linker
 * (vbcc78k3 has no way to flag "initialized AND written-to-at-runtime"
 * for us): never declare a global that is both (a) written to at
 * runtime and (b) given an explicit initializer -- that combination
 * compiles cleanly into a write that silently does nothing on real
 * hardware, because a plain MOV to a flash address doesn't program
 * flash (doc section 10), and this linker script has no flash-to-RAM
 * copy-on-startup step for .data. A mutable global MUST be declared
 * with no initializer (relying on implicit static zero-init) so it
 * lands in .bss -> ramdata_region (RAM) below instead. This is exactly
 * the mistake this comment exists to prevent: model.c's g_tx_inhibited
 * was originally written as "= 0" (an explicit initializer) and landed
 * in .data -> flash, where every runtime write to it would have
 * silently done nothing.
 *
 * .bss itself gets a real RAM region (ramdata_region, 0xFE40-0xFE5F,
 * 32 bytes) marked (NOLOAD) so vlink allocates addresses for its
 * contents without writing any bytes into the flash image for it --
 * there is nothing to load, and nothing CAN be "loaded" at a RAM
 * address by a flat flash image anyway. Zero-initialization at runtime
 * comes for free from crt0.s's existing call to 1F0C (resident
 * clear_resident_ram), which clears exactly 0xFE20-0xFEEF before
 * main() ever runs -- no separate startup clear loop needed.
 *
 * 0xFE40-0xFE5F was picked deliberately, not arbitrarily: hardware.h's
 * confirmed RAM claims in the "genuine free working RAM" block
 * (0xFE20-0xFE7F, doc section 1.4) are 0xFE28 (BU4094 shadow),
 * 0xFE29-0xFE2B (resident EEPROM call scratch: address/result/data
 * staging, doc section 9.2), and 0xFE2C (panel RX byte) -- so
 * 0xFE40-0xFE5F sits comfortably clear of all of them, with room to
 * grow up to 0xFE7F before encroaching on the doc's "unused register
 * bank, repurposed as scratch" region (0xFE80+) that resident code
 * itself sometimes borrows. Total internal RAM is only 256 bytes
 * (0xFE00-0xFEFF, stack included) -- this region is deliberately small
 * (32 bytes) because that budget is genuinely tight, not because 32 is
 * a natural limit; grow LENGTH here (and re-check for collisions) if a
 * future module needs more.
 */

MEMORY {
  vectbl_region    (rx) : ORIGIN = 0x8000, LENGTH = 0x40
  callttbl_region  (rx) : ORIGIN = 0x8040, LENGTH = 0x40
  crt0boot_region  (rx) : ORIGIN = 0x8080, LENGTH = 0x8235-0x8080
  eepromcb_region  (rx) : ORIGIN = 0x8235, LENGTH = 0x0b
  apptext_region   (rx) : ORIGIN = 0x8240, LENGTH = 0xe580-0x8240
  kwsig_region     (r)  : ORIGIN = 0xe580, LENGTH = 7
  padbyte_region   (r)  : ORIGIN = 0xe5ff, LENGTH = 1
  ramdata_region   (rw) : ORIGIN = 0xfe40, LENGTH = 0x20
}

SECTIONS {
  vectbl    0x8000 : { *(vectbl) }              >vectbl_region
  callttbl  0x8040 : { *(callttbl) }            >callttbl_region
  crt0boot  0x8080 : { *(crt0boot) }            >crt0boot_region
  eepromcb  0x8235 : { *(eepromcb) }            >eepromcb_region
  apptext   0x8240 : { *(.text) *(.data) }      >apptext_region
  kwsig     0xe580 : { *(kwsig) }               >kwsig_region
  padbyte   0xe5ff : { *(padbyte) }             >padbyte_region
  bss       0xfe40 (NOLOAD) : { *(.bss) }       >ramdata_region
}
