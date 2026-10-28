/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* panel_codes.h -- names for the bytes on the front-panel link.
 *
 * Every value here is understood well enough to name: bench-tested on a
 * real panel, or seen in the stock firmware with a clear meaning. The
 * evidence is in the hardware reference, docs/hardware/ (sections 11.8,
 * 11.10, 11.11), and docs/hardware/Front_Panel_Link.md (section 7). Bytes still
 * [OPEN] (0x84, 0x87, 0xB1/0xB2 sent to the panel, most of 0x88-0x8F)
 * get no name until we know what they do.
 *
 * The same value means different things in each direction (0x90 is
 * "TA icon on" going to the panel and "Volume Up pressed" coming back),
 * so the names say which way:
 *   PANEL_...       sent to the panel (commands)
 *   PANEL_KEY_...   from the panel: a key pressed or released
 *   PANEL_REQ_...   from the panel: a session request
 *   BEEP_...        the number sent after PANEL_BEEP
 *
 * panel_codes.inc is the assembler version, made from this file by
 * make_inc.sh; edit this file and run it. Hex is uppercase so the
 * assembler version reads the same. */

#ifndef PANEL_CODES_H
#define PANEL_CODES_H

/* ---- Sent to the panel ------------------------------------------------ */

#define PANEL_TEXT              0x02  /* + 4 or 8 characters (upper case only) */

#define PANEL_ALL_ICONS_ON      0x80  /* lamp test: every icon on */
#define PANEL_ALL_ICONS_FLASH   0x81  /* every icon flashing */
#define PANEL_FLASH_ALL         0x82  /* every icon and the text flash (stock PROGRAM screen) */
#define PANEL_ALL_SEGMENTS      0x83  /* as PANEL_FLASH_ALL, plus every text segment lit */
#define PANEL_SHOW_SCAN         0x85  /* the panel's own string "- SCAN -" */
#define PANEL_SHOW_AUTOTEL      0x86  /* the panel's own string "AUTONTEL" */
#define PANEL_SYS_GROUP         0x8B  /* + system, group: shows "<n> - <m>" */
#define PANEL_RESTART           0x8E  /* stock: first byte after each restart; turns TX icon off */

#define PANEL_TA_ON             0x90
#define PANEL_TA_OFF            0x91
#define PANEL_TA_FLASH          0x92
#define PANEL_TEL_ON            0x93  /* phone icon */
#define PANEL_TEL_OFF           0x94
#define PANEL_TEL_FLASH         0x95
#define PANEL_TX_ON             0x96  /* TX has no flash byte */
#define PANEL_TX_OFF            0x97
#define PANEL_BUSY_ON           0x99
#define PANEL_BUSY_OFF          0x9A
#define PANEL_BUSY_FLASH        0x9B
#define PANEL_CALL_ON           0x9C
#define PANEL_CALL_OFF          0x9D
#define PANEL_CALL_FLASH        0x9E
#define PANEL_SCIRCLE_ON        0xA0  /* the "(S)" icon, S in a circle */
#define PANEL_SCIRCLE_OFF       0xA1
#define PANEL_SCIRCLE_FLASH     0xA2
#define PANEL_ARROW_ON          0xA3  /* triangle */
#define PANEL_ARROW_OFF         0xA4
#define PANEL_ARROW_FLASH       0xA5
#define PANEL_OPT_ON            0xA6
#define PANEL_OPT_OFF           0xA7
#define PANEL_OPT_FLASH         0xA8
#define PANEL_AUX_ON            0xA9
#define PANEL_AUX_OFF           0xAA
#define PANEL_AUX_FLASH         0xAB

#define PANEL_SYNC              0xB0  /* reset handshake; the panel answers PANEL_SYNC_ACK */
#define PANEL_CLEAR             0xB0  /* the same byte, sent later: clears the screen */
#define PANEL_BEEP              0xB3  /* + BEEP_xx. Never send it alone: the panel locks up */
#define PANEL_BEEP_STOP         0xB4  /* stops a beep still playing; harmless otherwise */

