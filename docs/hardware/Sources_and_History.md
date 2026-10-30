<!-- Sources and revision history -->
Part of the [TK-x4x hardware reference](README.md) · previous: [Replacement firmware: bring-up, open items, design rules](Replacement_Firmware_Guide.md)

# 26. Source set used for this consolidated reference

This document merges project evidence from:

- the TK-840/TK-940/TK-941 service/schematic work;
- the µPD78312/78K/III CPU documentation used by the project;
- MC145190, M62363, AT24C02, AT29C256, and 4094-family datasheets;
- the common `0000-1FFF` mask-ROM dump and decoded `1Fxx` resident services;
- the analyzed TK-840 external firmware (`8000-FDFF`) and disassembly;
- the consolidated hardware reference and the project's documented bench probes;
- current panel/button/display reverse engineering through 2026-09-29,
  including stock-firmware annunciator command tracing;
- the NEC 78K/III Series Instructions User's Manual (covers the
  `µPD78312A` subseries by name; used for the CPU-architecture/instruction
  reference, notably the protected `MOV WDM,#byte`/`MOV STBC,#byte` form);
- exhaustive firmware/mask-ROM tracing for DAC channel calls, `0xFE28`
  bit context, `P32` polarity, `P30`/`P35` register references, EEPROM
  read call sites, and `WDM`/`STBC` write sites (2026-09-29);
- the datasheet's Table 1 Special Function Register list (pages 7-15/7-17 of
  the source PDF) for the serial-port/UART register addresses, cross-checked
  against direct mask-ROM/external-firmware disassembly for
  `SCM`/`SCC`/`BRG`/`RXB`/`TXB`, and against the existing, previously
  validated external CALLT table (section 17.1) to resolve `callt [0x0017]`/
  `callt [0x0019]` (2026-09-29).

Known retained analysis identifiers include:

```text
internal mask ROM 0000-1FFF:
    SHA-256 0572a52ad964ff67afc25feb0dbde5e7ad279cc5d00c47cc5ba1f9b2ae242543

TK-840 executable image documented by the hardware-interface notes:
    size     0x6600 bytes
    byte sum 0x8355
    SHA-256  7b061dc79ad69475484d68340edd872bd5e1a8f67efeb27bffbe3812a0ee7258

full external 8000-FDFF companion dump used in mask-API caller analysis:
    SHA-256 f5990f3454d7c6044ab83a6e2dd81df20cf99c57e495bc61e10d74d95bb1aacd
```

These hashes describe different retained artifacts/ranges; do not compare them
as though they were hashes of the same byte range.

Evidence tags in the body should remain the authoritative indication of how
strongly a specific fact is supported.

---

# 27. Research milestones

The dates below record technical discoveries and bench validation, not earlier public software releases.

## 2026-09-27

Initial hardware-communication guide written alongside Stage-1 test firmware.
At that point the C SFR idioms and PLL work were the strongest portions.
PTT, panel application bytes, volume control, and squelch still contained large
`[OPEN]` areas.

## 2026-09-29

Documented and cross-checked the following hardware findings:

- full boot/memory/vector contract and fixed application entries;
- baseline mask-ROM GPIO state;
- complete current main-CPU I/O map;
- shared P01/P02 bus rules;
- decoded 4094 FE28/Q-output map;
- model/RF-split analog thresholds;
- corrected P30 squelch-noise model;
- AT24C02 and AT29C256 resident programming details;
- accessory connector/horn/ignition/hook capabilities;
- current resident API and CALLT table;
- substantially upgraded panel section.

The panel is no longer treated as a fundamental replacement-firmware blocker.
The resident transport is understood, the receive queue can be consumed, and
ordinary eight-character display output has a strong known byte sequence. The
remaining panel work is the extended command/event dictionary.

## 2026-09-29 (addendum: interrupt architecture and internal RAM map)

Added new section 1.4 and new section 19, both from the NEC μPD7831xA/78P31xA
datasheet (Table 1/2, Figures 1/2/4/7/8) cross-checked against mask-ROM
disassembly of `1F0C` and `1F12`:

