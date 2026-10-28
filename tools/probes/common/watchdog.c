/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "watchdog.h"
#include "resident_api.h"

void wdt_poll(void)
{
    wdt_service();
}
