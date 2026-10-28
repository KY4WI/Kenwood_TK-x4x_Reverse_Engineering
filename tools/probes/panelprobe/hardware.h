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
 * mutable global to .bss (see selftest.ld's own comment for the
 * .data-vs-.bss split and why a mutable global MUST have no explicit
 * initializer to land there). selftest.ld maps .bss to ramdata_region,
 * 0xFE40-0xFE5F (32 bytes), chosen to sit clear of every RAM address
 * already claimed below (0xFE28 BU4094 shadow, 0xFE29-0xFE2B resident
 * EEPROM call scratch, 0xFE2C panel RX byte) with headroom to grow
 * before 0xFE80 (where resident code itself sometimes borrows
 * "unused register bank" scratch). Grow ramdata_region's LENGTH there
 * (and re-check for collisions) if more RAM-backed globals are needed
 * -- but every byte taken from FE20-FEEF is a byte not available to
 * the stack's actual headroom above its nominal 32, so keep it small.
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
#define BU4094_BIT_STR  0x40  /* TX/RX VCO selection -- see PLL sequencing */
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
#define PANEL_CMD_DISPLAY_TEXT 0x02  /* + 8 character bytes, space-padded */

/* BETA_22: sub_118D (the real CLONE-mode routine, section 11.14) sends
 * this ONE byte, via a plain single-byte send (no following data),
 * immediately before its first (and only) 0x02+8-byte display write --
 * confirmed from the mask ROM's own disassembly:
 *   mov a,#0x8e; call trampoline_panel_tx_queue   (this byte, alone)
 *   movw hl,#<"CLONE  " ptr>; call 0x15c8         (then the 0x02 burst)
 * hardware doc section 11.10 already names 0x8E as the byte sub_118D
 * sends right before showing " CLONE  ", but had not yet connected that
 * to the possibility that 0x8E is a genuine PREREQUISITE for the 8-byte
 * burst to be accepted, rather than just a CLONE-specific side effect.
 * BETA_17-21 all left the symptom (only the burst's first byte is ever
 * displayed; every later position shows that first byte's value plus
 * its own position index, regardless of actual content) completely
 * unchanged across four structurally different fixes at four different
 * points around the send -- never tried: whether 0x02 alone is even a
 * valid "accept 8 bytes" command without 0x8E sent first. This project
 * has never sent 0x8E anywhere. */
#define PANEL_CMD_CLONE_ANNOUNCE 0x8E

/* Panel sync handshake (previously missing from the documentation): the mask ROM's
 * OWN reset_handler (0x0040) sends 0xB0 and waits for the panel to
 * echo 0xB1 back into RAM 0xFE26, immediately after EI and before ever
 * validating the app signature or dispatching into application code.
 * See panel.c's panel_handshake(), built from already-confirmed stable
 * primitives (1F2D to send, 1F2A to recover on timeout) rather than
 * calling the real handshake routine (mask ROM 0x0159) directly, since
 * that address is not one of the stable 0x1Fxx trampoline entries. */
#define PANEL_HANDSHAKE_SYNC_BYTE 0xB0
#define PANEL_HANDSHAKE_ACK_BYTE  0xB1
#define PANEL_HANDSHAKE_RESPONSE  RAM8(0xFE26)

/* BETA_24: command bytes recovered by direct disassembly of
 * the external stock application firmware (hardware doc section 17.5-17.7,
 * "2026-10-08 update") -- not guessed. `0x96`/`0x97` are the
 * strongest-confirmed pair in that trace (dozens of independent call
 * sites; hardware doc section 11.13's own sanity-check note already
 * flagged them as TX on/off). `0x8B` is the command byte of a completely
 * different 3-byte "secondary/status" frame (`0x8B` + 2 data bytes,
 * section 17.6's `0xA531`), sent over the exact same transport as the
 * `0x02` text burst but structurally unrelated to it. */
#define PANEL_CMD_TX_ON             0x96
#define PANEL_CMD_TX_OFF            0x97
#define PANEL_CMD_SECONDARY_FRAME   0x8B

/* BETA_30: command bytes recovered via the real-time byte picker
 * (panel.c/panelprobe.c, user-driven bench exploration -- see hardware doc
 * section 11.21). Every icon follows the SAME triplet pattern: base+0
 * = on, base+1 = off, base+2 = flash (flash = "this icon joins global
 * flash mode 0x82's flashing set" -- it does not flash on its own
 * unless 0x82 is also active). Confirmed exhaustively for all nine
 * icons; no exceptions found. */
