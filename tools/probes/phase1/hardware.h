/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* =====================================================================
 * TK-840 replacement-firmware Phase 1: hardware register/bit definitions.
 *
 * Every address/bit here is transcribed directly from the confirmed
 * findings in docs/hardware/ (the hardware reference) (the authoritative
 * hardware doc for this project) -- nothing in this file is invented or
 * guessed. Anything the doc marks [OPEN] (not yet bench/schematic
 * confirmed) is either left out or explicitly commented as open; code
 * elsewhere must not treat an [OPEN] item as a firm hardware contract.
 * =====================================================================*/

#ifndef HARDWARE_H
#define HARDWARE_H

#ifndef U8_TYPEDEF_DEFINED
#define U8_TYPEDEF_DEFINED
typedef unsigned char  u8;
#endif
typedef unsigned short u16;

#define SFR8(addr)  (*(volatile u8  *)(addr))
#define SFR16(addr) (*(volatile u16 *)(addr))
#define RAM8(addr)  (*(volatile u8  *)(addr))

/* ---------------------------------------------------------------------
 * Memory map (doc section 2.3 / A above).
 * ---------------------------------------------------------------------
 *   0000-1FFF  internal mask ROM (off-limits, resident code)
 *   8000-E5FF  external application flash (THIS FIRMWARE)
 *   E600-FDFF  external codeplug/configuration flash (off-limits)
 *   FE00-FE1F  stack (32 bytes)
 *   FE20-FEEF  free RAM + repurposed register-bank scratch (see below)
 *   FEF0-FEFF  active register bank 0 (never touch via SEL RBn)
 *   FF00-FFFF  SFR space
 * FE20-FF1F is the compiler's own "saddr window" (machine.c's
 * SADDR_WINDOW_LO/HI) -- ordinary RAM8()/SFR8() accesses to addresses
 * in this file that fall in that window already get the compiler's
 * native saddr/SET1/CLR1/BT/BF addressing for free, no extra work
 * needed here.
 *
 * CONFIRMED HARD CONSTRAINT (doc section 1.4, datasheet-confirmed, not
 * inferred): total internal RAM is 256 bytes, FULL STOP -- 32 bytes of
 * stack (FE00-FE1F) + 208 bytes of free/scratch RAM (FE20-FEEF) + 16
 * bytes of live register bank (FEF0-FEFF). There is no way to hold a
 * 256-byte buffer (e.g. a full AT24C02 dump) ANYWHERE in RAM at once --
 * that alone would consume the entire budget, stack included. Doc
 * section 9.4's "dump all 256 EEPROM bytes / save with identity/date/
 * checksum" calibration-backup policy is therefore necessarily a
 * bench/host-side procedure (captured byte-by-byte over some
 * out-of-band channel, e.g. the panel link or a debug UART), not
 * something this firmware can do by buffering the whole EEPROM image
 * itself -- see eeprom.h's eeprom_checksum_all() for what this
 * firmware CAN honestly do towards that policy (a streaming,
 * one-byte-at-a-time read of the complete 256 bytes) without ever
 * allocating more than a couple of bytes of RAM for it.
 *
 * Compiler-managed static/global storage: vbcc78k3 only ever emits a
 * mutable global to .bss (see phase1.ld's own comment for the
 * .data-vs-.bss split and why a mutable global MUST have no explicit
 * initializer to land there). phase1.ld maps .bss to ramdata_region,
 * 0xFE40-0xFE5F (32 bytes), chosen to sit clear of every RAM address
 * already claimed below (0xFE28 BU4094 shadow, 0xFE29-0xFE2B resident
 * EEPROM call scratch, 0xFE2C panel RX byte) with headroom to grow
 * before 0xFE60, where the stack ends. Since 2026-10-16 crt0.s moves
 * the stack to FEBCh, growing down to FE60h (92 bytes), after the
 * start-up calls; at FE20h it had only 30 bytes and the C code
 * overflowed it. Keep .bss below FE60h.
 * --------------------------------------------------------------------- */
