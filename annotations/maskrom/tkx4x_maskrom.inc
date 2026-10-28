; 78k_disasm database, extracted from tkx4x_maskrom.lst.
; Edit the listing, not this file, then run --extract again. The
; part before the database records is plain assembler source and can be
; .include'd; the ;@ records are comments to the assembler.
        .include "uPD78312A.inc"
        .include "tkx4x_macros.inc"

; ---- Symbols ----
        .equ    app_eeprom_bg_callback, 0x8235 ; [FW] confirmed: BR 8381. Called by trampoline_eeprom_*_coop
                                        ; while waiting on EEPROM activity. A replacement firmware
                                        ; using the cooperative EEPROM API needs a valid routine here,
                                        ; at minimum a RET-compatible stub.
        .equ    panel_tx_byte, 0xFE25   ; [FW] panel link: the byte to send, shifted out LSB first;
                                        ; FFh between transfers ("nothing to send")
        .equ    panel_rx_byte, 0xFE26   ; [FW] panel link: the byte being received; after a transfer,
                                        ; the same as panel_rx_queue or FFh
        .equ    panel_bit_count, 0xFE27 ; [FW] panel link: data bits still to read, then stop-bit
                                        ; ticks (see the FRONT PANEL LINK notes at 170Ah)
        .equ    panel_rx_queue, 0xFE2C  ; [FW] panel link: the oldest byte received, valid while
                                        ; FEDA.2 is set. The application reads it and moves
                                        ; panel_rx_queue_1..3 up behind it.
        .equ    panel_rx_queue_1, 0xFEBC ; [FW] panel link: queued byte 2, valid while FEDD.0 is set
        .equ    panel_rx_queue_2, 0xFEBD ; [FW] panel link: queued byte 3, valid while FEDD.1 is set
        .equ    panel_rx_queue_3, 0xFEBE ; [FW] panel link: queued byte 4, valid while FEDD.2 is set;
                                        ; a fifth byte overwrites it
        .equ    outexp_shadow, 0xFE28   ; [RE] BU4094 output-expander shadow byte; 1F27 shifts
                                        ; it out whole. Reset value 25h.
        .equ    eeprom_addr, 0xFE29     ; [RE] AT24C02 address for 1F18/1F1B/1F1E/1F21
        .equ    eeprom_byte, 0xFE2A     ; [RE] 1F1B's result (also returned in A)
        .equ    reset_handler, 0x0040   ; [FW] RESET vector target
        .equ    wdt_handler, 0x089E     ; [FW] WDT vector target
        .equ    default_int_handler, 0x08A4 ; [FW] shared by NMI,TBF,TMF2,CRF00,CRF01,CRF10,SEF,SRF,STF,ADF,BRK
        .equ    tmf0_handler, 0x08A5    ; [FW] TMF0 (timer 0 flag) vector target
        .equ    tmf1_handler, 0x08BA    ; [FW] TMF1 (timer 1 flag) vector target
        .equ    exif1_handler, 0x08CB   ; [FW] EXIF1 (external interrupt 1) vector target
        .equ    kenwood_string, 0x08D7
        .equ    waveform_tables, 0x08F5
        .equ    waveform_table_3, 0x0AF5
        .equ    small_value_tables, 0x0BF5
        .equ    kpg_monitor, 0x0D34     ; [FW] target of trampoline_kpg_monitor (1F00H): DI, watchdog
                                        ; and SP setup, then the 1F0F/1F0C init calls
        .equ    exif2_handler, 0x1513   ; [FW] EXIF2 (external interrupt 2) vector target
        .equ    panel_text8, 0x15C8     ; [FW] sends 02H, then 8 characters from [HL+], each via
                                        ; trampoline_panel_tx_queue. HL points at one of the
                                        ; display_messages (12 calls in the mask ROM).
        .equ    panel_text4, 0x15EE     ; [FW] same with 4 characters. [OPEN] not called from the
                                        ; mask ROM; likely pairs with adjust_item_names.
        .equ    panel_rx_recover, 0x170A
        .equ    panel_tx_start, 0x1714
        .equ    crf11_handler, 0x1726   ; [FW] CRF11 (up/down counter 1) vector target; ends in RETI at 0x17a2
        .equ    exif0_handler, 0x17A3   ; [FW] EXIF0 (external interrupt 0) vector target; ends in RETI at 0x17c1
        .equ    unclassified_1, 0x1C00
        .equ    adjust_item_names, 0x1D00
        .equ    display_messages, 0x1E00
        .equ    trampoline_kpg_monitor, 0x1F00 ; [RE] KPG/programming monitor entry
        .equ    trampoline_bin16_to_bcd4, 0x1F03 ; [RE] 16-bit binary -> 4-digit packed BCD
        .equ    trampoline_bcd_add_signed, 0x1F06 ; [RE] signed packed-BCD add
        .equ    trampoline_bcd_sub_signed, 0x1F09 ; [RE] signed packed-BCD subtract
        .equ    trampoline_clear_resident_ram, 0x1F0C ; [FW] confirmed called 2nd in mask ROM's own reset handler
        .equ    trampoline_gpio_init, 0x1F0F ; [FW] confirmed called 1st in mask ROM's own reset handler
        .equ    trampoline_irq_periph_init, 0x1F12 ; [FW] confirmed called 3rd in mask ROM's own reset handler
        .equ    trampoline_model_rf_detect, 0x1F15 ; [RE] model/RF detector
        .equ    trampoline_eeprom_page_write, 0x1F18 ; [RE] AT24C02 page write, blocking
        .equ    trampoline_eeprom_random_read, 0x1F1B ; [RE] AT24C02 random read, blocking
        .equ    trampoline_eeprom_byte_write, 0x1F1E ; [RE] AT24C02 byte write, blocking
        .equ    trampoline_eeprom_seq_read, 0x1F21 ; [RE] AT24C02 sequential read, blocking
        .equ    trampoline_dac_write, 0x1F24 ; [RE] M62363FP DAC write; A=channel 1..8, X=value
        .equ    trampoline_outexp_commit, 0x1F27 ; [RE] BU4094 output-expander commit; shadow byte at 0xfe28
        .equ    trampoline_panel_rx_recover, 0x1F2A ; [FW] panel link: if the panel's start edge was missed,
                                        ; raise EXIF0 by hand (panel_rx_recover)
        .equ    trampoline_panel_tx_queue, 0x1F2D ; [FW] panel link: send A (panel_tx_start). Waits, with no
                                        ; time limit, while the link is busy
        .equ    trampoline_panel_inte0_isr, 0x1F30 ; [FW] for the application's EXIF0 vector: goes to
                                        ; exif0_handler, which ends in RETI. Never CALL it.
        .equ    trampoline_panel_timed_isr, 0x1F33 ; [FW] for the application's CRF11 vector: goes to
                                        ; crf11_handler, which ends in RETI. Never CALL it.
        .equ    trampoline_flash_write_64, 0x1F36 ; [RE] fixed 64-byte external flash write
        .equ    trampoline_clone_mode, 0x1F39 ; [RE] resident CLONE-mode program, non-returning
        .equ    trampoline_flash_sdp_disable, 0x1F3C ; [RE] AT29C256 software-data-protection disable
        .equ    trampoline_flash_page_prefix, 0x1F3F ; [RE] protected page-program prefix
        .equ    trampoline_eeprom_byte_write_coop, 0x1F42 ; [RE] cooperative AT24C02 byte write; calls app callback
        .equ    trampoline_eeprom_random_read_coop, 0x1F45 ; [RE] cooperative AT24C02 random read; calls app callback
        .equ    trampoline_flash_page_writer, 0x1F48 ; [RE] generic 64-byte AT29C256 page writer
        .equ    trampoline_model_id_field, 0x1F4B ; [RE] radio model-identification field

