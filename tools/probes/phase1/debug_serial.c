/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "debug_serial.h"

void dbg_puts(const char *s)
{
    while (*s) dbg_putc((u8)*s++);
    dbg_putc(0x0d);
    dbg_putc(0x0a);
}

/* Stack measurement. crt0.s fills FE60h-FEBBh with 55h before main() and
 * starts the stack at FEBCh, so the deepest point reached so far is the
 * lowest byte that is no longer 55h. Prints "STACK USED nn OF 92 BYTES";
 * with only_if_higher set, prints only when the figure has gone up. */
#define STACK_LOW    ((volatile u8 *)0xFE60)
#define STACK_BYTES  92u
#define STACK_FILL   0x55u

static u8 stack_high_water;

static void dbg_str(const char *s)
{
    while (*s) dbg_putc((u8)*s++);
}

static void dbg_dec(u8 v)
{
    u8 h = 0, t = 0;
    while (v >= 100) { v -= 100; h++; }
    while (v >= 10) { v -= 10; t++; }
    if (h) dbg_putc((u8)('0' + h));
    if (h || t) dbg_putc((u8)('0' + t));
    dbg_putc((u8)('0' + v));
}

void dbg_report_stack(u8 only_if_higher)
{
    u8 i;
    for (i = 0; i < STACK_BYTES; i++)
        if (STACK_LOW[i] != STACK_FILL) break;
    i = (u8)(STACK_BYTES - i);
    {
        /* Through a local: vbcc78k3 would otherwise emit "cmp a,<static>",
         * which has no 16-bit-address form on the 78K/III. */
        volatile u8 prev = stack_high_water;
        if (only_if_higher && i <= prev) return;
    }
    stack_high_water = i;
    dbg_str("STACK USED ");
    dbg_dec(i);
    dbg_puts(" OF 92 BYTES");
}

/* Mirror a display text to the debug output: "DISPLAY: <text>". */
void dbg_show(const char *s)
{
    dbg_str("DISPLAY: ");
    dbg_puts(s);
}

/* "<label><text>" on one line. */
void dbg_line(const char *label, const char *text)
{
    dbg_str(label);
    dbg_puts(text);
}

/* "<label><two hex digits>" on one line. */
void dbg_hex_line(const char *label, u8 v)
{
    char hex[3];
    u8 n;
    n = (u8)(v >> 4);
    hex[0] = (char)(n < 10 ? '0' + n : 'A' + n - 10);
    n = (u8)(v & 0x0F);
    hex[1] = (char)(n < 10 ? '0' + n : 'A' + n - 10);
    hex[2] = 0;
    dbg_str(label);
    dbg_puts(hex);
}
