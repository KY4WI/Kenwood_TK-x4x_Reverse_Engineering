/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* Model/variant detection (hardware doc section 15.2).
 *
 * The doc gives the ADC-threshold decision tree 1F15 computes
 * internally (AN3 low/high selects TK-840 vs TK-940/941 family, AN2
 * then thresholds within that family into M890B1/B2/B3 or M940B1/
 * M941B1) but does NOT itself state which of the three output RAM bits
 * (0xFED3.4, 0xFEDA.0, 0xFEDF.4) encodes which branch of that tree.
 *
 * That mapping is now confirmed -- not inferred, not guessed -- by
 * disassembling 1F15's real implementation directly out of
 * TK-840_Mask_ROM.bin (1F15 itself, at the fixed low address, is just a
 * one-instruction `br` trampoline to the real routine at 0x020F; see
 * disasm_c's output for 0x020F-0x0272). Traced instruction-by-instruction:
 *
 *   - AN3 is read first (sfr[0x68]=0x87 selects it); `cmp a,#0x7f` /
 *     `bnl` sets FED3.4 iff AN3>=0x7F ("high"), clears it iff AN3<0x7F
 *     ("low") -- matches doc 15.1's "AN3 low on TK-840".
 *   - AN2 is read second (sfr[0x68]=0x85). If FED3.4==0 (TK-840 family),
 *     execution jumps straight to the doc's 0x55/0xAA three-way TK-840
 *     threshold code; if FED3.4==1 (TK-940/941 family), it falls into
 *     the doc's 0x7F two-way TK-940/941 threshold code instead --
 *     confirming which family branch each FED3.4 value selects.
 *   - Within each family branch, the actual `set1`/`clr1` instructions
 *     on FEDA.0/FEDF.4 give the exact bit pattern for every named
 *     model, transcribed in model_id_t below.
 *
 * Full confirmed truth table (FED3.4 / FEDA.0 / FEDF.4):
 *
 *   0 / 1 / 0  ->  M890B3   (TK-840, AN2 < 0x55)
 *   0 / 0 / 1  ->  M890B2   (TK-840, 0x55 <= AN2 < 0xAA)
 *   0 / 0 / 0  ->  M890B1   (TK-840, AN2 >= 0xAA)
 *   1 / 0 / 0  ->  M941B1   (TK-940/941, AN2 < 0x7F)
 *   1 / 1 / 0  ->  M940B1   (TK-940/941, AN2 >= 0x7F)
 *   0 / 1 / 1  ->  never produced by the decode tree -- doc's own note:
 *                  "should be treated as unsupported hardware rather
 *                  than a sixth known radio". Also covers any other
 *                  combination reaching this firmware by some path 1F15
 *                  doesn't take (RAM corruption, a variant this decode
 *                  tree doesn't know about, etc).
 *
 * model_identify() decodes this and ALSO latches a permanent TX-inhibit
 * the moment it ever sees the invalid combination -- see its own
 * comment for why "permanent" and why the latch, not just a per-call
 * return value, matters here. */
#ifndef MODEL_H
#define MODEL_H

#include "hardware.h"

typedef enum {
    MODEL_UNKNOWN = 0,   /* invalid/unsupported flag combination */
    MODEL_TK840_M890B1,
    MODEL_TK840_M890B2,
    MODEL_TK840_M890B3,
    MODEL_TK940_M940B1,
    MODEL_TK941_M941B1
} model_id_t;

/* Runs model_detect() (resident 1F15) and decodes the three raw RAM
 * flag bits into a model_id_t per the confirmed truth table above.
 * Safe to call more than once -- 1F15 re-reads the ADC every time, it
 * does not latch a one-shot result -- but see model_tx_permitted()'s
 * comment: once MODEL_UNKNOWN is seen, TX stays inhibited for the rest
 * of this boot regardless of what a later call returns. */
model_id_t model_identify(void);

/* True if model_identify() has ever returned MODEL_UNKNOWN so far this
 * boot. Latched, not a live re-check: on real hardware AN2/AN3 are
 * hardwired resistor straps, so a genuine reading never changes between
 * calls -- if it ever comes back unknown, that reflects a real problem
 * (unpopulated/damaged strap, a variant this decode tree doesn't cover,
 * RAM corruption) that a later "lucky" valid-looking read should NOT be
 * trusted to have resolved. TX must stay inhibited for the rest of this
 * boot, not just for the one call that detected it -- hence a latch
 * rather than re-deriving the answer from the current flags every time
 * a caller asks. */
u8 model_tx_permitted(void);

/* Legacy accessor, kept for callers that only want the raw undecoded
 * bits (bit0=FED3.4, bit1=FEDA.0, bit2=FEDF.4; 1=set). Does NOT affect
 * or consult the model_tx_permitted() latch. */
u8 model_read_raw_flags(void);

/* Short (<=8 char) display name for a model_id_t, for the panel's
 * confirmed 8-character text field (panel_display_text()). Returns a
 * pointer to a string literal (flash, .data) -- never allocates or
 * writes RAM. Names are this project's own compact labels, not
 * anything read off real hardware; there is no confirmed on-radio
 * "model name string" this project has found. */
const char *model_name(model_id_t id);

#endif