/* ---- Beep numbers (the byte after PANEL_BEEP) ------------------------- */
/* _CONT beeps keep going until PANEL_BEEP_STOP or another beep. */

#define BEEP_BUSY_CONT          0x01  /* fast and urgent, like a busy signal */
#define BEEP_SHORT              0x02  /* normal key-press sound */
#define BEEP_WARBLE_CONT        0x03  /* two-tone warble, "something is wrong" */
#define BEEP_TONE_CONT          0x04  /* BEEP_SHORT's tone; stock: entering dealer or tuning mode */
#define BEEP_VOLUME             0x05  /* short, BEEP_BUSY_CONT's tone; stock: volume change */
#define BEEP_LOW_HIGH_LOW       0x06  /* low-high-low, fast */
#define BEEP_DOUBLE_ALERT       0x07  /* two beeps; the panel then sends 0xAA. Stock: repeating alert */
#define BEEP_LONG_LOW           0x08  /* long, lower pitch */
#define BEEP_ERROR              0x09  /* 6 or 7 very short beeps, "wrong entry" */
#define BEEP_KEY                0x0A  /* short; the stock key beep (22 places) */
#define BEEP_POWER_ON           0x0B  /* longer, BEEP_SHORT's tone; stock power-on beep */
#define BEEP_DOUBLE             0x0C  /* two short beeps */
#define BEEP_ALERT              0x0D  /* three long high beeps */

#define BEEP_FIRST              0x01
#define BEEP_LAST               0x0D  /* 0x0E and above lock up the panel */

/* ---- From the panel: keys ---------------------------------------------- */
/* One byte on press, the next byte up on release (_REL); no repeat codes. */

#define PANEL_KEY_SYS_UP        0x80
#define PANEL_KEY_SYS_UP_REL    0x81
#define PANEL_KEY_SYS_DN        0x82
#define PANEL_KEY_SYS_DN_REL    0x83
#define PANEL_KEY_GRP_UP        0x88
#define PANEL_KEY_GRP_UP_REL    0x89
#define PANEL_KEY_GRP_DN        0x8A
#define PANEL_KEY_GRP_DN_REL    0x8B
#define PANEL_KEY_VOL_UP        0x90
#define PANEL_KEY_VOL_UP_REL    0x91
#define PANEL_KEY_VOL_DN        0x92
#define PANEL_KEY_VOL_DN_REL    0x93
#define PANEL_KEY_SCAN          0x98
#define PANEL_KEY_SCAN_REL      0x99
#define PANEL_KEY_AUX           0x9C
#define PANEL_KEY_AUX_REL       0x9D

/* Read by the mask ROM in its power-on key check, before the application
   starts; different bytes from the ones above. */
#define PANEL_KEY_PWRON_SYS_UP  0x84
#define PANEL_KEY_PWRON_GRP_UP  0x8C
#define PANEL_KEY_PWRON_GRP_DN  0x8D
#define PANEL_KEY_PWRON_AUX     0x9E

/* ---- From the panel: other bytes --------------------------------------- */

#define PANEL_SYNC_ACK          0xB1  /* answer to PANEL_SYNC at reset */
#define PANEL_REQ_PREFIX        0x55  /* comes before PANEL_REQ_PROGRAM and PANEL_REQ_TUNING */
#define PANEL_REQ_PROGRAM       0xA0  /* 1200-baud "PROGRAM" on the mic jack: KPG monitor (1F00h) */
#define PANEL_REQ_ERASE_TUNE    0xA1  /* erase the whole codeplug, then tuning; no prefix needed */
#define PANEL_REQ_TUNING        0xA2  /* tuning session */
#define PANEL_BEEP_REPLY        0xAA  /* sent once after BEEP_DOUBLE_ALERT; meaning [OPEN] */

#endif
