/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "rf.h"
#include "ptt.h"
#include "outexp.h"
#include "pll.h"
#include "watchdog.h"

/* Receive on 450.000 MHz, the way the stock TK-840 code tunes (pll_tune_rx,
 * 9B23h): D = channel + PLL_840_RX_BASE, where the channel is in 12.5 kHz
 * steps above 300.000 MHz: (450.000 - 300.000) / 0.0125 = 12000, and
 * 12000 + 4FACh = 32396 (VCO 404.950 MHz, 45.050 MHz below the channel).
 * D fits in 16 bits on the TK-840, so bit 16 is 0. */
#define RF_TEST_CHANNEL  12000U
#define RF_TEST_D        (RF_TEST_CHANNEL + PLL_840_RX_BASE)

u8 rf_rx_bringup(void)
{
    u16 i;

    /* KEY inactive first, always -- this module never asserts it. */
    key_deassert();

    /* Start-up, as stock A4E0h: C register, then R register. */
    pll_write_c(PLL_C_STARTUP);
    pll_write_r(PLL_R_12K5);

    /* Tune, as stock pll_tune_rx: the divider, then STR set (receive VCO)
     * and committed, then C = 6Dh, 1.5 ms, C = 6Ch. */
    pll_write_d(0, RF_TEST_D);
    outexp_set(BU4094_BIT_STR);
    pll_write_c(PLL_C_TUNING);
    pll_wait_tm1(PLL_TUNE_WAIT_US);
    pll_write_c(PLL_C_RUNNING);

    /* The stock code waits 10 ms, then polls P32 for up to 10 ms more
     * (9C0Dh). Here: 10 ms, then up to 20 x 1 ms. */
    pll_wait_tm1(10000);
    for (i = 0; i < 20; i++) {
        if (pll_locked())
            return 1;
        pll_wait_tm1(1000);
    }
    return 0;
}
