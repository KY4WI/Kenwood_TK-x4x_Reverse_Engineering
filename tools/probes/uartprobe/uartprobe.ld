/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* uartprobe.ld -- TK-840 replacement-firmware hardware-UART probe.
 * Same structure/memory map as selftest.ld/phase1.ld/panelprobe.ld --
 * every fixed address below is a real hardware/mask-ROM constraint
 * (entry point, EEPROM callback stub, KENWOOD signature, etc.), not
 * specific to any one application image, so this file is unchanged
 * from the others except in name and comments. See crt0.s for what
 * goes in each named section.
 *
 * RAM placement rule (same as every other image's .ld -- see
 * panelprobe.ld for the full writeup): a mutable global MUST be
 * declared with no initializer (implicit static zero-init) so it lands
 * in .bss -> ramdata_region (RAM) via vlink, never .data (flash) where
 * a plain MOV write would silently do nothing. This image doesn't
 * currently declare any mutable globals at all (uart.c/uartprobe.c are
 * stateless aside from locals), but the region is kept for parity with
 * the other images and in case a later test here needs one.
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