#define PANEL_CMD_TA_ON         0x90
#define PANEL_CMD_TA_OFF        0x91
#define PANEL_CMD_TA_FLASH      0x92
#define PANEL_CMD_TEL_ON        0x93  /* phone icon */
#define PANEL_CMD_TEL_OFF       0x94
#define PANEL_CMD_TEL_FLASH     0x95
/* 0x96/0x97 (TX on/off, already named above) fit the same slot in this
 * range but no 0x98 "TX flash" exists -- confirmed a genuine no-op. */
#define PANEL_CMD_BUSY_ON       0x99
#define PANEL_CMD_BUSY_OFF      0x9A
#define PANEL_CMD_BUSY_FLASH    0x9B
#define PANEL_CMD_CALL_ON       0x9C
#define PANEL_CMD_CALL_OFF      0x9D
#define PANEL_CMD_CALL_FLASH    0x9E
#define PANEL_CMD_SCIRCLE_ON    0xA0  /* the "(S)" icon */
#define PANEL_CMD_SCIRCLE_OFF   0xA1
#define PANEL_CMD_SCIRCLE_FLASH 0xA2
#define PANEL_CMD_ARROW_ON      0xA3
#define PANEL_CMD_ARROW_OFF     0xA4
#define PANEL_CMD_ARROW_FLASH   0xA5
/* BETA_30 correction: hardware doc section 11.15 had previously
 * INFERRED (explicitly flagged uncertain) "0xA7 OPT off, 0xA8 OPT on".
 * The byte-picker data confirms the real, triplet-consistent
 * assignment below -- supersedes the old inferred entry. */
#define PANEL_CMD_OPT_ON        0xA6
#define PANEL_CMD_OPT_OFF       0xA7
#define PANEL_CMD_OPT_FLASH     0xA8
#define PANEL_CMD_AUX_ON        0xA9
#define PANEL_CMD_AUX_OFF       0xAA
#define PANEL_CMD_AUX_FLASH     0xAB

/* BETA_30: global/lamp-test mode bytes. All four are confirmed to
 * layer with the icon on/off bytes above rather than override them --
 * e.g. 0x82 then 0x91 (TA off) removes TA from the flashing set rather
 * than being ignored. */
#define PANEL_CMD_ALL_ON        0x80  /* lamp test: every icon on */
#define PANEL_CMD_ALL_FLASH     0x81  /* every icon flashing */
#define PANEL_CMD_FLASH_MODE    0x82  /* icons flash; ALSO extends flashing
                                       * to whatever text is currently
                                       * displayed */
#define PANEL_CMD_ALL_SEGMENTS  0x83  /* 0x82's effect, plus every LCD
                                       * text segment lit (displayed text
                                       * is still shown underneath) */

/* BETA_30: the panel has its own NATIVE, built-in display strings,
 * triggered by a single command byte -- these completely bypass
 * PANEL_CMD_DISPLAY_TEXT (0x02)'s suspect 8-byte-burst mechanism. This
 * is the third confirmed-clean multi-character display result this
 * project has ever seen (alongside panel_test_nine_byte_burst() and
 * panel_test_secondary_frame()), using genuinely varied content -- see
 * hardware doc section 11.21. 0x84/0x87 (immediately adjacent,
 * untested) may hold more of these; not yet explored. */
#define PANEL_CMD_NATIVE_SCAN_STR   0x85  /* shows "- SCAN -" */
#define PANEL_CMD_NATIVE_AUTOTEL_STR 0x86 /* shows "AUTONTEL", confirmed
                                           * read directly off the real
                                           * display, NOT the stock
                                           * "AUTO TEL" string disassembled
                                           * in section 11.16/17.7 -- same
                                           * neighborhood of letters, odd
                                           * extra "N", genuinely what the
                                           * panel renders for this byte */

