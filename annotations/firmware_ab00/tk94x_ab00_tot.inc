; Independently written annotations for AB00 TK-940/TK-941 application flash.
; Copyright (c) 2026 Dave, KY4WI. GPL-3.0-only.
; Use only with your own verified 0x6600-byte AB00 firmware binary at 8000h.
; No copyrighted Kenwood program bytes are included in this database.
;
; [FW] = directly supported by original AB00 code or byte comparison
; [RE] = inference based on control flow and codeplug-format research
; [OPEN] = behavior or operating-mode reachability not yet confirmed

        .equ    tot_load_counter, 0x94E6        ; [FW] selects Dispatch/TEL codeplug TOT and loads counter
        .equ    tot_dispatch_raw, 0xE662        ; [RE] codeplug +0062h: Dispatch TX TOT
        .equ    tot_telephone_raw, 0xE661       ; [RE] codeplug +0061h: telephone/interconnect TOT
        .equ    tot_countdown, 0xFE40           ; [FW] 16-bit countdown shared by the paths below
        .equ    tot_prescaler, 0xFE42           ; [FW] reloads with 0Ah
        .equ    timer_flags_fe7d, 0xFE7D        ; [FW] bit 0 set when either path detects zero
        .equ    timer_gate_fe78, 0xFE78         ; [OPEN] meaning of bit 7 not yet established
        .equ    tot_periodic_tick, 0xB19F       ; [FW] prescaled counter decrement and expiration test
        .equ    tot_zero_branch, 0xB1AE         ; [FW] historical two-byte patch site
        .equ    tot_zero_action, 0xB1B1         ; [FW] set FE7D.0 and call C2DCh
        .equ    tot_zero_callback, 0xC2DC       ; [OPEN] full side effects not yet traced
        .equ    tot_other_tick, 0xD6FC          ; [FW] alternate gated countdown path
        .equ    tot_other_decrement, 0xD702     ; [FW] same FE40h countdown, no local call C2DCh

;@rom 0x8000 26112 0x6C6A917F

;@ 0x94E6 code
;@ 0x94E6 text ;
;@ 0x94E6 text ; ============================================================================
;@ 0x94E6 text ; [FW][RE] APPLICATION FLASH: TX TIME-OUT TIMER INITIALIZATION
;@ 0x94E6 text ; ============================================================================
;@ 0x94E6 text ; Firmware: TK-940/TK-941 shared AB00 image, CRC-32 6C6A917F (8000-E5FF).
;@ 0x94E6 text ; The range here starts within a larger function; it is not a standalone API.
;@ 0x94E6 text ; FE79.1 and FE7F.2 select which programmed timer byte to read.
;@ 0x94E6 text ; Dispatch uses E662, telephone/interconnect uses E661.
;@ 0x94E6 text ; Both accept raw 00-27h (0-39): (raw+1)*15 seconds.
;@ 0x94E6 text ; Higher values are NOT treated as Off:
;@ 0x94E6 text ;   Dispatch >=28h is replaced by raw 03h (60 seconds).
;@ 0x94E6 text ;   Telephone >=28h is replaced by raw 0Bh (180 seconds).
;@ 0x94E6 text ; Thus FFh currently selects the fallback, not disabled TOT.
;@ 0x94E6 label tot_load_counter
;@ 0x94ED ; [FW] A <- codeplug E662h (Dispatch), if that selection is active
;@ 0x94ED op 2 tot_dispatch_raw
;@ 0x94F1 ; [FW] limit check: compare A with 28h (40)
;@ 0x94F5 ; [FW] out-of-range Dispatch value -> 03h (60 seconds after INC)
;@ 0x94F9 ; [FW] A <- codeplug E661h (telephone/interconnect)
;@ 0x94F9 op 2 tot_telephone_raw
;@ 0x94FD ; [FW] telephone limit check: compare A with 28h (40)
;@ 0x9501 ; [FW] out-of-range telephone value -> 0Bh (180 seconds after INC)
;@ 0x9503 ; [FW] raw index to count of 15-second intervals: A = raw+1
;@ 0x9507 ; [FW] 01F4h = 500 countdown units for each 15-second interval
;@ 0x950A ; [FW] multiply the selected interval count by 500
;@ 0x950E ; [FW] load 16-bit countdown FE40h
;@ 0x950E op 1 tot_countdown
;@ 0x9510 ; [FW] clear FE7D.0, which the expiration paths later set
;@ 0x9512 ; [FW] initialize prescaler FE42h to 0Ah
;@ 0x9512 op 1 tot_prescaler

