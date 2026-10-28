/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* Watchdog service (hardware doc section B).
 *
 * The actual kick primitive (wdt_service(), resident_api.h) MUST be a
 * hand-written asm shim -- WDM is a protected register with a special
 * instruction encoding a generic SFR8() write cannot produce, see
 * hardware.h and resident_api.h for why. This module is the thin
 * C-level policy wrapper around that primitive: call wdt_init() once
 * at startup (crt0.s does this, early, before any resident init calls
 * that might take a while), then wdt_poll() at least once per main-
 * loop iteration.
 *
 * Timeout is one of 4 program-selectable intervals, 5.5-349.3ms
 * (datasheet); the exact bit encoding of which bits of the confirmed
 * kick value (0x86) select which interval is not documented in
 * available sources, so this module does not attempt to choose or
 * report an interval -- it only reuses the one value both the mask ROM
 * and the original application always use. */
#ifndef WATCHDOG_H
#define WATCHDOG_H

/* Kick the watchdog once. Call this as early as possible in crt0, and
 * at least once per main-loop iteration thereafter -- anywhere the
 * gap since the last kick could plausibly approach the shortest
 * documented interval (5.5ms) risks a reset. */
void wdt_poll(void);

#endif
