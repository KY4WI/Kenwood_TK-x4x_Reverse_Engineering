/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* pll.h -- the synthesizer (MC145190 family, 12.8 MHz reference), done
 * the way the stock firmware does it. Traced in the TK-840 image 8355
 * (pll_write_na at 9BBCh, pll_tune_rx at 9B23h, start-up at A4E0h) and
 * the TK-940/941 image AB00 (the same writer at 9B53h); see the hardware
 * reference, section 6.
 *
 * Wiring: P00 = ENABLE (low while shifting, high latches), P01 = CLOCK,
 * P02 = DATA, MSB first; P32 = lock detect, 1 = locked. The register is
 * chosen by the number of clocks: C = 8, R = 16, A/N = 24.
 *
 * The divider D = 64 * N + A, and fVCO = 12.5 kHz * D. The A/N word is
 *   byte 2 = (D >> 14) OR 30h, byte 1 = (D >> 6) AND FFh, byte 0 = D AND 3Fh
 * so N starts at bit 8, A is bits 0-5, and bits 20-21 are always set.
 * D has 17 bits: the TK-840 (UHF) keeps it under 65536, the TK-940/941
 * need bit 16 (D is about 72000-76000 there). */
#ifndef PLL_H
#define PLL_H

#include "hardware.h"

/* C register values the stock code sends. [OPEN] what each bit does;
 * 6Dh-then-6Ch looks like a fast-lock setting followed by the normal one. */
#define PLL_C_STARTUP  0x6E   /* once, before the R register */
#define PLL_C_TUNING   0x6D   /* after each new divider, for PLL_TUNE_WAIT_US */
#define PLL_C_RUNNING  0x6C   /* after that wait */

/* R register: low 13 bits = 1024, 12.8 MHz / 1024 = 12.5 kHz steps. */
#define PLL_R_12K5     0x4400

/* Always set in the A/N word's top byte (bits 20-21). */
#define PLL_NA_FIXED_BITS 0x30

/* How long the stock TK-840 code leaves PLL_C_TUNING in place: timer 1
 * with MD1 = 05DCh, at 1 us per count (the mask ROM's 10 ms wait at
 * 1138h uses MD1 = 2710h). */
#define PLL_TUNE_WAIT_US 1500

/* TK-840 divider bases, added to a channel's 15-bit codeplug value
 * (12.5 kHz steps above 300.000 MHz). */
#define PLL_840_TX_BASE 0x5DC0   /* 24000 steps = 300.000 MHz: VCO on the channel */
#define PLL_840_RX_BASE 0x4FAC   /* 300.000 - 45.050 MHz: first LO 45.050 MHz below */

/* Program the C register (8 clocks). */
void pll_write_c(u8 value);

/* Program the R register (16 clocks). */
void pll_write_r(u16 value);

/* Program the A/N divider from D = d_high:d_low (d_high is bit 16 of D,
 * 0 on the TK-840), in the stock layout above. */
void pll_write_d(u8 d_high, u16 d_low);

/* Wait `us` microseconds on timer 1, polling its interrupt flag (the
 * interrupt itself stays masked) and kicking the watchdog. Stops timer 1
 * afterwards. */
void pll_wait_tm1(u16 us);

/* Nonzero if P32 says the synthesizer is locked. */
u8 pll_locked(void);

#endif
