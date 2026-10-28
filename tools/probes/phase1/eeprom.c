/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "eeprom.h"
#include "resident_api.h"
#include "watchdog.h"

void eeprom_read(u8 addr, void *dest, u8 count)
{
    eeprom_seq_read(addr, dest, count);
}

#ifdef EEPROM_ALLOW_WRITE
void eeprom_write(u8 addr, const void *src, u8 count)
{
    const u8 *p = (const u8 *)src;
    u8 remaining = count;
    u8 a = addr;

    while (remaining) {
        u8 space_in_page = EEPROM_PAGE_SIZE - (a % EEPROM_PAGE_SIZE);
        u8 chunk = (remaining < space_in_page) ? remaining : space_in_page;

        eeprom_page_write(a, p, chunk);

        a += chunk;
        p += chunk;
        remaining -= chunk;
    }
}
#endif

u8 eeprom_verify(u8 addr, const void *expect, u8 count)
{
    const u8 *e = (const u8 *)expect;
    u8 buf[EEPROM_PAGE_SIZE];
    u8 remaining = count;
    u8 a = addr;

    while (remaining) {
        u8 chunk = (remaining < EEPROM_PAGE_SIZE) ? remaining : EEPROM_PAGE_SIZE;
        u8 i;

        eeprom_seq_read(a, buf, chunk);
        for (i = 0; i < chunk; i++) {
            if (buf[i] != e[i]) return 0;
        }

        a += chunk;
        e += chunk;
        remaining -= chunk;
    }
    return 1;
}

void eeprom_dump_all(void *dest)
{
    /* Two 128-byte reads rather than one 256-byte read: the resident
     * routine's byte counter (B register) can't represent 256 in a
     * single u8 count argument, and whether a count of 0 wraps to mean
     * "256" is not confirmed -- splitting cleanly avoids relying on
     * that unconfirmed behavior. */
    u8 *d = (u8 *)dest;
    eeprom_seq_read(0, d, 128);
    eeprom_seq_read(128, d + 128, 128);
}

u8 eeprom_is_blank(void)
{
    return eeprom_byte_read(0xFF) == EEPROM_BLANK_BYTE;
}

u8 eeprom_checksum_all(void)
{
    u8 sum = 0;
    u16 addr;

    /* addr is u16 (not u8) purely so the loop condition can test
     * "< 256" without wrapping at 255->0 -- eeprom_byte_read() itself
     * still only ever sees a u8 address (0-255), matching the AT24C02's
     * own address space. */
    /* BETA_12: wdt_poll() every iteration -- see selftest/eeprom.c's
     * matching comment. The one loop in this codebase that could run
     * long without servicing the watchdog anywhere inside it. */
    for (addr = 0; addr < EEPROM_SIZE; addr++) {
        wdt_poll();
        sum = (u8)(sum + eeprom_byte_read((u8)addr));
    }

    return sum;
}