- **Internal RAM layout (1.4):** confirmed exact boundaries -- `FE00-FE1F`
  stack, `FE20-FE7F` genuine free RAM, `FE80-FEEF` unused-register-bank
  storage repurposed as scratch (explains why `FEDA`/`FED9`/`FEBC-FEBE` sit
  where they do), `FEF0-FEFF` the live register bank (resolves the previous
  "purpose unknown" note on why `1F0C` stops at `FEEF`). Also: Macroservice
  channel pointers share storage with Register Banks 0/1.
- **Full interrupt vector table (19.1):** all 15 maskable sources plus
  `BRK`/`NMI`/`WDT`/`RESET`, with real vector addresses and IC-register
  addresses -- previously only referenced piecemeal (`TMIC1` alone).
  `WDT` is confirmed nonmaskable (no `xxIC` register at all).
- **`1F12`'s baseline state, byte-exact (19.2):** all 15 registers it writes
  now identified by name; 14 are masked (13 to a fresh masked state, plus
  `SEIC`/`SRIC`/`STIC`/`ADIC`/`TBIC` are unchanged from their `47H` reset
  default), and exactly one -- `TMIC1`/`TMF1`/Timer1, this project's own
  baud-rate timer -- is left unmasked at maximum priority. New design rules
  (11, 12) and open items (10, 11) added as a direct consequence.

This closes the "what are the other 14 interrupt-control registers 1F12
touches" question raised in review, and turns the RAM-map notes scattered
across sections 4/11 into one consolidated, datasheet-grounded reference.

## 2026-09-29 (addendum: panel annunciator command tracing)

Updated the panel/display model from further stock-firmware tracing and direct
inspection of the TK-840 display glass:

- corrected the main display from the older 13-segment description to eight
  **14-segment** character positions;
- recorded the physical annunciators as `TX`, `BUSY`, `CALL`, `S` in an oval,
  `TA`, `OPT`, `AUX`, and a telephone symbol;
- documented that no raw segment-bitmap interface is currently visible from the
  main CPU side, and that replacement firmware can continue treating the
  µPD75308 as a black-box smart panel;
- identified `0x96` / `0x97` as TX annunciator on/off with very high confidence
  from their placement immediately around RF KEY assertion/deassertion;
- isolated candidate control families `99/9A`, `9C/9D/9E`, `A9/AA`, `90/91`,
  `93/94/95`, `A0/A1`, `A3/A4`, and `A7/A8` for fast bench identification;
- changed the recommended panel work from broad protocol discovery to a direct
  stock-observed command sweep plus normal key-event capture.

The exact symbolic mapping of every non-TX command remains deliberately
provisional until visible bench confirmation.

## 2026-09-29 (addendum: system clock, watchdog, DAC/4094/EEPROM tracing, P32)

A broad static-analysis pass targeting as many remaining open items as
possible without bench access, per explicit request to wrap up hardware docs
before returning to compiler work. Sources: continued NEC datasheet reading,
the 78K/III Instructions User's Manual, and exhaustive firmware/mask-ROM
tracing (new section 26 entries).

- **System clock resolved (new 1.5):** `fCLK = 6 MHz` confirmed (12 MHz
  crystal, `STBC` bit5=0, the only `STBC` value either ROM ever uses). This
  closes the long-open "Timer1 prescaler math" item: Stage-1's actual TX bit
  rate computes to ~9615 Hz against a 9600 baud target (0.16% error, normal
  UART quantization) -- the timer math was never the bug. Investigation of
  the earlier serial glitch should look at per-bit loop timing jitter
  instead. Also confirmed `vasm` already encodes the protected
  `MOV WDM`/`MOV STBC` form correctly (cross-checked against all 82 real
  instances in both ROMs).
- **Watchdog resolved (19.3):** datasheet prose confirms it's
  software-disableable with four 5.5-349.3 ms intervals; firmware tracing
  shows it is in fact continuously serviced (`WDM=0x00` then `0x86`,
  82+36 sites) throughout both ROMs. New design rule 13: replacement
  firmware must service or deliberately disable it, or risk an NMI-class
  reset at `0x000A`.
- **P32 lock polarity resolved (6.5):** firmware-logic polarity confirmed
  (P32=1 = locked/proceed) from the two real wait-loop sites that test it,
  both immediately after PLL reprogramming. Electrical polarity at the pin
  itself remains a bench item.