;@ 0x9516 data byte
;@ 0xB19F code
;@ 0xB19F text ;
;@ 0xB19F text ; ============================================================================
;@ 0xB19F text ; [FW] PRESCALED COUNTDOWN; SITE OF HISTORICAL NO-TOT PATCH
;@ 0xB19F text ; ============================================================================
;@ 0xB19F text ; Decrement FE42; when it reaches zero, reload 0Ah, decrement FE40,
;@ 0xB19F text ; then compare the 16-bit value with zero. On expiration, set
;@ 0xB19F text ; FE7D.0 and call C2DCh.
;@ 0xB19F text ; Historical patch found in an anonymously authored firmware image:
;@ 0xB19F text ;   CPU B1AE-B1AF (file 31AE-31AF): 81 01 -> 00 00.
;@ 0xB19F text ;   This replaces BE +1 with NOP/NOP. Execution then always reaches
;@ 0xB19F text ;   the RET at B1B0, bypassing FE7D.0 set and CALL C2DCh here.
;@ 0xB19F text ;   It does NOT stop the counter or alter the programmed timeout.
;@ 0xB19F text ; [OPEN] A different path at D702 also decrements FE40 and can
;@ 0xB19F text ; set FE7D.0. This patch alone is NOT proof that all modes lack TOT.
;@ 0xB19F label tot_periodic_tick
;@ 0xB19F ; [FW] decrement FE42 prescaler
;@ 0xB1A1 ; [FW] return when prescaler is not yet zero
;@ 0xB1A3 ; [FW] reload FE42 with 0Ah
;@ 0xB1A6 ; [FW] decrement FE40 countdown even in historical patched firmware
;@ 0xB1A9 ; [FW] read FE40 countdown into AX
;@ 0xB1AB ; [FW] set zero flag when AX == 0
;@ 0xB1AE text ; [FW] HISTORICAL PATCH SITE: BE tot_zero_action -> NOP / NOP
;@ 0xB1AE text ; Stock reaches B1B1 only when countdown has reached zero.
;@ 0xB1AE text ; NOP / NOP falls through to RET at B1B0 for both outcomes.
;@ 0xB1AE label tot_zero_branch
;@ 0xB1AE ; [FW] conditional branch on equal; bytes 81 01 in stock AB00
;@ 0xB1B0 ; [FW] patched and unexpired stock path returns here
;@ 0xB1B1 label tot_zero_action
;@ 0xB1B1 ; [FW] set FE7D.0 timer-expired state
;@ 0xB1B3 ; [FW] call C2DCh on expiration in stock code
;@ 0xB1B3 op 1 tot_zero_callback
;@ 0xB1B6 ; [FW] return

;@ 0xB1B7 data byte
;@ 0xD6FC code
;@ 0xD6FC text ;
;@ 0xD6FC text ; ============================================================================
;@ 0xD6FC text ; [FW] SECOND FE40 COUNTDOWN/EXPIRATION PATH
;@ 0xD6FC text ; ============================================================================
;@ 0xD6FC text ; FE78.7 gates this path. When it is set, the code decrements
;@ 0xD6FC text ; FE40 and sets FE7D.0 if the value reaches zero. Unlike B1B3,
;@ 0xD6FC text ; there is no local CALL C2DCh in this path.
;@ 0xD6FC text ; [OPEN] Determine operating mode(s) that enable FE78.7 and
;@ 0xD6FC text ; whether this path can expire TOT with the historical patch.
;@ 0xD6FC label tot_other_tick
;@ 0xD6FC ; [FW] branch around FE40 decrement when FE78.7 is clear
;@ 0xD702 label tot_other_decrement
;@ 0xD702 ; [FW] second DECW FE40; NOT modified by historical TOT patch
;@ 0xD705 ; [FW] read FE40 into AX
;@ 0xD707 ; [FW] compare FE40 against zero
;@ 0xD70A ; [FW] set FE7D.0 when this countdown reaches zero
;@ 0xD70C ; [FW] independent FEAE countdown follows
;@ 0xD719 data byte