; ---- Database ----
;@rom 0x0000 8192 0x4BCFCF73
;@global app_eeprom_bg_callback
;@global panel_tx_byte
;@global panel_rx_byte
;@global panel_bit_count
;@global panel_rx_queue
;@global panel_rx_queue_1
;@global panel_rx_queue_2
;@global panel_rx_queue_3
;@global outexp_shadow
;@global eeprom_addr
;@global eeprom_byte
;@macro KICK_WDT
;@ 0x0000 data ptr/1
;@ 0x0000 text ; ============================================================================
;@ 0x0000 text ; TK-x4x MASK ROM: COMMENTED DISASSEMBLY
;@ 0x0000 text ;
;@ 0x0000 text ; The uPD78312AGF3563BE's internal 8 KB mask ROM. It is the same in every
;@ 0x0000 text ; TK-x4x radio (TK-840, TK-940, TK-941): the mask code is part of the part
;@ 0x0000 text ; number, and Kenwood made only this one.
;@ 0x0000 text ;
;@ 0x0000 text ; Recreate an annotated listing using a mask-ROM dump from your own radio:
;@ 0x0000 text ;   78k_disasm rom.bin -d tkx4x_maskrom.inc --listing my_maskrom.lst
;@ 0x0000 text ; The listing is also assembler source when generated with --source.
;@ 0x0000 text ; After editing your listing, extract into NEW filenames for review:
;@ 0x0000 text ;   78k_disasm --extract my_maskrom.lst --db updated_maskrom.inc \
;@ 0x0000 text ;              --symbols updated_maskrom_symbols.inc
;@ 0x0000 text ; Compare and review new outputs before replacing existing data.
;@ 0x0000 text ;
;@ 0x0000 text ; Tags: [FW] read directly from this code; [RE] reverse-engineered or carried
;@ 0x0000 text ; over from earlier work, structurally confirmed here; [OPEN] not confirmed.
;@ 0x0000 text ;
;@ 0x0000 text ; REVERSE-ENGINEERING NOTES
;@ 0x0000 text ; Added from live-radio testing and control-flow analysis, 2026-09-24.
;@ 0x0000 text ;
;@ 0x0000 text ; High-confidence findings documented below:
;@ 0x0000 text ;
;@ 0x0000 text ;   * E66C bits 0-3 individually enable four power-on panel-event modes.
;@ 0x0000 text ;       event 84h -> E66C.2 -> mode 1 -> resident firmware PROGRAM/recovery
;@ 0x0000 text ;       event 9Eh -> E66C.3 -> mode 9 -> external entry 8089h (clone path)
;@ 0x0000 text ;       event 8Ch -> E66C.0 -> mode 3 -> external entry 8083h
;@ 0x0000 text ;       event 8Dh -> E66C.1 -> mode 6 -> external entry 8086h
;@ 0x0000 text ;     Event 84h is the known SYSTEM UP power-on key.  The physical identities
;@ 0x0000 text ;     of 9Eh/8Ch/8Dh are not yet assigned here.
;@ 0x0000 text ;
;@ 0x0000 text ;   * A bad/missing 7-byte KENWOOD signature at E580 forces the resident
;@ 0x0000 text ;     programming/recovery path without depending on external application code.
;@ 0x0000 text ;
;@ 0x0000 text ;   * Two distinct executable-firmware receive paths exist:
;@ 0x0000 text ;       - sequential raw-binary loader at 02F5h, exactly 0x6600 bytes
;@ 0x0000 text ;       - addressed ASCII-HEX loader at 03CDh
;@ 0x0000 text ;
;@ 0x0000 text ;   * The ASCII-HEX loader is Intel-HEX-like, but it is NOT a general Intel
;@ 0x0000 text ;     HEX implementation.  It parses ':' + byte count + 16-bit address, skips
;@ 0x0000 text ;     the two record-type hex characters, accepts the data bytes, and does not
;@ 0x0000 text ;     validate the Intel HEX checksum.  A zero byte count terminates immediately.
;@ 0x0000 text ;
;@ 0x0000 text ;   * Critical host-protocol detail: the byte which first causes FED7.4 to be
;@ 0x0000 text ;     seen at 02A6h is discarded at 02AAh before branching to 03CDh.  Therefore
;@ 0x0000 text ;     a host entering ASCII-HEX mode MUST send one sacrificial/synchronization
;@ 0x0000 text ;     byte before the first ':' record.  Sending ':' as the first byte loses
;@ 0x0000 text ;     that colon and causes the first record to be skipped.
;@ 0x0000 text ;
;@ 0x0000 text ;   * This precisely explains the first tk840upload firmware test failure.
;@ 0x0000 text ;     The intended record sequence 6580,6590,65A0,65B0 / 65C0,... lost the
;@ 0x0000 text ;     6580 record.  The first four records actually accepted were therefore
;@ 0x0000 text ;     6590,65A0,65B0,65C0.  The loader derives the 64-byte Flash destination
;@ 0x0000 text ;     from the fourth record address minus 0x30, yielding an unaligned E590h
;@ 0x0000 text ;     write.  AT29C256 page wrapping then produced the observed E5C0 page:
;@ 0x0000 text ;       +00 = data from record 65C0
;@ 0x0000 text ;       +10 = data from record 6590
;@ 0x0000 text ;       +20 = data from record 65A0
;@ 0x0000 text ;       +30 = data from record 65B0
;@ 0x0000 text ;     This matches the hardware readback exactly.
;@ 0x0000 text ;
;@ 0x0000 text ;   * Corrections 2026-10-13, after checking every line against the current
;@ 0x0000 text ;     disassembler and a byte-exact reassembly of this ROM:
;@ 0x0000 text ;       - (Withdrawn 2026-10-14.) This item swapped the operands of the 24
;@ 0x0000 text ;         two-operand short-direct instructions (MOV/MOVW/CMP/CMPW
;@ 0x0000 text ;         saddr,saddr), based on a reading of the manual's example that the
;@ 0x0000 text ;         code itself contradicts.  See the second 2026-10-14 entry below.
;@ 0x0000 text ;       - 0AF5h-0BF4h is a 256-byte table, not code (see there).
;@ 0x0000 text ;       - The 0BF5h table ends at 0D33h.  0D34h is code: the KPG monitor entry
;@ 0x0000 text ;         the 1F00h trampoline jumps to (kpg_monitor).
;@ 0x0000 text ;       - 1E00h-1EA3h holds the 8-character panel messages shown through
;@ 0x0000 text ;         panel_text8 (15C8h); 1D00h-1D1Fh holds 4-character names.
;@ 0x0000 text ;       - Absolute 16-bit operands carry NEC's '!' marker (call !x, br !x,
;@ 0x0000 text ;         mov a, !x), so they read differently from the shorter relative and
;@ 0x0000 text ;         short-direct forms.  Call targets show their entry names, and SFR
;@ 0x0000 text ;         operands their register names where confirmed.
;@ 0x0000 text ;
;@ 0x0000 text ;   * 2026-10-14: converted to the 78k_disasm listing format.  The listing now
;@ 0x0000 text ;     assembles byte for byte; every address has a name (your labels, the
;@ 0x0000 text ;     register names, or generated sub_/loc_/dat_/ram_/ext_ names); erased
;@ 0x0000 text ;     runs are fills anchored by .org; tables print several items per line.
;@ 0x0000 text ;
;@ 0x0000 text ;   * 2026-10-14: the 24 two-operand short-direct instructions are back in
;@ 0x0000 text ;     their true order (78k_disasm 1.0): the source is the instruction's
;@ 0x0000 text ;     second byte and the destination its third, with no bit 5/4 swap.  The
;@ 0x0000 text ;     code settles it: 179Bh copies panel_rx_byte to FE2Ch, where 11F9h and
;@ 0x0000 text ;     its neighbours read panel bytes; 1044h, 1061h and 1082h load
;@ 0x0000 text ;     eeprom_addr from FE38h before each EEPROM access; 1266h-12A3h load
;@ 0x0000 text ;     FEBFh, the byte 13D3h and 1412h compare a received byte against (set
;@ 0x0000 text ;     to 06h at 1248h), from FEC5h..FEC0h in turn.
;@ 0x0000 text ; ============================================================================
;@ 0x0000 text 
;@ 0x0000 text ; Firmware address used by the mask ROM (outside it)
;@ 0x0000 equ app_eeprom_bg_callback
;@ 0x0000 text 
;@ 0x0000 text ; Resident RAM used by the trampoline API (callers set or read these)
;@ 0x0000 equ panel_tx_byte
;@ 0x0000 equ panel_rx_byte
;@ 0x0000 equ panel_bit_count
;@ 0x0000 equ panel_rx_queue
;@ 0x0000 equ panel_rx_queue_1
;@ 0x0000 equ panel_rx_queue_2
;@ 0x0000 equ panel_rx_queue_3
;@ 0x0000 equ outexp_shadow
;@ 0x0000 equ eeprom_addr
;@ 0x0000 equ eeprom_byte
;@ 0x0000 text 
;@ 0x0000 text ; ------------------------------------------------------------
;@ 0x0000 text ; Hardware vector table (0000H-003FH)
;@ 0x0000 text ; The CPU takes interrupt vectors and the CALLT table from here only while
;@ 0x0000 text ; TPF, bit 1 of the CPU control word CCW (SFR FF4Eh), is 0, as it is from
;@ 0x0000 text ; reset (uPD78312A user's manual, chapter 3). With TPF set it takes them
;@ 0x0000 text ; from the application image instead: vectors at 8000h-803Fh, the CALLT
;@ 0x0000 text ; table at 8040h-807Fh. Every way into the mask ROM that takes over the
;@ 0x0000 text ; machine clears TPF first (0049h, 0D3Dh, 1196h); the application sets it
;@ 0x0000 text ; once its tables are ready, and until then reaches this ROM only with
;@ 0x0000 text ; CALL, never CALLT. This ROM uses no CALLT. The application's EXIF0 and
;@ 0x0000 text ; CRF11 vectors must lead back to 1F30h and 1F33h, or the front panel link
;@ 0x0000 text ; stops.
;@ 0x0000 text ; ------------------------------------------------------------
;@ 0x0000 ; RESET vector
;@ 0x0002 ; NMI vector
;@ 0x0004 ; EXIF0 vector
;@ 0x0006 ; EXIF1 vector
;@ 0x0008 ; EXIF2 vector
;@ 0x000A ; WDT vector
;@ 0x000C ; TBF vector
;@ 0x000E ; TMF0 vector
;@ 0x0010 ; TMF1 vector
;@ 0x0012 ; TMF2 vector
;@ 0x0014 ; (unused)
;@ 0x0016 ; (unused)
;@ 0x0018 ; (unused)
;@ 0x001A ; CRF00 vector
;@ 0x001C ; CRF01 vector
;@ 0x001E ; CRF10 vector
;@ 0x0020 ; CRF11 vector
;@ 0x0022 ; SEF vector
;@ 0x0024 ; SRF vector
;@ 0x0026 ; STF vector
;@ 0x0028 ; ADF vector
;@ 0x002A ; (unused)
;@ 0x002C ; (unused)
;@ 0x002E ; (unused)
;@ 0x0030 ; (unused)
;@ 0x0032 ; (unused)
;@ 0x0034 ; (unused)
;@ 0x0036 ; (unused)
;@ 0x0038 ; (unused)
;@ 0x003A ; (unused)
;@ 0x003C ; (unused)
;@ 0x003E ; BRK vector
;@ 0x0040 code
;@ 0x0040 text 
;@ 0x0040 text ; ------------------------------------------------------------
;@ 0x0040 text ; RESET handler
;@ 0x0040 text ; ------------------------------------------------------------
;@ 0x0040 label reset_handler
;@ 0x0040 ; no interrupts while the hardware is set up
;@ 0x0041 ; kick the watchdog: write 00h,
;@ 0x0045 ; then 86h (WDM writes carry a complement byte)
;@ 0x0049 ; CCW TPF = 0: vectors and CALLT table from
;@ 0x0049 ; this ROM (see the vector table notes at 0000h)
;@ 0x004C ; stack top: the stack grows down from FE1Fh,
;@ 0x004C ; below the resident RAM
;@ 0x0050 ; serial port, register bank 0, STBC, timers
;@ 0x0053 ; preset port latches P0-P3 and port modes
;@ 0x0056 ; zero resident RAM FE20h-FEEFh (D0h bytes)
;@ 0x0059 ; flash write key: FE30h-FE31h = "KJ"
;@ 0x005D ; FE32h-FE33h = "  ": no match, so flash
;@ 0x005D ; writes are locked (0362h, 0396h and 045Eh
;@ 0x005D ; program only while FE32h = FE30h)
;@ 0x0061 ; preset every interrupt control register
;@ 0x0064 ; BU4094 output-expander reset value 25h
;@ 0x0067 ; shift it out to the expander
;@ 0x006A ; settle: wait 41h timer-1 ticks, kicking the
;@ 0x006A ; watchdog
;@ 0x006D ; identify the model and RF band
;@ 0x0070 ; drop any pending external int 0 (panel-link
;@ 0x0070 ; start bit)
;@ 0x0073 ; unmask it
;@ 0x0076 ; interrupts on
;@ 0x0077 ; panel handshake: send B0h until the panel
;@ 0x0077 ; answers B1h (loops forever with no panel)
;@ 0x007A ; Z=1 if E580h-E586h holds "KENWOOD"
;@ 0x007D text 
;@ 0x007D text ; [RE] Reset-mode dispatch.  If the E580 signature comparison fails, force
;@ 0x007D text ; resident mode 1 at 0273h and mark FEDD.3.  With a valid signature, 0196h
;@ 0x007D text ; returns the enabled power-on key mode.  Mode 1 is special-cased to resident
;@ 0x007D text ; 0273h; all other values are added to 8080h, giving the external entry table:
;@ 0x007D text ;   00h -> 8080h normal application entry
;@ 0x007D text ;   03h -> 8083h
;@ 0x007D text ;   06h -> 8086h
;@ 0x007D text ;   09h -> 8089h
;@ 0x007D ; signature good: read the power-on keys
;@ 0x007F ; no valid application: force mode 1
;@ 0x0081 ; flag the forced entry (recovery then shows
;@ 0x0081 ; " UNPROG " instead of "PROGRAM ")
;@ 0x0083 ; go to resident programming/recovery
;@ 0x0086 ; never reached
;@ 0x0088 ; A = power-on key mode: 00h, 01h, 03h, 06h, 09h
;@ 0x008B ; mode 1 (SYSTEM UP)?
;@ 0x008D ; no: enter the application
;@ 0x008F ; yes: resident programming/recovery at 0273h
;@ 0x0092 ; jump there
;@ 0x0094 ; X = 00h
;@ 0x0096 ; A = 00h, X = mode, so AX = 00:mode
;@ 0x0097 ; application entry table at 8080h
;@ 0x009A ; AX = 8080h + mode: 8080h, 8083h, 8086h, 8089h
;@ 0x009C ; jump to recovery or into the application
;@ 0x00A6 ; CK1 = 0: fCLK = 12 MHz / 2 = 6 MHz
;@ 0x00AD ; PSWH = 00h (MOV PSWH,#byte): register bank 0
;@ 0x00B0 ; RSS = 0: A, X, AX are R1, R0, RP0
;@ 0x00B8 ; UDCC1: UDC1 up at fCLK/3, modulo CR11
;@ 0x00BB ; CRC: CR00-CR11 compare, not capture
;@ 0x00C1 ; CR11: a panel link tick is 206 counts (170Ah)
;@ 0x00ED text 
;@ 0x00ED text ; ------------------------------------------------------------
;@ 0x010C text 
;@ 0x010C text ; ------------------------------------------------------------
;@ 0x0116 text 
;@ 0x0116 text ; ------------------------------------------------------------
;@ 0x0144 text 
;@ 0x0144 text ; ------------------------------------------------------------
;@ 0x0159 text 
;@ 0x0159 text ; ------------------------------------------------------------
;@ 0x0159 text ; [RE] panel_handshake: send B0h to the front panel until it answers B1h.
;@ 0x0159 text ; Resends every 5 timer-2 ticks and never gives up, so with no panel connected
;@ 0x0159 text ; the radio stays here.
;@ 0x0159 ; B0h: handshake request to the panel
;@ 0x015B ; send it (waits until the link is free)
;@ 0x015E ; allow 5 timer-2 ticks for the answer
;@ 0x0160 ; kick the watchdog
;@ 0x0168 ; nothing received yet?
;@ 0x016C ; take the byte
;@ 0x016E ; is it the B1h answer?
;@ 0x0171 ; no: ignore it, keep waiting
;@ 0x0173 ; yes: the panel is there
;@ 0x0174 ; back to the wait loop
;@ 0x0176 ; no tick yet: check the receiver
;@ 0x017A ; take the tick
;@ 0x017D ; one tick less to wait
;@ 0x017E ; time left: keep waiting
;@ 0x0180 ; timed out: send B0h again
;@ 0x0182 ; keep waiting
;@ 0x0184 ; restart a receive whose start edge was missed
;@ 0x0187 ; wait loop
;@ 0x0189 ; start over; there is no way out
;@ 0x018B text 
;@ 0x018B text ; [RE] check_external_firmware_signature
;@ 0x018B text ; Compares E580-E586 against the seven-byte resident signature at 08D7h.
;@ 0x018B text ; Reset uses equality to continue normal boot-key dispatch.  A mismatch takes
;@ 0x018B text ; the forced resident recovery path above, so recovery does not depend on a
;@ 0x018B text ; valid external application image.
;@ 0x018B ; DE -> signature in the application image
;@ 0x018E ; HL -> "KENWOOD" in this ROM
;@ 0x018E op 2 kenwood_string
;@ 0x0191 ; seven bytes
;@ 0x0193 ; compare while equal: Z=1 if all seven match
;@ 0x0196 text 
;@ 0x0196 text ; ------------------------------------------------------------
;@ 0x0196 text ; [RE] decode_enabled_power_on_panel_event
;@ 0x0196 text ; Reads codeplug byte E66C and recognizes four panel event codes.  Each event
;@ 0x0196 text ; is accepted only when its corresponding E66C bit is set:
;@ 0x0196 text ;   FE26=84h + E66C.2 -> A=01h  (known SYSTEM UP -> resident firmware loader)
;@ 0x0196 text ;   FE26=9Eh + E66C.3 -> A=09h  (reset dispatch -> external 8089h; clone path)
;@ 0x0196 text ;   FE26=8Ch + E66C.0 -> A=03h  (reset dispatch -> external 8083h)
;@ 0x0196 text ;   FE26=8Dh + E66C.1 -> A=06h  (reset dispatch -> external 8086h)
;@ 0x0196 text ; Otherwise A=00h.  From 01E1h the key must then stay held for 29Ah timer-2
;@ 0x0196 text ; ticks: any further panel event before that means a normal start.
;@ 0x0196 text ; [RE] Bench-confirmed E66C power-on mappings (2026-09-24):
;@ 0x0196 text ; [RE]   bit 0 / panel event 0x8C = GRP UP -> LCD segment test, then normal operation
;@ 0x0196 text ; [RE]   bit 1 / panel event 0x8D = GRP DOWN -> "UHF F1 Tuning" flashes briefly, then apparently normal operation
;@ 0x0196 text ; [RE]   bit 2 / panel event 0x84 = SYSTEM UP -> firmware programming / recovery
;@ 0x0196 text ; [RE]   bit 3 / panel event 0x9E = AUX -> CLONE mode
;@ 0x0196 text ; [RE]
;@ 0x0196 ; wait up to 21h timer-2 ticks for a key event
;@ 0x0198 ; nothing from the panel yet?
;@ 0x019C ; take the byte
;@ 0x019E ; A = codeplug byte E66Ch: modes enabled
;@ 0x01A2 ; SYSTEM UP?
;@ 0x01A5 ; no
;@ 0x01A7 ; yes: enabled by E66Ch bit 2?
;@ 0x01AA ; no: normal start
;@ 0x01AD ; yes: mode 1, resident firmware loader
;@ 0x01AF ; check that the key stays held
;@ 0x01B1 ; AUX?
;@ 0x01B4 ; no
;@ 0x01B6 ; yes: enabled by E66Ch bit 3?
;@ 0x01B9 ; no: normal start
;@ 0x01BC ; yes: mode 9, CLONE (8089h)
;@ 0x01BE ; check that the key stays held
;@ 0x01C0 ; GRP UP?
;@ 0x01C3 ; no
;@ 0x01C5 ; yes: enabled by E66Ch bit 0?
;@ 0x01C8 ; no: normal start
;@ 0x01CB ; yes: mode 3, LCD test (8083h)
;@ 0x01CD ; check that the key stays held
;@ 0x01CF ; GRP DOWN?
;@ 0x01D2 ; no
;@ 0x01D4 ; yes: enabled by E66Ch bit 1?
;@ 0x01D7 ; no: normal start
;@ 0x01DA ; yes: mode 6, tuning (8086h)
;@ 0x01DC ; check that the key stays held
;@ 0x01DE ; any other key: normal start
;@ 0x01E1 ; BC = 029Ah ticks (FEF2h is register BC of
;@ 0x01E1 ; bank 0, reached as saddr)
;@ 0x01E5 ; kick the watchdog
;@ 0x01ED ; no new panel byte?
;@ 0x01F1 ; [RE] another event (key let go): normal start
;@ 0x01F4 ; wait for a timer-2 tick
;@ 0x01F8 ; take the tick
;@ 0x01FB ; count BC down; SUBW on saddr sets Z
;@ 0x01FF ; not held long enough yet
;@ 0x0201 ; held: return with the mode in A
;@ 0x0202 ; no tick yet: keep waiting
;@ 0x0206 ; take the tick
;@ 0x0209 ; one tick less to wait
;@ 0x020A ; time left: keep waiting
;@ 0x020C ; no key at power-on: normal start
;@ 0x020F text 
;@ 0x020F text ; ------------------------------------------------------------
;@ 0x0273 text 
;@ 0x0273 text ; ------------------------------------------------------------
;@ 0x0273 text ; [RE] resident_programming_recovery_main
;@ 0x0273 text ; Entered for SYSTEM-UP firmware recovery (mode 1) and automatically when the
;@ 0x0273 text ; external KENWOOD signature fails.  It offers both raw-binary and ASCII-HEX
;@ 0x0273 text ; firmware receive paths.
;@ 0x0282 op 2 display_messages
;@ 0x02A6 text 
;@ 0x02A6 text ; [RE] ASCII-HEX mode selection.  IMPORTANT: FED7.4 says a serial byte has
;@ 0x02A6 text ; arrived, but this byte is NOT passed to the HEX parser.  02AAh clears the
;@ 0x02A6 text ; receive-ready flag and 02ACh jumps to 03CDh.  Thus the first host byte is a
;@ 0x02A6 text ; sacrificial mode-select byte.  If the host sends ':' first, that colon is
;@ 0x02A6 text ; discarded and 03CDh scans past the remainder of record #1 until record #2.
;@ 0x02F5 text 
;@ 0x02F5 text ; [RE] sequential_raw_firmware_loader
;@ 0x02F5 text ; Waits for FFh synchronization, then programs the complete 8000-E5FF image.
;@ 0x02F5 text ; FE37=CC causes 0xCC iterations, each consuming two 64-byte buffers:
;@ 0x02F5 text ;   0xCC * 0x80 = 0x6600 bytes exactly.
;@ 0x02F5 text ; FE40-FE7F and FE80-FEBF are used as alternating 64-byte buffers while DE
;@ 0x02F5 text ; advances from 8000h.  This path is a true sequential raw-binary loader.
;@ 0x0396 text 
;@ 0x0396 text ; ------------------------------------------------------------
;@ 0x03CD text 
;@ 0x03CD text ; ------------------------------------------------------------
;@ 0x03CD text ; [RE] addressed_ascii_hex_firmware_loader
;@ 0x03CD text ; Intel-HEX-like parser used by the resident firmware programmer.  For each
;@ 0x03CD text ; record it:
;@ 0x03CD text ;   1. scans for ':'
;@ 0x03CD text ;   2. decodes byte count into B/C
;@ 0x03CD text ;   3. byte count 00h -> finish immediately at 04A4h
;@ 0x03CD text ;   4. decodes the 16-bit relative address into DE
;@ 0x03CD text ;   5. reads/discards two hex characters (record type; value is not checked)
;@ 0x03CD text ;   6. decodes exactly B data bytes sequentially into the FE40 staging buffer
;@ 0x03CD text ; It does not explicitly consume or validate the Intel HEX checksum; trailing
;@ 0x03CD text ; characters are ignored while scanning for the next ':'.
;@ 0x03CD text ;
;@ 0x03CD text ; FE37 starts at 4, so four records are accumulated before a Flash write.
;@ 0x03CD text ; With four normal 16-byte sequential records (page,+10,+20,+30), FE40-FE7F
;@ 0x03CD text ; contains exactly one 64-byte AT29C256 page.  After record #4 the loader adds
;@ 0x03CD text ; 8000h to that record's address and subtracts 0030h, deriving the page start.
;@ 0x03CD text ; Therefore record ordering/spacing is part of the protocol; arbitrary record
;@ 0x03CD text ; grouping is unsafe even though the records carry addresses.
;@ 0x0418 text 
;@ 0x0418 text ; [RE] Skip two ASCII-hex characters corresponding to the Intel HEX record
;@ 0x0418 text ; type.  The decoded value is never tested, so this parser does not implement
;@ 0x0418 text ; normal Intel HEX record-type semantics.
;@ 0x0452 text 
;@ 0x0452 text ; [RE] Four records have been accepted.  Program exactly 64 bytes from FE40.
;@ 0x0452 text ; Destination is based on record #4's starting address minus 30h.  This is
;@ 0x0452 text ; correct only for the expected +00/+10/+20/+30 sequence of 16-byte records.
;@ 0x0487 text 
;@ 0x0487 text ; ------------------------------------------------------------
;@ 0x0487 text ; [RE] ascii_hex_digit_to_nibble
;@ 0x0487 text ; Accepts uppercase '0'-'9' and 'A'-'F' only.  Any other character branches
;@ 0x0487 text ; directly to the resident ERROR path at 04BBh.
;@ 0x04A4 text 
;@ 0x04A4 text ; ------------------------------------------------------------
;@ 0x04A4 text ; [RE] firmware_loader_success_finish
;@ 0x04A4 text ; Reached directly when the parser sees a zero byte count.  This proves the
;@ 0x04A4 text ; resident parser does not require a complete standard Intel HEX EOF record;
;@ 0x04A4 text ; ':00' is sufficient to select the finish path once the colon/count are read.
;@ 0x04BB text 
;@ 0x04BB text ; [RE] firmware_loader_error
;@ 0x04BB text ; Invalid ASCII-hex input or an out-of-range addressed record reaches this
;@ 0x04BB text ; non-returning ERROR-display path.
;@ 0x0514 text 
;@ 0x0514 text ; ------------------------------------------------------------
;@ 0x0527 text 
;@ 0x0527 text ; ------------------------------------------------------------
;@ 0x0527 text ; [RE] checksum_external_firmware_8000_e5ff
;@ 0x0527 text ; Sums every byte from 8000h through E5FFh into BC (16-bit additive sum) and
;@ 0x0527 text ; stores the byte-swapped result at FE42.w for the status/checksum display.
;@ 0x0527 text ; This is the same simple 16-bit firmware checksum used for image identification.
;@ 0x0547 text 
;@ 0x0547 text ; ------------------------------------------------------------
;@ 0x0552 text 
;@ 0x0552 text ; ------------------------------------------------------------
;@ 0x0558 text 
;@ 0x0558 text ; ------------------------------------------------------------
;@ 0x057C text 
;@ 0x057C text ; ------------------------------------------------------------
;@ 0x05AA text 
;@ 0x05AA text ; ------------------------------------------------------------
;@ 0x05D8 text 
;@ 0x05D8 text ; ------------------------------------------------------------
;@ 0x0606 text 
;@ 0x0606 text ; ------------------------------------------------------------
;@ 0x0634 text 
;@ 0x0634 text ; ------------------------------------------------------------
;@ 0x0662 text 
;@ 0x0662 text ; ------------------------------------------------------------
;@ 0x0690 text 
;@ 0x0690 text ; ------------------------------------------------------------
;@ 0x06BE text 
;@ 0x06BE text ; ------------------------------------------------------------
;@ 0x06F4 text 
;@ 0x06F4 text ; ------------------------------------------------------------
;@ 0x071A text 
;@ 0x071A text ; ------------------------------------------------------------
;@ 0x0729 text 
;@ 0x0729 text ; ------------------------------------------------------------
;@ 0x0743 text 
;@ 0x0743 text ; ------------------------------------------------------------
;@ 0x075B text 
;@ 0x075B text ; ------------------------------------------------------------
;@ 0x0773 text 
;@ 0x0773 text ; ------------------------------------------------------------
;@ 0x0791 text 
;@ 0x0791 text ; ------------------------------------------------------------
;@ 0x07A9 text 
;@ 0x07A9 text ; ------------------------------------------------------------
;@ 0x07C6 text 
;@ 0x07C6 text ; ------------------------------------------------------------
;@ 0x07D6 text 
;@ 0x07D6 text ; ------------------------------------------------------------
;@ 0x07E6 text 
;@ 0x07E6 text ; ------------------------------------------------------------
;@ 0x0813 text 
;@ 0x0813 text ; ------------------------------------------------------------
;@ 0x0831 text 
;@ 0x0831 text ; ------------------------------------------------------------
;@ 0x0838 text 
;@ 0x0838 text ; ------------------------------------------------------------
;@ 0x0853 text 
;@ 0x0853 text ; ------------------------------------------------------------
;@ 0x0877 text 
;@ 0x0877 text ; ------------------------------------------------------------
;@ 0x089E text 
;@ 0x089E text ; ------------------------------------------------------------
;@ 0x089E text ; WDT interrupt handler
;@ 0x089E text ; ------------------------------------------------------------
;@ 0x089E label wdt_handler
;@ 0x08A4 text 
;@ 0x08A4 text ; ------------------------------------------------------------
;@ 0x08A4 text ; Shared interrupt handler: NMI, TBF, TMF2, CRF00, CRF01, CRF10, SEF, SRF, STF, ADF, BRK
;@ 0x08A4 text ; ------------------------------------------------------------
;@ 0x08A4 label default_int_handler
;@ 0x08A5 text 
;@ 0x08A5 text ; ------------------------------------------------------------
;@ 0x08A5 text ; TMF0 interrupt handler
;@ 0x08A5 text ; ------------------------------------------------------------
;@ 0x08A5 label tmf0_handler
;@ 0x08BA text 
;@ 0x08BA text ; ------------------------------------------------------------
;@ 0x08BA text ; TMF1 interrupt handler
;@ 0x08BA text ; ------------------------------------------------------------
;@ 0x08BA label tmf1_handler
;@ 0x08CB text 
;@ 0x08CB text ; ------------------------------------------------------------
;@ 0x08CB text ; EXIF1 interrupt handler
;@ 0x08CB text ; ------------------------------------------------------------
;@ 0x08CB label exif1_handler
;@ 0x08D7 data string/30
;@ 0x08D7 text 
;@ 0x08D7 text ; ------------------------------------------------------------
;@ 0x08D7 label kenwood_string
;@ 0x08F5 data byte
;@ 0x08F5 text 
;@ 0x08F5 text ; ------------------------------------------------------------
;@ 0x08F5 label waveform_tables
;@ 0x0AF5 text 
;@ 0x0AF5 text ; ------------------------------------------------------------
;@ 0x0AF5 text ; [RE] 256-byte data table, not code (previously disassembled as sub_0af5).
;@ 0x0AF5 text ; Four 64-byte quarters, each the mirror image of its neighbour: a smooth
;@ 0x0AF5 text ; curve, like the waveform tables before it.  Nothing in the ROM refers to any
;@ 0x0AF5 text ; address in 0AF5h-0BF4h, and every branch target that used to land in the
;@ 0x0AF5 text ; middle of an instruction (021Fh, 0B23h, 0B40h, 0B95h, 0BDEh, 0BE9h) came
;@ 0x0AF5 text ; from inside this region.
;@ 0x0AF5 label waveform_table_3
;@ 0x0BF5 text 
;@ 0x0BF5 text ; ------------------------------------------------------------
;@ 0x0BF5 label small_value_tables
;@ 0x0D34 code
;@ 0x0D34 text 
;@ 0x0D34 text ; ------------------------------------------------------------
;@ 0x0D34 text ; [RE] kpg_monitor: where the 1F00h trampoline (trampoline_kpg_monitor) jumps.
;@ 0x0D34 text ; Previously hidden at the end of the 0BF5h table region, which really ends at
;@ 0x0D34 text ; 0D33h.  Starts like the reset handler: DI, watchdog service, SP=FE20h, then
;@ 0x0D34 text ; the 1F0Fh/1F0Ch initialization calls.
;@ 0x0D34 label kpg_monitor
;@ 0x0D3D ; CCW TPF = 0: vectors and CALLT from this ROM
;@ 0x0D71 ; [OPEN] 8Eh, first after a restart
;@ 0x0D76 op 2 display_messages
;@ 0x0DDA ; Serial E: if FEDF.1 is set by W, invoke EEPROM cleanup at 102Bh; else just exit.
;@ 0x0E35 text 
;@ 0x0E35 text ; ------------------------------------------------------------
;@ 0x0E48 text 
;@ 0x0E48 text ; ------------------------------------------------------------
;@ 0x0E6C text 
;@ 0x0E6C text ; ------------------------------------------------------------
;@ 0x0E89 text 
;@ 0x0E89 text ; ------------------------------------------------------------
;@ 0x0EC2 text ; Serial programming W handler. If the W path reaches 0EC6h, it sets
;@ 0x0EC2 text ; FEDF.1 BEFORE testing the destination address/transfer length. The
;@ 0x0EC2 text ; subsequent serial E exit tests this flag and may invoke the EEPROM
;@ 0x0EC2 text ; cleanup at 102Bh even after an invalid/rejected W request. [FW]
;@ 0x0EC2 text ; ------------------------------------------------------------
;@ 0x0F1C text 
;@ 0x0F1C text ; ------------------------------------------------------------
;@ 0x0F32 text 
;@ 0x0F32 text ; ------------------------------------------------------------
;@ 0x0F53 text 
;@ 0x0F53 text ; ------------------------------------------------------------
;@ 0x0F65 text 
;@ 0x0F65 text ; ------------------------------------------------------------
;@ 0x0F76 text 
;@ 0x0F76 text ; ------------------------------------------------------------
;@ 0x0FA4 text 
;@ 0x0FA4 text ; ------------------------------------------------------------
;@ 0x0FDA text 
;@ 0x0FDA text ; ------------------------------------------------------------
;@ 0x1006 text 
;@ 0x1006 text ; ------------------------------------------------------------
;@ 0x102B text ; EEPROM cleanup invoked by the mask-ROM serial E exit only if
;@ 0x102B text ; FEDF.1 was set by the W handler (0EC6h). [FW]
;@ 0x102B text ;   1. Read EEPROM[FFh] into FE38h. If not FFh, set 48 consecutive
;@ 0x102B text ;      EEPROM bytes to FFh starting at that address, 8-bit wrapping.
;@ 0x102B text ;   2. Unconditionally set EEPROM[C0h..D1h] (18 bytes) to FFh.
;@ 0x102B text ;   3. Read EEPROM[FFh] AGAIN and write ROM[E66Fh] to EEPROM at
;@ 0x102B text ;      (new EEPROM[FFh] + 27h) modulo 256.
;@ 0x102B text ; The second FFh read may differ if step 1 overwrote address FFh.
;@ 0x102B text ; Do not confuse this cleanup with the application's tuning 2Bh
;@ 0x102B text ; EEPROM erase. Both are potentially destructive. [FW][RE]
;@ 0x102B text ; ------------------------------------------------------------
;@ 0x1089 text 
;@ 0x1089 text ; ------------------------------------------------------------
;@ 0x1108 text 
;@ 0x1108 text ; ------------------------------------------------------------
;@ 0x1113 text 
;@ 0x1113 text ; ------------------------------------------------------------
;@ 0x1129 text 
;@ 0x1129 text ; ------------------------------------------------------------
;@ 0x1138 text 
;@ 0x1138 text ; ------------------------------------------------------------
;@ 0x114E text 
;@ 0x114E text ; ------------------------------------------------------------
;@ 0x1161 text 
;@ 0x1161 text ; ------------------------------------------------------------
;@ 0x118D text 
;@ 0x118D text ; ------------------------------------------------------------
;@ 0x1196 ; CCW TPF = 0: vectors and CALLT from this ROM
;@ 0x11CA ; [OPEN] 8Eh, first after a restart
;@ 0x13EE text 
;@ 0x13EE text ; ------------------------------------------------------------
;@ 0x1465 text 
;@ 0x1465 text ; ------------------------------------------------------------
;@ 0x148C text 
;@ 0x148C text ; ------------------------------------------------------------
;@ 0x14B6 text 
;@ 0x14B6 text ; ------------------------------------------------------------
;@ 0x1513 text 
;@ 0x1513 text ; ------------------------------------------------------------
;@ 0x1513 text ; EXIF2 interrupt handler
;@ 0x1513 text ; ------------------------------------------------------------
;@ 0x1513 label exif2_handler
;@ 0x153F text 
;@ 0x153F text ; ------------------------------------------------------------
;@ 0x1564 text 
;@ 0x1564 text ; ------------------------------------------------------------
;@ 0x159A text 
;@ 0x159A text ; ------------------------------------------------------------
;@ 0x15C8 text 
;@ 0x15C8 text ; ------------------------------------------------------------
;@ 0x15C8 text ; [RE] panel_text8: show 8 characters on the panel.  Sends 02h, then the 8
;@ 0x15C8 text ; bytes at [HL] (HL advancing), each through trampoline_panel_tx_queue.  The
;@ 0x15C8 text ; callers point HL at one of the display_messages at 1E00h.
;@ 0x15C8 label panel_text8
;@ 0x15C8 ; 02h: characters follow
;@ 0x15CA ; send it
;@ 0x15CD ; character 1
;@ 0x15CE ; send it
;@ 0x15D1 ; character 2
;@ 0x15D2 ; send it
;@ 0x15D5 ; character 3
;@ 0x15D6 ; send it
;@ 0x15D9 ; character 4
;@ 0x15DA ; send it
;@ 0x15DD ; character 5
;@ 0x15DE ; send it
;@ 0x15E1 ; character 6
;@ 0x15E2 ; send it
;@ 0x15E5 ; character 7
;@ 0x15E6 ; send it
;@ 0x15E9 ; character 8
;@ 0x15EA ; send it
;@ 0x15EE text 
;@ 0x15EE text ; ------------------------------------------------------------
;@ 0x15EE text ; [RE] panel_text4: as panel_text8, but 02h plus 4 characters.  Not called
;@ 0x15EE text ; from the mask ROM.  [OPEN] probably used with adjust_item_names (1D00h).
;@ 0x15EE label panel_text4
;@ 0x15EE ; 02h: characters follow
;@ 0x15F0 ; send it
;@ 0x15F3 ; character 1
;@ 0x15F4 ; send it
;@ 0x15F7 ; character 2
;@ 0x15F8 ; send it
;@ 0x15FB ; character 3
;@ 0x15FC ; send it
;@ 0x15FF ; character 4
;@ 0x1600 ; send it
;@ 0x1604 text 
;@ 0x1604 text ; ------------------------------------------------------------
;@ 0x1637 text 
;@ 0x1637 text ; ------------------------------------------------------------
;@ 0x1666 text 
;@ 0x1666 text ; ------------------------------------------------------------
;@ 0x1675 text 
;@ 0x1675 text ; ------------------------------------------------------------
;@ 0x1689 text 
;@ 0x1689 text ; ------------------------------------------------------------
;@ 0x16AC text 
;@ 0x16AC text ; ------------------------------------------------------------
;@ 0x1701 text 
;@ 0x1701 text ; ------------------------------------------------------------
;@ 0x170A text 
;@ 0x170A text ; ------------------------------------------------------------
;@ 0x170A text ; ============================================================================
;@ 0x170A text ; FRONT PANEL LINK
;@ 0x170A text ;
;@ 0x170A text ; [FW] The front panel has its own CPU, which drives the display and reads
;@ 0x170A text ; the keys. It and this CPU swap one byte per transfer, both ways at once,
;@ 0x170A text ; over two lines:
;@ 0x170A text ;   P0.4, out    our bits to the panel; high when idle
;@ 0x170A text ;   P2.1, in     the panel's bits to us; high when idle. Its falling edge is
;@ 0x170A text ;                external interrupt 0 (EXIF0).
;@ 0x170A text ; A transfer is framed like an asynchronous serial byte: a low start bit,
;@ 0x170A text ; 8 data bits LSB first, then the line high. The panel sends its start bit
;@ 0x170A text ; first, and this CPU times the rest from that edge with up/down counter 1,
;@ 0x170A text ; whose compare interrupt CRF11 comes twice per bit:
;@ 0x170A text ;   exif0_handler    the panel's start edge: P0.4 low (our start bit),
;@ 0x170A text ;                    restart the counter, unmask CRF11.
;@ 0x170A text ;   crf11_handler    each tick in turn puts our next bit on P0.4 or reads
;@ 0x170A text ;                    the panel's bit from P2.1, half a bit apart; then two
;@ 0x170A text ;                    bit times of stop, then the transfer ends.
;@ 0x170A text ; To send, panel_tx_start (1F2Dh) waits until the link is free, stores the
;@ 0x170A text ; byte and pulls P0.4 low, which asks the panel for a transfer. When the
;@ 0x170A text ; panel starts one by itself (a key event), this CPU sends FFh, "nothing".
;@ 0x170A text ; Every received byte other than FFh goes into a four-byte queue.
;@ 0x170A text ;
;@ 0x170A text ; Link state, in the resident RAM:
;@ 0x170A text ;   panel_tx_byte     FE25h  the byte to send; FFh between transfers
;@ 0x170A text ;   panel_rx_byte     FE26h  the byte being received
;@ 0x170A text ;   panel_bit_count   FE27h  bits still to read, then stop ticks
;@ 0x170A text ;   FED9h  bit 7      a transfer is running (start edge to end)
;@ 0x170A text ;          bit 4      the next tick reads the panel's bit (clear: sends ours)
;@ 0x170A text ;          bit 3      data done: sending the stop bits
;@ 0x170A text ;          bit 5      stop bits done; bit 6: the last tick before the end
;@ 0x170A text ;                     (bits 0-2 belong to the application)
;@ 0x170A text ;   FEDAh  bit 1      link busy: a byte waits to go, or is going.
;@ 0x170A text ;                     panel_tx_start waits while it is set; the end of
;@ 0x170A text ;                     every transfer clears it.
;@ 0x170A text ;          bit 2      panel_rx_queue holds a byte
;@ 0x170A text ;   FEDDh  bits 0-2   panel_rx_queue_1..3 hold a byte
;@ 0x170A text ;   panel_rx_queue    FE2Ch, then panel_rx_queue_1..3 at FEBCh-FEBEh
;@ 0x170A text ;
;@ 0x170A text ; Timing, from the datasheet: 00C1h sets CR11 (FF0Eh) to 00CDh, and 00B8h
;@ 0x170A text ; sets UDCC1 to 81h: UDC1 (FF1Eh) counts up at fCLK/3 in up/down modulo
;@ 0x170A text ; mode, clearing on the count after it matches CR11, so a tick is 206
;@ 0x170A text ; counts. STBC = 00h (00A6h) makes fCLK 12 MHz / 2 = 6 MHz, so UDC1 counts
;@ 0x170A text ; at 2 MHz: a tick every 103 us, a bit every 206 us (about 4,850 bit/s,
;@ 0x170A text ; which is 4,800 baud 1.1% fast), a transfer of 23 ticks in 2.4 ms, plus
;@ 0x170A text ; the panel's delay in answering. sub_009E is run by reset, kpg_monitor,
;@ 0x170A text ; CLONE mode (118Dh) and the application's restarts (9862h; 80BAh enters
;@ 0x170A text ; at 00A4h); nothing else writes FF0Eh or FF7Ah.
;@ 0x170A text ;
;@ 0x170A text ; What the link needs, for any code that uses it:
;@ 0x170A text ;   * EXIF0 unmasked and interrupts enabled; CRF11 is unmasked by
;@ 0x170A text ;     exif0_handler for each transfer. 1F12h masks both.
;@ 0x170A text ;   * The handlers reached: through this ROM's vectors (CCW TPF clear),
;@ 0x170A text ;     or through vectors at 8004h and 8020h that lead to 1F30h and 1F33h.
;@ 0x170A text ;   * Interrupts not held off for long: a late tick shifts a bit.
;@ 0x170A text ;   * P0.4 and P2.1 left alone, and FE25h-FE27h, FE2Ch, FEBCh-FEBEh,
;@ 0x170A text ;     FED9h bits 3-7, FEDAh bits 1-2 and FEDDh bits 0-2 written only as
;@ 0x170A text ;     the application does. crf11_handler uses register bank 1 (RAM
;@ 0x170A text ;     FEE0h-FEEFh) when the interrupted code is in bank 0.
;@ 0x170A text ;   * A send before the previous one ends waits in panel_tx_start; if no
;@ 0x170A text ;     transfer ever starts, it waits for ever, kicking the watchdog.
;@ 0x170A text ; ============================================================================
;@ 0x170A text ;
;@ 0x170A text ; [FW] panel_rx_recover (1F2Ah): if the panel's line is low and no
;@ 0x170A text ; transfer is running, the start edge came while EXIF0 could not be taken;
;@ 0x170A text ; set EXIF0's request flag so exif0_handler runs now. The mask ROM's own
;@ 0x170A text ; wait loops call it; the AB00 application never does.
;@ 0x170A label panel_rx_recover
;@ 0x170A ; a transfer is running: nothing to do
;@ 0x170D ; the panel's line is high: nothing waiting
;@ 0x1710 ; low, no transfer: the edge was missed; raise EXIF0
;@ 0x1713 ; return
;@ 0x1714 text 
;@ 0x1714 text ; ------------------------------------------------------------
;@ 0x1714 text ; [FW] panel_tx_start (1F2Dh): send A to the front panel. Waits for the
;@ 0x1714 text ; link to be free, stores the byte and pulls P0.4 low to ask the panel for
;@ 0x1714 text ; a transfer; the interrupts do the rest. Returns before the byte is sent.
;@ 0x1714 text ; Received bytes arrive in the queue, not in A.
;@ 0x1714 label panel_tx_start
;@ 0x1714 ; kick the watchdog
;@ 0x171C ; link busy: wait (no time limit)
;@ 0x171F ; the byte to send
;@ 0x1721 ; link busy
;@ 0x1723 ; P0.4 low: ask the panel for a transfer
;@ 0x1725 ; the interrupts send it
;@ 0x1726 text 
;@ 0x1726 text ; ------------------------------------------------------------
;@ 0x1726 text ; CRF11 interrupt handler
;@ 0x1726 text ; ------------------------------------------------------------
;@ 0x1726 text ; [FW] One tick of a panel transfer, twice per bit (see the FRONT PANEL LINK
;@ 0x1726 text ; notes at 170Ah). Ticks alternate: read the panel's bit from P2.1, then put
;@ 0x1726 text ; our next bit on P0.4. The first tick reads the panel's start bit, which
;@ 0x1726 text ; drops out of panel_rx_byte after nine reads. Then four ticks with P0.4
;@ 0x1726 text ; high (the stop bits) and two more; the last masks CRF11, files the byte
;@ 0x1726 text ; and frees the link.
;@ 0x1726 label crf11_handler
;@ 0x1726 ; let other interrupts in
;@ 0x1727 ; register bank 1, so A is not the caller's A
;@ 0x1729 ; data done: stop bits
;@ 0x172C ; this tick reads the panel's bit
;@ 0x172F ; this tick sends: the bits still to go
;@ 0x1731 ; lowest bit into CY
;@ 0x1733 ; onto the line
;@ 0x1736 ; keep the rest
;@ 0x1738 ; next tick reads
;@ 0x173A ; return
;@ 0x173C ; CY = the panel's bit
;@ 0x173F ; the bits so far
;@ 0x1741 ; CY in at bit 7, the rest move down
;@ 0x1743 ; keep them
;@ 0x1745 ; next tick sends
;@ 0x1747 ; one bit less to read
;@ 0x1749 ; more to come: return
;@ 0x174B ; start bit and 8 data bits read: stop bits
;@ 0x174D ; four ticks of stop: two bit times
;@ 0x1750 ; stop not done yet
;@ 0x1752 ; drop the edges the panel's data bits made
;@ 0x1755 ; return
;@ 0x1757 ; stop bits done?
;@ 0x175A ; no: our line high, the stop bit
;@ 0x175C ; one tick less
;@ 0x175E ; more to come: return
;@ 0x1760 ; stop bits done
;@ 0x1762 ; one tick more before the end
;@ 0x1764 ; return
;@ 0x1766 ; waited the extra tick?
;@ 0x1769 ; no: wait it now
;@ 0x176B ; return
;@ 0x176D ; transfer over: mask CRF11
;@ 0x1770 ; send "nothing" next unless a byte is queued
;@ 0x1773 ; unmask EXIF0 for the next start edge
;@ 0x1776 ; did the panel send anything?
;@ 0x1779 ; FFh: no
;@ 0x177B ; queue empty: this byte goes to the front
;@ 0x177F ; else behind the others:
;@ 0x1781 ; slot 1 taken?
;@ 0x1784 ; no: slot 1
;@ 0x1786 ; the byte
;@ 0x1788 ; done
;@ 0x178A ; slot 2 taken?
;@ 0x178D ; no: slot 2
;@ 0x178F ; the byte
;@ 0x1791 ; done
;@ 0x1793 ; slot 3, taken or not
;@ 0x1795 ; the byte
;@ 0x1797 ; done
;@ 0x1799 ; the queue's front holds a byte
;@ 0x179B ; the byte
;@ 0x179E ; link free
;@ 0x17A0 ; transfer over
;@ 0x17A2 ; return
;@ 0x17A3 text 
;@ 0x17A3 text ; ------------------------------------------------------------
;@ 0x17A3 text ; EXIF0 interrupt handler
;@ 0x17A3 text ; ------------------------------------------------------------
;@ 0x17A3 text ; [FW] The panel's start edge on P2.1 begins a transfer: our start bit,
;@ 0x17A3 text ; the counter restarted, CRF11 unmasked. The same whether we asked for the
;@ 0x17A3 text ; transfer (P0.4 already low) or the panel has something to say.
;@ 0x17A3 label exif0_handler
;@ 0x17A3 ; let other interrupts in
;@ 0x17A4 ; mask EXIF0: the data bits make edges too
;@ 0x17A7 ; a transfer is running: ignore the edge
;@ 0x17AA ; our start bit (already low if we asked)
;@ 0x17AC ; UDC1 = 0: first tick half a bit from here
;@ 0x17B0 ; data, not stop bits
;@ 0x17B2 ; read 9 bits: start bit and 8 data bits
;@ 0x17B5 ; drop any old tick
;@ 0x17B8 ; unmask CRF11: the ticks begin
;@ 0x17BB ; a transfer is running
;@ 0x17BD ; link busy, whoever started it
;@ 0x17BF ; the first tick reads the start bit
;@ 0x17C1 ; return
;@ 0x17C2 text 
;@ 0x17C2 text ; ------------------------------------------------------------
;@ 0x17DC text 
;@ 0x17DC text ; ------------------------------------------------------------
;@ 0x17E5 text 
;@ 0x17E5 text ; ------------------------------------------------------------
;@ 0x1803 data byte
;@ 0x1C00 data word/2
;@ 0x1C00 text 
;@ 0x1C00 text ; ------------------------------------------------------------
;@ 0x1C00 text ; [RE] Table of 16-bit words, each stored twice in a row.  Not referenced from
;@ 0x1C00 text ; the mask ROM.  [OPEN] purpose unknown; the firmware may use it.
;@ 0x1C00 label unclassified_1
;@ 0x1C50 data byte
;@ 0x1D00 data string/4
;@ 0x1D00 text 
;@ 0x1D00 text ; ------------------------------------------------------------
;@ 0x1D00 text ; [RE] Eight 4-character names: SQL POW DEV BAL FQT FDQT FID FREQ.  Not
;@ 0x1D00 text ; referenced from the mask ROM.  [OPEN] adjustment-mode item names, probably
;@ 0x1D00 text ; shown by the firmware with panel_text4.
;@ 0x1D00 label adjust_item_names
;@ 0x1D20 data byte
;@ 0x1E00 data string/8
;@ 0x1E00 text 
;@ 0x1E00 text ; ------------------------------------------------------------
;@ 0x1E00 text ; [RE] Panel messages, 8 characters each (7 for the four CP entries at 1E38h).
;@ 0x1E00 text ; Shown by loading HL with a message address and calling panel_text8.  The
;@ 0x1E00 text ; mask ROM itself uses PROGRAM, PROGRAM_, UNPROG, UNPROG_, ERROR, END and
;@ 0x1E00 text ; CLONE; TUNING, 900MHZ ... AUTO TEL, FIN are not referenced here and may be
;@ 0x1E00 text ; used by the firmware.  Previously listed as unclassified data.
;@ 0x1E00 label display_messages
;@ 0x1E3F break
;@ 0x1E46 break
;@ 0x1E4D break
;@ 0x1E54 break
;@ 0x1E5C break
;@ 0x1E64 break
;@ 0x1E6C break
;@ 0x1E74 break
;@ 0x1E7C break
;@ 0x1E84 break
;@ 0x1E8C break
;@ 0x1E94 break
;@ 0x1E9C break
;@ 0x1EA4 data byte
;@ 0x1F00 code
;@ 0x1F00 text 
;@ 0x1F00 text ; ------------------------------------------------------------
;@ 0x1F00 text ; Trampoline table (1F00H-1F4DH): the mask ROM's public entry points
;@ 0x1F00 text ; ------------------------------------------------------------
;@ 0x1F00 label trampoline_kpg_monitor
;@ 0x1F03 label trampoline_bin16_to_bcd4
;@ 0x1F06 label trampoline_bcd_add_signed
;@ 0x1F09 label trampoline_bcd_sub_signed
;@ 0x1F0C label trampoline_clear_resident_ram
;@ 0x1F0F label trampoline_gpio_init
;@ 0x1F12 label trampoline_irq_periph_init
;@ 0x1F15 label trampoline_model_rf_detect
;@ 0x1F18 label trampoline_eeprom_page_write
;@ 0x1F1B label trampoline_eeprom_random_read
;@ 0x1F1E label trampoline_eeprom_byte_write
;@ 0x1F21 label trampoline_eeprom_seq_read
;@ 0x1F24 label trampoline_dac_write
;@ 0x1F27 label trampoline_outexp_commit
;@ 0x1F2A label trampoline_panel_rx_recover
;@ 0x1F2D label trampoline_panel_tx_queue
;@ 0x1F30 label trampoline_panel_inte0_isr
;@ 0x1F33 label trampoline_panel_timed_isr
;@ 0x1F36 label trampoline_flash_write_64
;@ 0x1F39 label trampoline_clone_mode
;@ 0x1F3C label trampoline_flash_sdp_disable
;@ 0x1F3F label trampoline_flash_page_prefix
;@ 0x1F42 label trampoline_eeprom_byte_write_coop
;@ 0x1F45 label trampoline_eeprom_random_read_coop
;@ 0x1F48 label trampoline_flash_page_writer
;@ 0x1F4B label trampoline_model_id_field
;@ 0x1F4E data byte