#define FLASH_BASE      0x8000UL
#define FLASH_END       0xE5FFUL   /* inclusive */
#define APP_SIGNATURE_ADDR 0xE580UL /* required "KENWOOD", doc J.13 */

/* ---------------------------------------------------------------------
 * Watchdog (doc B). WDM (0xFF42) is a PROTECTED register -- "MOV WDM,
 * #byte" is a special instruction form requiring a one's-complement
 * guard byte the CPU checks in hardware (datasheet p.275; confirmed
 * correctly encoded by this project's vasm cpu.c, 82/82 match against
 * real mask-ROM instances). SFR8(0xFF42)=... from C would NOT produce
 * this special form -- it would silently generate a plain SFR write
 * that the CPU's guard check REJECTS (OPE code trap), so servicing the
 * watchdog is implemented as a hand-written asm shim, wdt_service(),
 * in resident_api.s -- never attempt it via SFR8() from C.
 * --------------------------------------------------------------------- */
#define WDT_KICK_LOW   0x00
#define WDT_KICK_HIGH  0x86   /* the one value both ROMs always use */

/* ---------------------------------------------------------------------
 * PLL bit-bang (doc F). Port P0 = SFR8(0xFF00). Reference osc 12.8MHz,
 * confirmed stock R-register value gives a 12.5kHz reference step.
 * --------------------------------------------------------------------- */
#define PLL_P0          SFR8(0xFF00)
#define PLL_ENABLE_BIT  0x01   /* P00 */
#define PLL_CLOCK_BIT   0x02   /* P01 */
#define PLL_DATA_BIT    0x04   /* P02 */
#define PLL_LOCK_PORT   SFR8(0xFF03)
#define PLL_LOCK_BIT    0x04   /* P32/CI1 -- 1 = locked (confirmed from 2 real wait-loop sites) */
#define PLL_REF_STEP_HZ 12500UL /* 12.5kHz, from stock R=0x4400 (low 13 bits = 1024) */

/* ---------------------------------------------------------------------
 * BU4094 output expander (doc G). Shadow byte lives in RAM at 0xFE28
 * (in the saddr window) -- read-modify-write it directly, then call
 * bu4094_commit() (resident_api.h, already built).
 * --------------------------------------------------------------------- */
#define BU4094_SHADOW   RAM8(0xFE28)
#define BU4094_BIT_AM   0x01  /* audio mute/control */
#define BU4094_BIT_AC   0x02  /* couples with TR/STT -- meaning [OPEN] */
#define BU4094_BIT_TR   0x04  /* T/R-related, complementary with STT */
#define BU4094_BIT_STT  0x08  /* complementary with TR */
#define BU4094_BIT_RLO  0x10  /* gated output -- condition [OPEN] */
#define BU4094_BIT_LOK  0x20  /* link-complete/accessory signaling */
#define BU4094_BIT_STR  0x40  /* VCO select: set = receive, clear = transmit
                                 (stock TK-840 pll_tune_rx 9B23h sets it,
                                 pll_tune_tx 9AC1h clears it) */
#define BU4094_BIT_SQ   0x80  /* logic squelch/accessory output */
#define BU4094_RESET_VALUE 0x25 /* mask ROM's own power-up value: TR=1, rest 0 */

/* ---------------------------------------------------------------------
 * M62363 DAC (doc G). Reached only via CALLT slot 0x66 -> 0x808F ->
 * BR 0x1F24 in the real firmware (21 confirmed call sites), never a
 * direct CALL 0x1F24. dac_write() (resident_api.h) goes through the
 * same CALLT slot for that reason.
 * --------------------------------------------------------------------- */
#define DAC_CALLT_SLOT_ADDR 0x0066UL
/* Confirmed channel map (doc G) -- treat any channel not listed here
 * as unconfirmed; do not invent a use for it. */
