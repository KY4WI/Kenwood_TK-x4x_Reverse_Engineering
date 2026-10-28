/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "calibration.h"
#include "outexp.h"
#include "eeprom.h"

u8 calibration_apply_confirmed(void)
{
    if (eeprom_is_blank())
        return 0;

    /* hardware.h's DAC_CH_* constants already carry their confirmed
     * EEPROM source address in their own comment; using them here
     * (rather than re-deriving 0xEC/0xF4/0xF6 by hand) keeps this the
     * single place that would need to change if a channel mapping were
     * ever corrected. */
    dac_write_calibrated(DAC_CH_VCXO_OR_TX, 0xF4);
    dac_write_calibrated(DAC_CH_UNKNOWN3,   0xF6);
    dac_write_calibrated(DAC_CH_UNKNOWN6,   0xEC);

    /* Channels 4 and 7 deliberately not applied -- see calibration.h. */
    return 1;
}