- **DAC channel map, first real data (new 7.1.1):** channels 2/3/6 confirmed
  against specific EEPROM calibration bytes (`0xF4`/`0xF6`/`0xEC`); channel 5
  identified as a strong volume candidate (32-entry lookup table indexed by
  a UI-driven RAM counter, not EEPROM-sourced); channels 4/7 traced to a
  EEPROM/RAM dual-source pattern gated by an unconfirmed flag; channels 1
  and 8 still have no confirmed use.
- **4094 bit relationships traced (new 8.2.1):** `TR`/`STT` shown
  complementary at every site found; `AC`+`TR`/`STT` form a coupled
  three-bit group across two mutually exclusive firmware states (not yet
  named); `RLO` shown to be gated by an EEPROM byte + a ROM table bit + two
  port bits, not a simple toggle.
- **EEPROM partial field map (new 9.4.1):** ties the DAC-channel bytes above
  into a first calibration map, and separately documents a service/display
  access pattern over nearby offsets whose relationship to the operational
  bytes is flagged as unconfirmed rather than assumed.
- **P30 squelch: searched and confirmed not found** (14.1) at the register
  level in either ROM -- recorded explicitly so this isn't re-searched from
  scratch, with the real next step (trace the EXIF0/EXIF1/TMF0/TMF1 vector
  targets) named instead.
- **P35/DATA2:** confirmed its alternate function is enabled at boot as part
  of a bulk 6-pin config write, but no post-boot use was found anywhere;
  downgraded to low priority rather than left as a flat unknown.

Everything above that could not be nailed down is recorded as a narrower,
more specific open item than before (section 22) rather than left as a
broad unknown -- the goal this round was to convert vague `[OPEN]` markers
into either resolved facts or precisely-scoped remaining questions.

## 2026-09-29 (addendum: real on-chip hardware UART identified, new 12.4)

Triggered by the observation that the hardware UART is routed to the
optional accessory/KCT-19 path rather than the normal external programming
connector. It is therefore highly useful on a suitably equipped development
radio, but not universally accessible without optional hardware or modification.
Verifying and extending that observation against the datasheet and both ROMs
directly confirmed:

- **The UART is a real, separate on-chip peripheral, not related to
  Timer1.** Datasheet Table 1 gives its own register set with a dedicated
  baud-rate generator: `SCM` (`FF50H`), `SCC` (`FF52H`), `BRG` (`FF53H`),
  `RXB` (`FF56H`), `TXB` (`FF57H`).
- **The mask ROM programs it unconditionally at reset** (`SCC=0x04`,
  `BRG=0x4E`, `SCM=0xCD`), in the same reset routine as the project's
  already-established single `STBC` write (section 1.5) -- directly
  verified against the mask-ROM binary's exact bytes, not just the
  disassembly text.
- **The external stock firmware has working, watchdog-safe byte-level
  send/receive primitives for it** at `0x8178`/`0x818a`, which resolve
  exactly to `callt [0x0017]`/`callt [0x0019]` in the project's own
  already-validated external CALLT table (section 17.1) -- a clean
  cross-check between two independently-derived pieces of evidence.
- **A real command dispatcher exists on this UART** (`0xb1d3`-`0xb22c`):
  it reads a byte, recognizes `'C'`/`'O'`, replies ACK (`0x06`) or NAK
  (`0x15`), and rewrites `SCC` afterward. Earlier text incorrectly labeled
  this as the PC clone/programming-cable protocol. Because the normal
  programming path is on `P14`/`P15`/`P22`/`P23` and this UART is on the
  accessory side, it is now documented only as an accessory/data-terminal
  protocol until the command meanings are actually traced.
- New section 12.4, an updated `P24`/`P25` I/O-map evidence tag, new `SCM`/
  `SCC`/`BRG`/`RXB`/`TXB` rows in section 20, a debug-console note in
  section 21 Stage A, and open item 12 (section 22) all follow from this.

For a development radio that already exposes the accessory UART, this is the
preferred debug console because it frees `P15` and avoids bit-bang timing
jitter. It is **not** the universal project debug/service interface: stock
radios without the accessory hardware still require the externally accessible
programming/microphone path for no-modification diagnostics and service.

## 2026-09-29 (addendum: review of the -8 revisions)

