/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "ptt.h"
#include "outexp.h"
#include "pll.h"
#include "model.h"

void key_deassert(void)
{
    KEY_PORT = KEY_PORT & (u8)~KEY_BIT;
}

u8 key_is_asserted(void)
{
    return KEY_PORT & KEY_BIT;
}

u8 key_assert_safe(u8 tx, u16 max_polls)
{
    u16 i;

    /* Unknown/unsupported model permanently inhibits TX (hardware doc
     * Phase 2 rule) -- checked first, before touching VCO select or
     * anything else, so an inhibited unit never even reaches the PLL
     * lock wait. Only gates the TX VCO-select path (`tx` nonzero);
     * key_assert_safe(0, ...) for RX is unaffected -- an unknown model
     * should still be able to receive, it just must never transmit. */
    if (tx && !model_tx_permitted()) {
        key_deassert();
        return 0;
    }

    key_deassert();

    if (tx)
        outexp_clear(BU4094_BIT_STR);   /* clear = transmit VCO */
    else
        outexp_set(BU4094_BIT_STR);     /* set = receive VCO */

    for (i = 0; i < max_polls; i++) {
        if (pll_locked()) {
            KEY_PORT = KEY_PORT | KEY_BIT;
            return 1;
        }
    }
    return 0;
}

u8 ptt_ext_requested(void)
{
    return (PTT_EXT_PORT & PTT_EXT_BIT) == 0;
}
