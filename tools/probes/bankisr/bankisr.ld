/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* bankisr.ld -- TK-840 replacement-firmware register-bank-ISR probe.
 * Same structure/memory map as uartprobe.ld/panelprobe.ld -- every
 * fixed address below is a real hardware/mask-ROM constraint (entry
 * point, EEPROM callback stub, KENWOOD signature, etc.), not specific
 * to this image. See crt0.s for what goes in each named section.
 *
 * RAM placement rule (same as every other image's .ld): a mutable
 * global MUST be declared with no initializer (crt0.s's isr_fires/
 * canary_fails are plain .byte 0 in a .bss section) so it lands in
 * RAM via vlink, never flash, where a plain write would silently do
 * nothing on real hardware.
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
