/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* =====================================================================
 * TK-840 replacement-firmware: on-chip hardware UART (baudsweep).
 *
 * Direct register access, not a resident_api.s shim -- unlike the panel
 * link (1F2D) or the EEPROM/BU4094 calls, there is no resident mask-ROM
 * trampoline for this peripheral we can call into: the two polled byte
 * primitives this project has evidence for (send at 0x8178, receive at
 * 0x818A, hardware doc section 12.4) live in the EXTERNAL APPLICATION
 * ROM this image replaces, not in the internal mask ROM, so they are
 * not available to us. We reuse their PROVEN logic (same registers,
 * same bit, same clear-before-send/poll-with-watchdog shape), just
 * written as our own C instead of called as someone else's routine at
 * a fixed external address.
 *
 * Register map (hardware doc section 12.4; bit layouts below are now
 * confirmed from NEC's actual uPD78310A(A)/78312A(A) User's Manual
 * (IEU-1265D), section 4.4 "Serial Communication Interface" -- the
 * uartprobe image's header still carried the "not confirmed" hedge
 * because only the short-form data sheet was available when it was
 * written; that hedge is now resolved and removed):
 *
 *   SCM  FF50H  serial communication mode. Our proven value 0xCD
 *               decodes (Fig. 4-47, asynchronous mode layout) to:
 *                 bit 7 TXRDY = 1  transmission enabled (CTS low --
 *                                  P27/CTS is hardwired to ground on
 *                                  this board, hardware doc section 4,
 *                                  so this is unconditionally true)
 *                 bit 6 RXE   = 1  reception enabled
 *                 bit 5 PEN   = 0  no parity
 *                 bit 3 CL    = 1  8-bit characters
 *                 bit 2 SL    = 1  TWO stop bits (8N2, not 8N1)
 *                 bits 1,0 MD1,MD0 = 0,1  asynchronous mode
 *               This image never changes SCM -- only the bit rate
 *               varies across the sweep, framing stays fixed 8N2.
 *
 *   SCC  FF52H  serial communication control. Bits 2-0 (PRS2-PRS0,
 *               Fig. 4-49) select which Time Base Counter tap feeds
 *               the baud rate generator -- this is the "n" the short
 *               data sheet's Table 5-3 used without saying where it
 *               lived. PRS2,PRS1,PRS0 read directly as the 3-bit value
 *               of n (0-7).
 *
 *   BRG  FF53H  baud rate generator -- an 8-bit divisor "G" (1-255),
 *               nothing more; n lives in SCC, not here.
 *
 *   RXB  FF56H  receive buffer (read-only)
 *   TXB  FF57H  transmit buffer (write-only)
 *   STIC FFDEH  serial transmit interrupt control; bit 7 is STF, the
 *               transmit-complete flag polled here -- confirmed by
 *               name against the User's Manual's own SEIC/SRIC/STIC
 *               figure (4-55), same bit the external application's own
 *               0x8178 send primitive polls.
 *
 * THE FORMULA (User's Manual section 4.4.3, matches every row of its
 * own reference table to within the table's own stated rounding error):
 *
 *     Baud = fCLK / (2^(n+2) x G)          fCLK = 6 MHz (confirmed)
 *
 * This resolves the previously-open question definitively: uartprobe's
 * SCC=0x04 (n=4, PRS2..PRS0=100) with BRG=0x4E (G=78) computes to
 * 6,000,000 / (64 x 78) = 1201.9 baud -- i.e. uartprobe actually ran at
 * 1200 baud, not 9600 as originally guessed, and the tester has since
 * confirmed clean reception at 1200 on the real bench unit. The n/G
 * pairs this image cycles through are below (all ~0.16% error, same
 * order of accuracy NEC's own table uses throughout):
 *
 *   1200   n=4 (SCC=0x04)  G=78  (BRG=0x4E)  -- bench-proven, kept as-is
 *   9600   n=0 (SCC=0x00)  G=156 (BRG=0x9C)
 *   19200  n=0 (SCC=0x00)  G=78  (BRG=0x4E)  -- same BRG as 1200, just SCC changed!
 *   38400  n=0 (SCC=0x00)  G=39  (BRG=0x27)
 *   57600  n=0 (SCC=0x00)  G=26  (BRG=0x1A)
 *   115200 n=0 (SCC=0x00)  G=13  (BRG=0x0D)
 *
 * WHY THERE ARE TWO ENTRY POINTS (uart_init / uart_set_rate), NOT ONE --
 * the manual's warning below. It was first thought to cause the "garbage
 * at every rate" result; that was really the vbcc string bug (see
 * baudsweep.c). The split stays, because the warning is real:
 *
 * The NEC User's Manual (IEU-1265D), chapter 12.1 ("Precautions
 * regarding serial communication interface function"), has an errata
 * note that is the whole reason this header looks the way it does. In
 * short: while sending in asynchronous mode, do not change the CTS pin
 * level or the TXRDY bit (bit 7 of the serial mode register). A CTS
 * low-to-high change, or setting TXRDY, can raise an extra "transmit
 * complete" interrupt; the transmit buffer (TxB) is then written twice
 * and a byte is lost.
 *
 * SCM bit 7 IS TXRDY. The first version of this image called a single
 * uart_init(scc,brg) on every rate change, which rewrote SCM=0xCD --
 * TXRDY=1 over an already-1 TXRDY -- every single time, in the middle
 * of live communication (reception was also continuously enabled via
 * RXE, same register). That is exactly the hazard this note describes.
 * uartprobe never hit this: it wrote SCM exactly once, at boot, before
 * any byte had ever been sent -- "during serial communication" didn't
 * apply yet. The note doesn't call out SCC or BRG at all -- the
 * documented hazard is specifically the TXRDY bit (and the CTS level,
 * which is hardwired low on this board and can't change anyway) -- so
 * the fix is to set SCM (TXRDY/RXE/framing) exactly ONCE, before the
 * first byte is ever sent, and on every later rate change touch ONLY
 * SCC/BRG, never SCM again:
 *
 *   uart_init(scc, brg)      -- call ONCE, before any transmission:
 *                                sets SCC, BRG, AND SCM (establishes
 *                                TXRDY/RXE the one time it's safe to).
 *   uart_set_rate(scc, brg)  -- call on every later rate change: SCC
 *                                and BRG only, SCM untouched.
 * =====================================================================*/

#ifndef UART_H
#define UART_H

#ifndef U8_TYPEDEF_DEFINED
#define U8_TYPEDEF_DEFINED
typedef unsigned char u8;
#endif

/* One-time setup: write SCC/BRG/SCM in the mask ROM's own proven order
 * (SCC, then BRG, then SCM last -- SCM carries the TXRDY/RXE enable
 * bits, so writing it last means the interface only goes live once the
 * clock is already set up). scc_val/brg_val select the bit rate per
 * this file's header table; SCM is always written as the fixed,
 * already-proven 0xCD (asynchronous, 8N2, no parity).
 *
 * Call this EXACTLY ONCE, before the first byte is ever sent or
 * received -- see this file's header for why: writing SCM again later,
 * mid-communication, is the documented hazard (User's Manual 12.1) that
 * caused this image's first version to produce garbage at every rate.
 * For every later rate change, call uart_set_rate() instead. */
void uart_init(u8 scc_val, u8 brg_val);

/* Rate change after uart_init() has already run once: SCC and BRG only
 * -- deliberately does NOT touch SCM, so TXRDY/RXE are never rewritten
 * after their one safe, pre-communication write. Safe to call any
 * number of times, including mid-sweep, mid-run, forever. */
void uart_set_rate(u8 scc_val, u8 brg_val);

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
