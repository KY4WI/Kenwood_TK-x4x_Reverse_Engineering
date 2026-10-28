; panel_codes.inc -- made from panel_codes.h by make_inc.sh; do not edit.
; See panel_codes.h for what these are and where the evidence is.
; SPDX-License-Identifier: GPL-3.0-only

; ---- Sent to the panel ------------------------------------------------
	.equ	PANEL_TEXT, 0x02                ; + 4 or 8 characters (upper case only)
	.equ	PANEL_ALL_ICONS_ON, 0x80        ; lamp test: every icon on
	.equ	PANEL_ALL_ICONS_FLASH, 0x81     ; every icon flashing
	.equ	PANEL_FLASH_ALL, 0x82           ; every icon and the text flash (stock PROGRAM screen)
	.equ	PANEL_ALL_SEGMENTS, 0x83        ; as PANEL_FLASH_ALL, plus every text segment lit
	.equ	PANEL_SHOW_SCAN, 0x85           ; the panel's own string "- SCAN -"
	.equ	PANEL_SHOW_AUTOTEL, 0x86        ; the panel's own string "AUTONTEL"
	.equ	PANEL_SYS_GROUP, 0x8B           ; + system, group: shows "<n> - <m>"
	.equ	PANEL_RESTART, 0x8E             ; stock: first byte after each restart; turns TX icon off
	.equ	PANEL_TA_ON, 0x90
	.equ	PANEL_TA_OFF, 0x91
	.equ	PANEL_TA_FLASH, 0x92
	.equ	PANEL_TEL_ON, 0x93              ; phone icon
	.equ	PANEL_TEL_OFF, 0x94
	.equ	PANEL_TEL_FLASH, 0x95
	.equ	PANEL_TX_ON, 0x96               ; TX has no flash byte
	.equ	PANEL_TX_OFF, 0x97
	.equ	PANEL_BUSY_ON, 0x99
	.equ	PANEL_BUSY_OFF, 0x9A
	.equ	PANEL_BUSY_FLASH, 0x9B
	.equ	PANEL_CALL_ON, 0x9C
	.equ	PANEL_CALL_OFF, 0x9D
	.equ	PANEL_CALL_FLASH, 0x9E
	.equ	PANEL_SCIRCLE_ON, 0xA0          ; the "(S)" icon, S in a circle
	.equ	PANEL_SCIRCLE_OFF, 0xA1
	.equ	PANEL_SCIRCLE_FLASH, 0xA2
	.equ	PANEL_ARROW_ON, 0xA3            ; triangle
	.equ	PANEL_ARROW_OFF, 0xA4
	.equ	PANEL_ARROW_FLASH, 0xA5
	.equ	PANEL_OPT_ON, 0xA6
	.equ	PANEL_OPT_OFF, 0xA7
	.equ	PANEL_OPT_FLASH, 0xA8
	.equ	PANEL_AUX_ON, 0xA9
	.equ	PANEL_AUX_OFF, 0xAA
	.equ	PANEL_AUX_FLASH, 0xAB
	.equ	PANEL_SYNC, 0xB0                ; reset handshake; the panel answers PANEL_SYNC_ACK
	.equ	PANEL_CLEAR, 0xB0               ; the same byte, sent later: clears the screen
	.equ	PANEL_BEEP, 0xB3                ; + BEEP_xx. Never send it alone: the panel locks up
	.equ	PANEL_BEEP_STOP, 0xB4           ; stops a beep still playing; harmless otherwise

; ---- Beep numbers (the byte after PANEL_BEEP) -------------------------
	.equ	BEEP_BUSY_CONT, 0x01            ; fast and urgent, like a busy signal
	.equ	BEEP_SHORT, 0x02                ; normal key-press sound
	.equ	BEEP_WARBLE_CONT, 0x03          ; two-tone warble, "something is wrong"
	.equ	BEEP_TONE_CONT, 0x04            ; BEEP_SHORT's tone; stock: entering dealer or tuning mode
	.equ	BEEP_VOLUME, 0x05               ; short, BEEP_BUSY_CONT's tone; stock: volume change
	.equ	BEEP_LOW_HIGH_LOW, 0x06         ; low-high-low, fast
	.equ	BEEP_DOUBLE_ALERT, 0x07         ; two beeps; the panel then sends 0xAA. Stock: repeating alert
	.equ	BEEP_LONG_LOW, 0x08             ; long, lower pitch
	.equ	BEEP_ERROR, 0x09                ; 6 or 7 very short beeps, "wrong entry"
	.equ	BEEP_KEY, 0x0A                  ; short; the stock key beep (22 places)
	.equ	BEEP_POWER_ON, 0x0B             ; longer, BEEP_SHORT's tone; stock power-on beep
	.equ	BEEP_DOUBLE, 0x0C               ; two short beeps
	.equ	BEEP_ALERT, 0x0D                ; three long high beeps
	.equ	BEEP_FIRST, 0x01
	.equ	BEEP_LAST, 0x0D                 ; 0x0E and above lock up the panel

; ---- From the panel: keys ----------------------------------------------
	.equ	PANEL_KEY_SYS_UP, 0x80
	.equ	PANEL_KEY_SYS_UP_REL, 0x81
	.equ	PANEL_KEY_SYS_DN, 0x82
	.equ	PANEL_KEY_SYS_DN_REL, 0x83
	.equ	PANEL_KEY_GRP_UP, 0x88
	.equ	PANEL_KEY_GRP_UP_REL, 0x89
	.equ	PANEL_KEY_GRP_DN, 0x8A
	.equ	PANEL_KEY_GRP_DN_REL, 0x8B
	.equ	PANEL_KEY_VOL_UP, 0x90
	.equ	PANEL_KEY_VOL_UP_REL, 0x91
	.equ	PANEL_KEY_VOL_DN, 0x92
	.equ	PANEL_KEY_VOL_DN_REL, 0x93
	.equ	PANEL_KEY_SCAN, 0x98
	.equ	PANEL_KEY_SCAN_REL, 0x99
	.equ	PANEL_KEY_AUX, 0x9C
	.equ	PANEL_KEY_AUX_REL, 0x9D
	.equ	PANEL_KEY_PWRON_SYS_UP, 0x84
	.equ	PANEL_KEY_PWRON_GRP_UP, 0x8C
	.equ	PANEL_KEY_PWRON_GRP_DN, 0x8D
	.equ	PANEL_KEY_PWRON_AUX, 0x9E

; ---- From the panel: other bytes ---------------------------------------
	.equ	PANEL_SYNC_ACK, 0xB1            ; answer to PANEL_SYNC at reset
	.equ	PANEL_REQ_PREFIX, 0x55          ; comes before PANEL_REQ_PROGRAM and PANEL_REQ_TUNING
	.equ	PANEL_REQ_PROGRAM, 0xA0         ; 1200-baud "PROGRAM" on the mic jack: KPG monitor (1F00h)
	.equ	PANEL_REQ_ERASE_TUNE, 0xA1      ; erase the whole codeplug, then tuning; no prefix needed
	.equ	PANEL_REQ_TUNING, 0xA2          ; tuning session
	.equ	PANEL_BEEP_REPLY, 0xAA          ; sent once after BEEP_DOUBLE_ALERT; meaning [OPEN]