#define DAC_CH_VCXO_OR_TX  2   /* EEPROM 0xF4 */
#define DAC_CH_UNKNOWN3    3   /* EEPROM 0xF6 */
#define DAC_CH_UNKNOWN6    6   /* EEPROM 0xEC */
#define DAC_CH_VOLUME_CANDIDATE 5 /* ROM table @0x0D14, indexed by RAM 0xFEA7 -- NOT bench-confirmed */

/* ---------------------------------------------------------------------
 * PTT / KEY (doc H). KEY is the only thing that actually enables RF --
 * never assert it without completing the safe sequence in doc F (PLL
 * lock + calibration first). P10/P06/P15 are PTT *request* sources,
 * not KEY itself; P15's direction-switching with programming mode is
 * [OPEN], so it is NOT wired up here.
 * --------------------------------------------------------------------- */
#define KEY_PORT        SFR8(0xFF01)
#define KEY_BIT         0x80   /* P17, active HIGH -- the only real TX enable */
#define PTT_EXT_PORT    SFR8(0xFF01)
#define PTT_EXT_BIT     0x01   /* P10, active LOW, external/accessory PTT request */

/* Squelch: deliberately NOT implemented here. Doc H confirms the old
 * SFR8(0xFF04) bit-read idea is wrong/obsolete, and the real algorithm
 * (P30/CI0 noise-pulse counting, reached via interrupt vectors, not a
 * polled register) is not yet decoded ([OPEN] in the doc, flagged as
 * "next research step, not another register search"). Do not add a
 * squelch_status() stub that reads a plausible-looking register --
 * that would be fabricating a hardware contract this project has not
 * confirmed. Revisit once the interrupt-driven algorithm is traced. */

/* ---------------------------------------------------------------------
 * Model detection (doc C). Resident 1F15 writes its result into these
 * fixed RAM flag bits -- call model_detect() (resident_api.h) first,
 * then read these.
 * --------------------------------------------------------------------- */
#define MODEL_FAMILY_FLAG_ADDR   0xFED3
#define MODEL_FAMILY_FLAG_BIT    0x10   /* bit 4 */
#define MODEL_SPLIT_FLAG1_ADDR   0xFEDA
#define MODEL_SPLIT_FLAG1_BIT    0x01   /* bit 0 */
#define MODEL_SPLIT_FLAG2_ADDR   0xFEDF
#define MODEL_SPLIT_FLAG2_BIT    0x10   /* bit 4 */

/* ---------------------------------------------------------------------
 * Panel link (doc E). Resident RAM state -- FE2C/FEDA/FEDD/FEBC-FEBE
 * are all in the saddr window.
 * --------------------------------------------------------------------- */
#define PANEL_RX_BYTE       RAM8(0xFE2C)
#define PANEL_RX_BACKUP1    RAM8(0xFEBC)
#define PANEL_RX_BACKUP2    RAM8(0xFEBD)
#define PANEL_RX_BACKUP3    RAM8(0xFEBE)
#define PANEL_STATE_FEDA    RAM8(0xFEDA)
#define PANEL_TX_BUSY_BIT   0x02   /* FEDA.1 */
#define PANEL_RX_PENDING_BIT 0x04  /* FEDA.2 */
#define PANEL_STATE_FEDD    RAM8(0xFEDD)
#define PANEL_QUEUE_BIT0    0x01
#define PANEL_QUEUE_BIT1    0x02
#define PANEL_QUEUE_BIT2    0x04
#define PANEL_IDLE_BYTE     0xFF
/* Panel command bytes, keys and beeps: panel_codes.h, in include/ at the
   top of this repository. */
#include "panel_codes.h"

#define EXIC0_REG   SFR8(0xFFC8)   /* panel-link INTE0 mask -- masked by 1F12's baseline, unmask deliberately */

#endif
