/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "pll.h"
#include "watchdog.h"

#define TMC1  SFR8(0xFF82)    /* timer 1 control: 80h runs it, 00h stops it */
#define MD1   SFR16(0xFF8E)   /* timer 1 modulus, 1 us per count */
#define TMIC1 SFR8(0xFFD0)    /* timer 1 interrupt control */
#define TMIC1_FLAG 0x80       /* bit 7: the count has been reached */

/* One bit: DATA (P02) = bit, then a CLOCK pulse on P01 (stock 9C02h:
 * MOV1 P0.2,CY / NOP / SET1 P0.1 / NOP / CLR1 P0.1). */
static void pll_clock_bit(u8 bit)
{
    if (bit)
        PLL_P0 |= PLL_DATA_BIT;
    else
        PLL_P0 &= ~PLL_DATA_BIT;
    PLL_P0 |= PLL_CLOCK_BIT;
    PLL_P0 &= ~PLL_CLOCK_BIT;
}

/* `nbits` bits of `value`, MSB first. ENABLE is the caller's. */
static void pll_shift(u16 value, u8 nbits)
{
    u8 i;
    for (i = 0; i < nbits; i++) {
        u8 bit = (u8)((value >> (nbits - 1 - i)) & 1);
        pll_clock_bit(bit);
    }
}

void pll_write_c(u8 value)
{
    PLL_P0 &= ~PLL_ENABLE_BIT;
    pll_shift(value, 8);
    PLL_P0 |= PLL_ENABLE_BIT;
}

void pll_write_r(u16 value)
{
    PLL_P0 &= ~PLL_ENABLE_BIT;
    pll_shift(value, 16);
    PLL_P0 |= PLL_ENABLE_BIT;
}

void pll_write_d(u8 d_high, u16 d_low)
{
    /* byte 2: bits 16-14 of D, plus the fixed bits 20-21 */
    u8 top = (u8)((d_high << 2) | (d_low >> 14) | PLL_NA_FIXED_BITS);
    /* byte 1: D >> 6 (bits 13-6 of D); byte 0: D AND 3Fh */
    u16 rest = (u16)(((d_low << 2) & 0xFF00) | (d_low & 0x3F));

    PLL_P0 &= ~PLL_ENABLE_BIT;
    pll_shift(top, 8);
    pll_shift(rest, 16);
    PLL_P0 |= PLL_ENABLE_BIT;
}

/* The mask ROM's 10 ms wait (1138h), with our count and a watchdog kick. */
void pll_wait_tm1(u16 us)
{
    u8 ic;

    TMC1 = 0x00;
    MD1 = us;
    ic = TMIC1;
    ic &= ~TMIC1_FLAG;
    TMIC1 = ic;
    TMC1 = 0x80;
    for (;;) {
        if (TMIC1 & TMIC1_FLAG) break;
        wdt_poll();
    }
    TMC1 = 0x00;
    ic = TMIC1;
    ic &= ~TMIC1_FLAG;
    TMIC1 = ic;
}

u8 pll_locked(void)
{
    return PLL_LOCK_PORT & PLL_LOCK_BIT;
}
