/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* Receive-only RF bring-up: tune the synthesizer to one receive channel
 * the way the stock TK-840 firmware does, and check for lock. Never
 * asserts KEY (rf_rx_bringup() calls key_deassert() first), so there is
 * no transmit path here.
 *
 * The channel is 450.000 MHz (VCO 404.950 MHz, D = 32396); rf.c has the
 * arithmetic, pll.h the sequence and where it was traced. TK-840 only:
 * the TK-940/941 use other bases and a 17-bit D. */
#ifndef RF_H
#define RF_H

#include "hardware.h"

/* Program C and R, tune to the test channel with the receive VCO
 * selected, wait as the stock code does, and poll P32. Returns 1 if the
 * synthesizer locked within about 30 ms, 0 if not. KEY stays off. */
u8 rf_rx_bringup(void);

#endif
