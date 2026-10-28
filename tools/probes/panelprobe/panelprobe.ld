/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* panelprobe.ld -- TK-840 replacement-firmware minimal panel-link probe.
 * Identical structure/rationale to selftest.ld/phase1.ld -- a third,
 * independent application-flash image for the same hardware. Built after
 * 13 betas of debugging our OWN bolted-on bit-bang debug-serial channel
 * and crt0 instrumentation without resolving why panelprobe.c's first dbg_puts
 * never shows a letter: time to regroup and test the ACTUAL goal (panel
 * display + buttons) using ONLY the confirmed resident calls (1F2D send,
 * 1F30/1F33 interrupt-driven receive/shift), with ZERO custom debug
 * serial code anywhere in this image -- see panelprobe/panelprobe.c and
 * crt0.s. If the display shows something here, the panel link itself is
 * proven fine and every open question was in our own diagnostic code; if
 * it doesn't, the problem is upstream of (or in) the panel link itself,
 * independent of all the serial/watchdog theorizing so far. Every fixed
 * address below is a real hardware/mask-ROM constraint (entry point,
 * EEPROM callback stub, KENWOOD signature, etc.), not something specific
 * to any one application image, so this file is unchanged from
 * selftest.ld/phase1.ld except in name and comments.
 *
 * Section names (vectbl/callttbl/crt0boot/eepromcb/kwsig/padbyte) are
 * emitted by crt0.s (via ".section NAME,\"a\""); see that file for what
 * goes in each. Every compiled C module's .text lands in the "apptext"
 * region via the ordinary *(.text) wildcard.
 *
 * RAM placement: vbcc78k3 (machine.c's gen_var_head()) emits a static-
 * storage global with an explicit initializer (even "= 0") to .data, and
 * one with NO initializer (plain "static u8 x;", relying on C's implicit
 * static zero-init) to .bss via ".space", never a literal byte. .data
 * stays mapped into apptext (flash) below -- confirmed that's not just
 * mutable globals, but also every string literal, which is genuinely
 * read-only at runtime, so flash is the correct, safe place for it, same
 * as .text.
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
 * lands in .bss -> ramdata_region (RAM) below instead.
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
 * 0xFE40-0xFE5F was picked deliberately, not arbitrarily (see phase1.ld/
 * hardware.h for the full collision-avoidance reasoning against the
 * BU4094 shadow, resident EEPROM call scratch, and panel RX byte) --
 * same choice reused here since this image never runs alongside
 * phase1.bin (they are alternative images for the same flash socket,
 * never loaded together), so there is no cross-image RAM conflict to
 * worry about, only the same fixed resident/mask-ROM claims phase1.ld
 * already accounts for.
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