/* BETA_30: 0xB0 sent mid-session (i.e. NOT as part of the boot-time
 * panel_handshake() exchange) cleanly, repeatably CLEARS THE SCREEN --
 * it is not a "stuck waiting for the rest of the handshake" state as
 * BETA_29's bench report (the AF/B0 blank-screen symptom) was
 * originally guessed to mean. Simplest reading now: 0xB0 may just BE
 * a "clear screen" command in the panel's own vocabulary, and the
 * mask ROM's boot handshake is sending it for exactly that reason
 * (clear whatever the panel is showing at power-on) -- the panel's
 * 0xB1 echo back into FE26 may be an independent boot-time ack rather
 * than something specifically tied to 0xB0's meaning. Not yet proven,
 * but no longer treated as a "hang" risk. PANEL_HANDSHAKE_SYNC_BYTE
 * above already names this same 0xB0 value for the boot-handshake use. */

/* BETA_30 CAUTION, RESOLVED BETA_31: 0xB3 sent ALONE via the byte
 * picker LOCKS UP the panel (BETA_30 bench report). Direct disassembly
 * of the real stock application image (hardware doc section 17.7) now
 * explains why: 0xB3 is NEVER sent alone anywhere in stock code. All
 * 20 confirmed call sites send 0xB3 then exactly ONE more standalone
 * byte immediately afterward (sub-codes 0x01 through 0x0D observed),
 * through the same CALLT[0]/CALLT[1] trampoline, back-to-back, no gap.
 * 0xB3 is a genuine TWO-BYTE command PREFIX -- not a dangerous value on
 * its own -- and the picker's "lockup" is exactly what sending half of
 * a two-byte command and then waiting (at human reaction speed) for
 * the second byte would be expected to do: the panel is left waiting
 * for a byte that, from its perspective, never arrives in time.
 * PANEL_CMD_B3_PREFIX names the first byte; there is deliberately NO
 * constant for any sub-code yet -- none has a bench-confirmed meaning,
 * only confirmed EXISTENCE as a real value stock code sends. See
 * panel.c's panel_test_b3_subcode() (BETA_31), which always sends both
 * bytes back-to-back, matching stock's own shape -- never send
 * PANEL_CMD_B3_PREFIX alone. */
#define PANEL_CMD_B3_PREFIX 0xB3

/* BETA_32 CAUTION: bench-confirmed -- sub-code 0x0E (one past the
 * highest sub-code, 0x0D, actually observed in stock disassembly)
 * LOCKS UP the panel too, same as 0xB3 sent alone did. 0x01-0x0D is
 * therefore being treated as the real, bounded range for this command
 * family (consistent with stock code: it is the complete set of
 * values stock itself was ever seen sending, section 11.22/17.7) until
 * proven otherwise -- most likely a fixed-size lookup table on the
 * panel's own side, indexed by this byte with no bounds check, so a
 * value past the table's end reads/jumps into whatever happens to
 * follow it in the panel's own memory. panel_test_b3_subcode() (BETA_32)
 * refuses to send anything outside this range. Do NOT widen it without
 * new bench evidence -- see hardware doc section 11.23. */
#define PANEL_B3_SUBCODE_MIN 0x01
#define PANEL_B3_SUBCODE_MAX 0x0D

#define EXIC0_REG   SFR8(0xFFC8)   /* panel-link INTE0 mask -- masked by 1F12's baseline, unmask deliberately */

/* BETA_19/20: a PM1/sfr[0xCA]/sfr[0xCC]/RAM-0xFEDE block from mask-ROM
 * sub_118d was tried here as panel_link_arm() (BETA_19) on the theory
 * that it was sub_118d's pre-panel-send setup. Disproven on real
 * hardware, and a closer re-read of sub_118d's disassembly showed why:
 * that block actually runs AFTER sub_118d's own panel sends (0x8E +
 * "CLONE" text), not before -- almost certainly CLONE-mode's own prep
 * for what it does next (flash programming), unrelated to the panel
 * link. Removed from panel.c/panel.h; the register identities are
 * still worth recording for anyone tracing this again, now cross-
 * checked against the datasheet's full interrupt-controller table
 * (docs/hardware/ (the hardware reference) section 19): sfr[0xCA] is EXIC1
 * (EXIF1's own interrupt-control register, not some generic "other
 * source"), and sfr[0xCC] is EXIC2 (EXIF2's). Neither is EXIF0 (the
 * panel link's own INTE0, confirmed == EXIC0_REG above) or CRIC11
 * (CRF11, the panel bit-clock, == sfr[0xC6] -- also already identified
 * in that section, and confirmed self-managed by exif0_handler/
 * crf11_handler, not something boot code needs to touch). The real
 * BETA_20 candidate is unrelated to any of these three registers --
 * see crt0.s. */

#endif
