/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* debug_serial.h -- bench-only polled bit-bang serial debug channel.
 *
 * Added after BETA_04 (the bounded panel_send_byte fix) came back from
 * its first real hardware test still completely dark: with no display
 * output and no serial, "still frozen somewhere" and "running fine but
 * every panel write is silently giving up" are indistinguishable from
 * outside the radio. This gives a real, independent observability
 * channel so the next bench test produces actual evidence instead of
 * another inconclusive "nothing happened."
 *
 * Reuses P15 (sfr[0xFF01] bit 5) + Timer1 exactly as stage1.s already
 * proved on real hardware (confirmed 9600 8N1 bit-bang TX, same reload
 * value 0x0067) -- purely polled, no ISR, no vector-table dependency,
 * so it coexists safely with the still-unconfirmed 1F33 question. Grep
 * confirms neither pin nor Timer1 is used anywhere else in phase1 or
 * selftest, so there is no conflict with existing code. Call dbg_init()
 * once, then dbg_puts()/dbg_putc() anywhere after.
 */
#ifndef DEBUG_SERIAL_H
#define DEBUG_SERIAL_H

typedef unsigned char u8;

void dbg_init(void);
void dbg_putc(u8 c);
void dbg_puts(const char *s);

/* BETA_29 (panelprobe): busy-wait ~1.3 real seconds, watchdog-serviced
 * throughout -- see debug_serial_asm.s for the exact derivation. Added
 * for panel_test_icon_sweep()'s automatic, self-paced candidate sweep;
 * harmless to declare here for every image sharing this header, since
 * only panelprobe currently calls it. */
void dbg_long_delay(void);
/* phase1 only (its debug_serial.c): stack use and labelled lines. */
void dbg_report_stack(u8 only_if_higher);
void dbg_show(const char *s);   /* "DISPLAY: <text>" */
void dbg_line(const char *label, const char *text);   /* phase1 */
void dbg_hex_line(const char *label, u8 v);              /* phase1 */

#endif
