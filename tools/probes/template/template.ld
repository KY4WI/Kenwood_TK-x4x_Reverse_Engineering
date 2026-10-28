/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* template.ld -- starting-point linker script for a new TK-840
 * probe. Copied unchanged from panelprobe.ld: every fixed address below
 * is a real hardware/mask-ROM constraint (entry point, EEPROM callback
 * stub, KENWOOD signature, etc.), not something specific to any one
 * application image, so this file does not need editing to start a new
 * probe. See panelprobe.ld for the full derivation and panel.c's BETA_*
 * history for how each address was confirmed.
 *
 * Section names (vectbl/callttbl/crt0boot/eepromcb/kwsig/padbyte) are
 * emitted by crt0.s (via ".section NAME,\"a\""); this template borrows
 * ../panelprobe/crt0.s unchanged (see build.sh), so these must match it
 * exactly. Every compiled C module's .text lands in the "apptext"
 * region via the ordinary *(.text) wildcard.
 *
 * THE ACTUAL RULE TO KNOW BEFORE WRITING YOUR OWN CODE, enforced by
 * review/comment rather than the linker (vbcc78k3 has no way to flag
 * "initialized AND written-to-at-runtime" for us): never declare a
 * global that is both (a) written to at runtime and (b) given an
 * explicit initializer -- that combination compiles cleanly into a
 * write that silently does nothing on real hardware, because a plain
 * MOV to a flash address doesn't program flash (hardware doc section
 * 10), and this linker script has no flash-to-RAM copy-on-startup step
 * for .data. A mutable global MUST be declared with no initializer
 * (relying on implicit static zero-init) so it lands in .bss ->
 * ramdata_region (RAM) below instead.
 *
 * .bss gets a real RAM region (ramdata_region, 0xFE40-0xFE5F, 32 bytes)
 * marked (NOLOAD) so vlink allocates addresses for its contents without
 * writing any bytes into the flash image for it. Zero-initialization at
 * runtime comes for free from crt0.s's existing call to 1F0C (resident
 * clear_resident_ram), which clears 0xFE20-0xFEEF before main() ever
 * runs -- no separate startup clear loop needed. 0xFE40-0xFE5F avoids
 * the fixed resident/mask-ROM claims documented in phase1.ld/hardware.h
 * (BU4094 shadow, resident EEPROM call scratch, panel RX byte).
 *
 * If your own probe needs more than 32 bytes of RAM, widen
 * ramdata_region here -- there is spare RAM above 0xFE5F before the
 * fixed claims phase1.ld documents; check there first so two images'
 * RAM layouts don't collide if you ever run them side by side on the
 * same bench session.
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
