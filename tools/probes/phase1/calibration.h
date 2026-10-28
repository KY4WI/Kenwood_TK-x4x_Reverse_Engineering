/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* Calibration preservation (Phase 2 step 2; hardware doc section 9.4).
 *
 * Doc policy: dump/back up EEPROM before any write experiments, keep
 * ordinary firmware read-mostly, and -- the part this module actually
 * does -- never invent a DAC calibration value; always source it from
 * AT24C02 EEPROM. This module applies EXACTLY the confirmed calibration
 * fields (hardware doc section 9.4.1's "operational path" table) and
 * nothing else:
 *
 *   0xEC -> DAC channel 6   confirmed
 *   0xF4 -> DAC channel 2   confirmed
 *   0xF6 -> DAC channel 3   confirmed
 *
 * DAC channels 4 and 7 are deliberately NOT initialized here -- the doc
 * is explicit that their exact EEPROM offset (0xF1 vs 0xF2 for channel
 * 4; 0xEE vs 0xEF for channel 7) is "not fully traced", i.e. genuinely
 * unconfirmed, not just undocumented. Writing either offset would be
 * guessing which of two real EEPROM bytes is the right one -- exactly
 * the kind of fabricated hardware contract this project avoids. Add
 * those two channels here only once the doc's own [OPEN] item is
 * resolved (bench-confirm the selection condition), never by picking
 * one offset and hoping.
 *
 * This module does NOT write to EEPROM (doc policy: read-mostly). It
 * also does not hold a RAM copy of calibration data between calls --
 * every application re-reads the relevant EEPROM byte fresh, which
 * costs a little time but needs no RAM budget (see hardware.h's
 * RAM-constraint comment) and can never go stale. */
#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "hardware.h"

/* Read the confirmed calibration bytes (0xEC/0xF4/0xF6) and write them
 * to DAC channels 6/2/3 respectively, via outexp.c's
 * dac_write_calibrated(). Safe to call more than once (e.g. after an
 * EEPROM write experiment on some OTHER, unrelated address) -- it does
 * not cache anything, so a repeat call just re-applies the current
 * EEPROM contents. Call once at startup after the output-expander is
 * in its known-safe state.
 *
 * First checks eeprom_is_blank() -- a never-formatted EEPROM has no
 * real calibration data at these offsets (they would read as 0xFF,
 * not a genuine calibration value), so applying them would be exactly
 * the "invent a DAC value" mistake the hardware doc warns against, just
 * with an extra step. If the EEPROM is blank, this leaves the DAC
 * channels untouched and returns 0; otherwise applies all three
 * confirmed channels and returns 1. */
u8 calibration_apply_confirmed(void);

#endif
