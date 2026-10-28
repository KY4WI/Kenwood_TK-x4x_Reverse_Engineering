/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* PTT / RF KEY control (hardware doc section H).
 *
 * KEY (P17, SFR8(0xFF01) bit 0x80, active HIGH) is the only thing that
 * actually enables RF. The safe sequence (hardware doc, design rule 2,
 * section 6.5) is: drop KEY -> select VCO state via BU4094 STR ->
 * program PLL -> wait for lock -> apply calibrated DAC values -> THEN
 * assert KEY. key_assert_safe() below enforces the ordering it can
 * (VCO select, lock wait, then KEY) but does NOT itself call the PLL
 * programming or DAC calibration steps -- those need real target-
 * frequency/channel data this module has no opinion on -- so
 * key_assert_safe() is only really usable once a caller
 * has already programmed the synthesizer and applied calibration
 * itself. It still enforces the lock-wait and VCO-select ordering
 * unconditionally, and refuses to assert KEY if the lock wait times
 * out, which is real, callable safety value on its own.
 *
 * SQUELCH IS NOT IMPLEMENTED IN THIS MODULE. The hardware doc confirms
 * the real algorithm is P30/CI0 noise-pulse counting, reached via
 * interrupt vectors rather than a polled register, and is explicitly
 * marked [OPEN] / "next research step" -- not a register this project
 * has decoded. A function here that read some plausible-looking
 * register and called it "squelch status" would be fabricating a
 * hardware contract this project hasn't confirmed, so none is
 * provided. See hardware.h's comment on this same point. */
#ifndef PTT_H
#define PTT_H

#include "hardware.h"

/* Immediately drop KEY (deassert RF). Always safe to call -- this is
 * the "if lock is lost mid-transmit, clear KEY immediately" emergency
 * path from the hardware doc's safety rules, as well as the normal
 * end-of-transmission path. */
void key_deassert(void);

/* True if KEY is currently asserted. */
u8 key_is_asserted(void);

/* Select the TX or RX VCO state via the BU4094 STR bit (doc G: bit 6,
 * "TX/RX VCO selection"), then wait up to `max_polls` iterations of
 * pll_locked() for lock, then assert KEY only if lock was achieved
 * within that budget. Returns 1 if KEY was asserted, 0 if the lock
 * wait timed out (KEY is left deasserted in that case). Does NOT
 * program the PLL or DAC itself -- call those first. `tx` selects the
 * VCO state (nonzero = TX: STR clear; zero = RX: STR set), not KEY directly.
 *
 * When `tx` is nonzero, also checks model_tx_permitted() (model.h)
 * FIRST, before anything else -- an unknown/unsupported hardware
 * variant permanently inhibits TX, returning 0 immediately without
 * touching VCO select or waiting on lock. RX (`tx`==0) is never
 * gated by this -- an unknown model can still receive. */
u8 key_assert_safe(u8 tx, u16 max_polls);

/* True if the external/accessory PTT request line is asserted (P10,
 * active LOW -- hardware doc: exact pull-up/direction bits [OPEN], but
 * the port bit and active-low polarity are confirmed). This is a
 * REQUEST, not KEY itself -- see the file comment. */
u8 ptt_ext_requested(void);

#endif
