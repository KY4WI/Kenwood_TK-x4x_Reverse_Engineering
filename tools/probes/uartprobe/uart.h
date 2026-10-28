/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* =====================================================================
 * TK-840 replacement-firmware: on-chip hardware UART (uartprobe).
 *
 * Direct register access, not a resident_api.s shim -- unlike the panel
 * link (1F2D) or the EEPROM/BU4094 calls, there is no resident mask-ROM
 * trampoline for this peripheral we can call into: the two polled byte
 * primitives this project has evidence for (send at 0x8178, receive at
 * 0x818A, hardware doc section 12.4) live in the EXTERNAL APPLICATION
 * ROM this image replaces, not in the internal mask ROM, so they are
 * not available to us. We reuse their PROVEN logic (same registers, same
 * bit, same clear-before-send/poll-with-watchdog shape), just written as
 * our own C instead of called as someone else's routine at a fixed
 * external address.
 *
 * Register map (hardware doc section 12.4, datasheet Table 1):
 *   SCM  FF50H  serial communication mode
 *   SCC  FF52H  serial communication control
 *   BRG  FF53H  baud rate generator
 *   RXB  FF56H  receive buffer (read-only)
 *   TXB  FF57H  transmit buffer (write-only)
 *   STIC FFDEH  serial transmit interrupt control; bit 7 is the transmit-
 *               complete flag polled here, same bit the external
 *               application's own 0x8178 send primitive polls.
 *
 * SCC=0x04 / BRG=0x4E(78) / SCM=0xCD are the mask ROM's own values,
 * confirmed written unconditionally in reset_handler every boot and
 * independently proven live by the external application's accessory
 * protocol (hardware doc 12.4). uart_init() below rewrites the exact
 * same three values explicitly rather than trusting they survived
 * untouched from reset_handler's own earlier pass -- same "redo known-
 * good init ourselves, don't assume residual state" discipline as
 * crt0.s's sub_sysclk_init/sub_startup_wait/model_detect calls.
 *
 * KNOWN GAP: the actual baud rate these three values produce is NOT
 * confirmed numerically -- the bit-level SCM/SCC/BRG encoding isn't in
 * any datasheet excerpt this project has (hardware doc 12.4 says so
 * explicitly, and a fresh search for the real uPD78312A-series hardware
 * manual came up empty; the two public NEC 78K/III-family manuals that
 * were findable use a visibly different serial-interface register
 * layout, so borrowing their formula would be a guess dressed up as a
 * fact). These are reused because they are PROVEN TO WORK, not because
 * the resulting bit rate is known -- treat the real number as something
 * to confirm empirically on the bench (start a terminal at 9600, the
 * standard default for this hardware's era and the same rate already
 * used elsewhere in this project; adjust from there if it's not clean).
 * =====================================================================*/

#ifndef UART_H
#define UART_H

#ifndef U8_TYPEDEF_DEFINED
#define U8_TYPEDEF_DEFINED
typedef unsigned char u8;
#endif

/* Write the mask ROM's own proven SCC/BRG/SCM sequence (same values,
 * same order: SCC then BRG then SCM, matching reset_handler's own
 * block). Safe to call even though reset_handler already did this once
 * before our crt0 ever ran -- see this file's header. */
void uart_init(void);

/* Send one byte: clear the transmit-complete flag, write TXB (which
 * starts the real transmission), then poll that flag until it sets
 * again, servicing the watchdog every iteration -- byte-for-byte the
 * same shape as the external application's own proven 0x8178 routine
 * (hardware doc 12.4), just written here instead of called there. */
void uart_send_byte(u8 c);

/* Flat loop over uart_send_byte() for a NUL-terminated literal -- no
 * shared loop-pointer state lives across a call the way panel.c's old
 * panel_display_text() bug did; each call's argument is a plain stack
 * value fully consumed before the next character. */
void uart_puts(const char *s);

#endif
