/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
#include "model.h"
#include "resident_api.h"

/* No explicit initializer (relying on C's implicit static zero-init):
 * that's what puts this in vbcc78k3's .bss (gen_var_head()'s v->clist
 * check, machine.c) instead of .data. This matters here in a way it
 * wouldn't on a typical hosted target: phase1.ld currently has no
 * initialized-RAM-from-flash copy step, and this MUST live in actual
 * RAM (0xFExx), not flash -- a mutable global placed in flash (as an
 * explicit "= 0" initializer would do, landing in .data) would compile
 * and link cleanly but silently do nothing at runtime, since writing
 * to a flash address with a plain MOV doesn't program the flash (see
 * doc section 10). See phase1.ld's ramdata_region/`.bss` comment for
 * the RAM placement and the NOLOAD linker-script mechanics that make
 * relying on this zero-init actually correct: 1F0C already clears
 * 0xFE20-0xFEEF at boot (crt0.s, before main() runs), so an
 * (uninitialized-in-the-image, NOLOAD) global here really does start
 * at zero on real hardware, with no separate runtime clear needed. */
static u8 g_tx_inhibited;

u8 model_read_raw_flags(void)
{
    u8 flags;

    model_detect();

    flags = 0;
    if (RAM8(MODEL_FAMILY_FLAG_ADDR) & MODEL_FAMILY_FLAG_BIT) flags |= 0x01;
    if (RAM8(MODEL_SPLIT_FLAG1_ADDR) & MODEL_SPLIT_FLAG1_BIT) flags |= 0x02;
    if (RAM8(MODEL_SPLIT_FLAG2_ADDR) & MODEL_SPLIT_FLAG2_BIT) flags |= 0x04;
    return flags;
}

model_id_t model_identify(void)
{
    u8 flags = model_read_raw_flags();
    model_id_t id;

    /* flags: bit0=FED3.4 (family), bit1=FEDA.0, bit2=FEDF.4 -- see
     * model.h's confirmed truth table. */
    switch (flags) {
    case 0x02: id = MODEL_TK840_M890B3; break;  /* 0/1/0 */
    case 0x04: id = MODEL_TK840_M890B2; break;  /* 0/0/1 */
    case 0x00: id = MODEL_TK840_M890B1; break;  /* 0/0/0 */
    case 0x01: id = MODEL_TK941_M941B1; break;  /* 1/0/0 */
    case 0x03: id = MODEL_TK940_M940B1; break;  /* 1/1/0 */
    default:   id = MODEL_UNKNOWN;      break;  /* includes 0/1/1 and
                                                  * any FED3.4=1 combo
                                                  * with FEDF.4 set,
                                                  * neither of which the
                                                  * decode tree ever
                                                  * produces */
    }

    if (id == MODEL_UNKNOWN)
        g_tx_inhibited = 1;

    return id;
}

u8 model_tx_permitted(void)
{
    return !g_tx_inhibited;
}

const char *model_name(model_id_t id)
{
    switch (id) {
    case MODEL_TK840_M890B1: return "840-B1";
    case MODEL_TK840_M890B2: return "840-B2";
    case MODEL_TK840_M890B3: return "840-B3";
    case MODEL_TK940_M940B1: return "940-B1";
    case MODEL_TK941_M941B1: return "941-B1";
    default:                 return "NO MDL";
    }
}