Reviewed the edits to section 12.4 (physical accessibility of the
accessory UART, and walking back the "PC clone protocol" identification to
an unproven "accessory/data-terminal protocol"), the Stage A debug-console
note, open item 12, and the revision history. All register-level and
disassembly evidence from the prior pass (`SCM`/`SCC`/`BRG`/`RXB`/`TXB`
addresses, the mask-ROM init sequence, the `0x8178`/`0x818a` primitives, the
`callt [0x0017]`/`callt [0x0019]` CALLT-table resolution, and the `0xb1d3`
command-dispatcher trace) is untouched by the edits and remains accurate.
The corrections themselves are outside what firmware/datasheet tracing can
confirm or refute -- the physical-accessibility claim carries a `[BENCH]`
tag this project has no independent way to check -- so they are accepted as
authoritative. Checked for, and found, no leftover reference anywhere else
in the document to the retracted "PC clone/programming-cable protocol"
identification. Added one cross-reference sentence to section 12.3's KCT-19
pin table pointing to 12.4's accessibility caveat, since a reader landing on
that table first would otherwise have no reason to suspect pins 14/15 are
different from the rest of the connector.

## 2026-10-14 (front panel link; vector/CALLT switch; NEC manuals)

- New companion document, `docs/hardware/Front_Panel_Link.md`: the panel link tick
  by tick, its timing (about 4,800 bit/s, 2.4 ms per transfer), the mask
  ROM's handlers and entries, how the stock TK-941 `AB00h` application hooks
  in, the byte sequences sent to the panel, and a replacement-firmware
  checklist with a way to tell why a send never completes.
- Section 2.3: `CCW` (`FF4Eh`) bit 1, `TPF`, moves the interrupt vectors and
  the CALLT table from the mask ROM to `8000h-807Fh` of the image; confirmed
  from the µPD78312A user's manual and datasheet, now in `docs/NEC/`.
- Sections 11.6 and 17.1: their application addresses are from the TK-840
  `8355h` image, not the TK-941 `AB00h` image.
- Section 11.9: `0x02` is also followed by only 4 characters, and after
  `0xB3` it is a beep number, not text. Section 11.10: the mask ROM's use of
  `0x96`/`0x97` as a checksum indicator.
- Section 19.1: the stock start-up's `EXIF0` unmask confirmed.
- Section 21 Stage A: set `TPF` only once the tables are complete.
- Section 22: item 3 updated; items 10-12 can now be settled from the NEC user's
  manual.
- `resident_api/tk840_maskrom_api_1f3x.txt`: `FF1Eh` is `UDC1`, and which
  vector table is used depends on `TPF`.

## 2026-10-15 (NEC user's manual: watchdog, timers, UART; programming sessions)

