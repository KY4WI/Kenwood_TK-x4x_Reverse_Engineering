; SPDX-License-Identifier: GPL-3.0-only
; Copyright (c) 2026 Dave, KY4WI
; Macros for the TK-x4x listings, for 78k_disasm (-i tkx4x_macros.inc,
; -m NAME) and vasm. Bodies use numbers or names from uPD78312A.inc only.

; Restart the watchdog: WDM is written 00H, then 86H (each write carries
; its complement byte, as WDM requires).
        .macro  KICK_WDT
        mov     wdm, #0x00
        mov     wdm, #0x86
        .endm
