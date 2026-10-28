/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* BU4094 output expander + M62363 DAC control (hardware doc section G,
 * "4094/DAC control" building block).
 *
 * BU4094: read-modify-write the RAM shadow byte (0xFE28) then commit
 * the WHOLE byte via bu4094_commit() (resident_api.h) -- never a
 * partial update, per the hardware doc's explicit warning. The
 * bit-set/clear helpers below enforce that by construction (read
 * shadow, modify one bit, commit -- always the full byte).
 *
 * DAC: dac_write() (resident_api.h) writes one channel directly.
 * NEVER invent a calibration value -- always source it from AT24C02
 * EEPROM (see eeprom.h) first. dac_write_calibrated() below is the
 * one entry point that actually does this end-to-end, matching the
 * hardware doc's explicit warning; a bare dac_write() call for an
 * arbitrary literal value is still exposed for the confirmed
 * mute-style channel-1 on/off cases (hardware doc: ch1 only ever seen
 * written with literal 0x00/0xFF), but callers should prefer the
 * calibrated path for channels 2/3/4/6/7 (the confirmed
 * EEPROM-sourced channels). */
#ifndef OUTEXP_H
#define OUTEXP_H

#include "hardware.h"

/* Set the given BU4094_BIT_* bits in the shadow and commit. */
void outexp_set(u8 bits);

/* Clear the given BU4094_BIT_* bits in the shadow and commit. */
void outexp_clear(u8 bits);

/* Replace the whole shadow byte and commit (same as bu4094_write(),
 * exposed here too for symmetry with outexp_set/outexp_clear). */
void outexp_write(u8 value);

/* Read the current shadow byte WITHOUT touching hardware (the shadow
 * is authoritative RAM state -- reading BU4094_SHADOW directly from
 * hardware.h is equally correct; this exists for callers that prefer
 * not to reach into hardware.h directly). */
u8 outexp_read(void);

/* Read one calibration byte from EEPROM at `eeprom_addr` and write it
 * to DAC channel `channel`, per the hardware doc's explicit "never
 * invent a DAC value, always source from EEPROM" rule. */
void dac_write_calibrated(u8 channel, u8 eeprom_addr);

#endif