- Section 12.4: the hardware UART starts at 1200 baud, 8N2 (`SCC=04h`,
  `BRG=78`, `SCM=CDh`, and the manual's baud formula). The `'C'`/`'O'`
  dispatcher decoded: a speed code picks the new rate (`SCC = 5 - code`),
  `'O'` shows `PROGRAM`, and `'S'` starts the application's own read/write
  session at `80AAh`. Open item 12 mostly answered.
- Section 19.2: the `xxIC` bit layout, and `TMIC1 = 80H` corrected: it sets
  the `TMF1` request flag; `TMIC1`/`TMIC2` take the timer group's priority
  from `TMIC0`.
- Section 19.3: `WDM` bits (`86h` = run, `NMI` first, 349.5 ms) and the
  early-clear rule that explains the `00h`/`86h` kick pair. Open items 10
  and 11 answered.
- `docs/programming/Programming_Protocol.md`: what the radio does in the microphone-
  port session (the mask ROM's KPG monitor, entered on panel byte `A0h`;
  ACK at 1200, then 9600; watchdog-reset exit).
- `docs/hardware/Front_Panel_Link.md`: `INTM`, the audio-mute requirement for
  beeps, `A0h`, and the 32-byte stack limit.
- `annotations/maskrom/uPD78312A.inc`: every register in the
  data sheet's SFR table.
- `docs/programming/Programming_Protocol.md`: the application's tuning session
  (`BB63h`, `" TUNING "`), entered on panel bytes `55h A2h`; `A1h` erases
  the codeplug first. Its software serial port: transmit on `P1.5` at 1200
  or 9600, receive on `P2.2` at 9600. `A0h` (channel programming) also
  needs `55h` first in three of the four places.
- `docs/programming/Programming_Protocol.md`: the tuning session's command table,
  with every byte the radio sends in reply.
- `docs/hardware/Front_Panel_Link.md`: the microphone-jack lines are not the
  panel link; the mask ROM's `15C8h` as the simplest way to show 8
  characters.
- Section 11.10: the 2026-10-08 bench results: the icon on/off/flash table, the
  whole-display bytes `0x80`-`0x83`, `0xB0` clear, the built-in strings
  `0x85`/`0x86`, `0x8B` + 2 bytes, `0x8E` turning TX off, the `0xB3`
  lockups, and why the loader's PROGRAM screen flashes (`0x82`). Section
  11.11: why the bench beep test was silent.
- Section 11.8: the bench key table, press and release byte for each of
  the eight front-panel keys.
- Sections 11.10 and 11.11 (`benchtest`, 2026-10-16): the display's
  character set, the 13 beep sounds and where the stock code uses them,
  `0xB4` stopping a beep, `0x84`/`0x87` doing nothing, the panel's `0xAA`
  after beep 07, and `PROGRAM` on the mic jack confirmed to arrive as panel
  bytes `55h A0h`.
- Section 11.8: held keys send no repeat codes (bench, 2026-10-16).
- New `docs/programming/Mic_Port_Serial.md`: the microphone-port software serial
  port (the three stock implementations, timing, speed limits), the mask
  ROM firmware loader's 19200 raw-binary path (Scan key), and design notes
  for a faster channel-programming session and a codeplug CRC-16.

## 2026-10-16 (channel to synthesizer, traced in both stock images)

- New section 6.6: how a channel goes from its codeplug record to the
  synthesizer, the RAM it loads, the transmit paths, the dealer test
  frequencies, and what the Enhanced TX Patch changes and cannot reach
  (transmit below 851.000 MHz).
- Section 6.4: the TK-940/941 bases explained; `FEDAh.0` is the TK-940;
  stock AB00 transmits on the RX word less 45 or 39 MHz, or on it with
  Talk Around. The TK-840 channel transmit lock wait is `E2E0h`.
- Section 6.2: only the TK-840 sends C = `6Dh`/`6Ch`; the A/N writer's
  swapped register restore.
- Section 6.5: the two P32 tests described correctly (`9C21h` tests once).

## 2026-10-16 (the synthesizer, from both stock images)

- Section 6.2: the A/N word layout, traced in TK-840 8355 (`9BBCh`) and
  TK-940/941 AB00 (`9B53h`), the same code: N from bit 8, A in bits 0-5,
  bits 20-21 set. The layout this document gave before was wrong.
- Section 6.4: how each image tunes: TK-840 bases `5DC0h` (transmit) and
  `4FACh` (receive), the `6Dh`/1.5 ms/`6Ch` C-register sequence; the
  TK-940/941's 17-bit D and bases.
- Section 8.3: STR set = receive VCO, clear = transmit (TK-840 image).

## 2026-10-16 (compiler correction and UART bench results)

- Section 12.4: the hardware UART sends clean text at 1200-115200 bit/s
  (bench); the bench radio's CN3 tap; the earlier wrong results explained
  by the vbcc string bug (the earlier vbcc backend revision `2945d2e`).
- Section 1.5: the serial glitch was traced to that compiler issue.
- CLONE-mode entry, the front-panel key-byte encoding, the TA/"(S)"
  test procedure, the mask-ROM vector trace, and resident services
  `009E`/`0144` were documented in their respective reference sections.
- Names for the panel bytes (`include/panel_codes.h`) shown next to the
  hex in 11.8, 11.10 and 11.11.
- Section 4 and 4.1: the schematic's pin labels (Dave), and the mic jack
  wiring: `P15`/`P23` on the PTT line, `P14`/`P22` on the hanger line, so
  the mic-port serial output pulls PTT low on every 0 bit.

---

Part of the [TK-x4x hardware reference](README.md) · previous: [Replacement firmware: bring-up, open items, design rules](Replacement_Firmware_Guide.md)
