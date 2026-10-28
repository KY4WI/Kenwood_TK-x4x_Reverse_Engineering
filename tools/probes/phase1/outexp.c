/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "outexp.h"
#include "resident_api.h"
#include "eeprom.h"

void outexp_set(u8 bits)
{
    BU4094_SHADOW = BU4094_SHADOW | bits;
    bu4094_commit();
}

void outexp_clear(u8 bits)
{
    BU4094_SHADOW = BU4094_SHADOW & (u8)~bits;
    bu4094_commit();
}

void outexp_write(u8 value)
{
    bu4094_write(value);
}

u8 outexp_read(void)
{
    return BU4094_SHADOW;
}

void dac_write_calibrated(u8 channel, u8 eeprom_addr)
{
    u8 value;
    eeprom_read(eeprom_addr, &value, 1);
    dac_write(channel, value);
}
