<!-- Replacement firmware: bring-up, open items, design rules -->
Part of the [TK-x4x hardware reference](README.md) · previous: [Resident mask ROM services and interrupts](Resident_ROM_and_Interrupts.md) · next: [Sources and revision history](Sources_and_History.md)

# 21. Recommended first replacement-firmware bring-up sequence

This is a conservative project plan, not a claim that every step exactly
matches stock initialization order. `[RE]`

## Stage A: harmless boot

1. Place valid `KENWOOD` signature at E580.
2. Preserve/reconstruct external vector/CALLT infrastructure, and set
   `CCW` `TPF` (`FF4Eh` bit 1) only once it is complete -- or leave `TPF`
   clear and use no CALLT (section 2.3).
3. Implement the 8080/8083/8086/8089 entry contract.
4. Call `1F0C` to clear resident working RAM.
5. Call `1F0F` for known-safe GPIO baseline.
6. Call `1F12` for known-safe interrupt/peripheral baseline.
7. Decide and implement a watchdog policy: either service `WDM` (section
   19.3) periodically from here on, or deliberately disable it -- do not
   leave it unaddressed.
8. Keep `P17/KEY` inactive.
9. Provide a callable routine at 8235 if cooperative ROM services might run.
10. Prove execution with a harmless panel/output indication.

**Debug console note:** step 10's "harmless output indication" does not have
to go through the panel. On a development radio with the KCT-19/accessory UART
path already installed or intentionally brought out, the on-chip UART
(`P24/TXD`/`P25/RXD`, section 12.4) is the cleanest debug console: use the
mask ROM's proven `SCC`/`BRG`/`SCM` setup and the `0x8178`/`0x818a`-style
byte primitives as a template. **Do not make this a requirement for ordinary
radios.** For no-modification testing on stock hardware, retain a console or
service path through the externally available programming/microphone interface.

## Stage B: identify hardware and preserve calibration

1. Call `1F15` and decode model/RF-split flags.
2. If the flag combination is impossible/unsupported, permanently inhibit TX.
3. Dump/read AT24C02 calibration.
4. Initialize DAC channels only from known calibration values.
5. Establish a known-safe FE28 4094 shadow state.

## Stage C: receive only

1. Select RX VCO state with `STR`.
2. Program PLL R/C/A from known stock values/math.
3. Verify lock indication.
4. Implement P30 noise-pulse measurement.
5. Enable the resident panel vectors (the checklist in
   [Front Panel Link](Front_Panel_Link.md), section 8), receive events through the FE2C queue,
   prove `0x02` + eight-character text output, and bench-test the stock-observed
   annunciator command families before assigning final symbolic names.

## Stage D: transmit under instrumentation

1. Use a dummy load/service monitor/spectrum analyzer.
2. Drop KEY before every frequency/VCO change.
3. Load TX VCO/PLL/calibration state.
4. Verify lock.
5. Assert KEY only for short controlled tests.
6. Confirm power, frequency, deviation, spurious behavior, and immediate de-key
   on unlock.

---

# 22. Open items that still block a feature-complete replacement firmware

These are the highest-value remaining reverse-engineering tasks:

1. **M62363 channel map.** Channels 2/3/6 are now confirmed (EEPROM
   `0xF4`/`0xF6`/`0xEC`), channel 5 is a strong candidate for volume, and
   4/7 have a traced-but-not-fully-resolved EEPROM/RAM dual source. See
   section 7.1.1. Remaining: bench-confirm channel 5, resolve 4 vs 7, find
   any use of channels 1 and 8.
2. **4094 AC/TR/STT/RLO semantics and polarity.** Net names are known, and
   firmware tracing now shows how the bits relate to each other (TR/STT
   complementary, AC+TR/STT a coupled triple, RLO config-gated) -- see
   section 8.2.1. The physical/electrical meaning of each state is still
   unconfirmed.
3. **Extended panel dictionary.** The transport is now fully understood
   ([Front Panel Link](Front_Panel_Link.md)), as are basic receive queue handling, ordinary
   text output (`0x02` then 4 or 8 characters), and the `0xB3` + number
   beep sequences (which sounds, still open). The icon commands
   (on/off/flash for all nine icons, `0x96`/`0x97` = TX), the whole-display
   bytes and two built-in strings are bench-confirmed (section 11.10).
   The keys (press/release, no repeat), the beep sounds, `0x84`/`0x87` and
   the character set are bench-confirmed too (sections 11.8, 11.10, 11.11).
   Remaining: the panel's `0xAA` after beep 07, and any
   partial-display/cursor behavior. Raw 14-segment
   access is optional unless arbitrary custom glyphs are desired.
4. **P32 lock polarity/peripheral behavior.** Firmware-logic polarity is now
   resolved (P32=1 = locked/proceed, section 6.5); only the electrical
   polarity at the physical pin remains to be bench-confirmed.
5. **P30 squelch algorithm.** Not in the mask ROM: no register reads, and
   its `CI0` counter interrupts go to a bare `RETI` (section 14.1). Next:
   trace the stock application's own vectors and `CI0` use.
6. **P15/P22/P23 normal-operation mode switching.** Separate mic/programming/
   clone semantics cleanly.
7. **P35/DATA2.** Its alternate function is enabled at boot alongside five
   other pins (section 4), but nothing was found using it afterward. Low
   priority unless a specific feature is suspected to need it.
8. **Transmit/RX analog sequencing.** Confirm exact FE28 + DAC + PLL order and
   settling delays used around RX/TX transitions.
9. **Calibration EEPROM format.** A first partial map now exists (section
   9.4.1: bytes `0xEC`/`0xF4`/`0xF6` for DAC channels 6/2/3, plus a
   separate service-display access pattern over nearby offsets that isn't
   yet reconciled with the operational one). Continue naming fields and
   confirm the two access patterns' relationship on the bench.
10. **`TMF1` sourcing nuance.** The datasheet's timer block diagram (Fig. 4)
    shows `TMF1` coming out of a "Select" block fed by both `TM0` and `TM1`,
    not simply as `TM1`'s direct flag output (unlike `TMF0`/`TM0` and
    `TMF2`/`TM1`, which are wired directly). Confirm whether `TMC0`'s mode
    bits affect what feeds `TMF1`, or whether under this project's
    configuration (`TM0` in normal, non-"two one-shot" mode) `TMF1` is
    simply `TM1`'s flag as assumed throughout section 19 and elsewhere in
    this document. **Answered (user's manual, section 4.5):** with timer 0 in
    interval mode, `TM1`'s underflow sets both `TMF1` and `TMF2` (and `TM0`'s
    sets `TMF0`); only timer 0's one-shot mode makes `MD0`'s underflow set
    `TMF1` instead. The ROMs use timer 1's period for bit timing, which fits
    interval mode.
11. **`WDM`'s interval-select bit encoding.** Confirmed the watchdog can be
    disabled by software and is in fact continuously serviced in real
    firmware (section 19.3), and that the operative value is `0x86`. Not yet
    confirmed which bits of `0x86` select which of the four 5.5-349.3 ms
    intervals, or which bit is the enable/disable bit. **Answered:** see
    section 19.3 (`0x86` = run, 349.5 ms; the `0x00`/`0x86` pair avoids the
    early-clear trap).
12. **Hardware-UART accessory/data-terminal protocol.** The on-chip UART's
    `'C'`/`'O'` command dispatcher (section 12.4, external firmware
    `0xb1d3`-`0xb22c`) is confirmed to exist and to ACK/NAK over the wire.
    It should not be conflated with the normal PC clone/programming path,
    which uses the separate `P14`/`P15`/`P22`/`P23` interface. Still open:
    the actual accessory/MDT meaning of `'C'`/`'O'`, the `0xb333` validation
    helper, the exact value written into `SCC` after each ACK, and any other
    command bytes. **Mostly answered (section 12.4):** the helper reads a
    speed code, `SCC` becomes `5 - code`, `'O'` shows `PROGRAM` and `'S'`
    starts a read/write session; the baud formula is now known. What the
    accessory side uses this for is still open. Earlier note: the
    mask ROM's own values (`SCC=0x04`, `BRG=0x4E`, `SCM=0xCD`) are recorded
    exactly and should be reused rather than guessed at until this is
    resolved.

---

# 23. Design rules for replacement firmware

1. **Use the resident mask ROM as a HAL where it is already understood.**
2. **Never key RF while changing PLL/VCO/calibration state.**
3. **Maintain complete shadow state for the 4094.**
4. **Do not invent DAC calibration values.**
5. **Back up the AT24C02 before writes.**
6. **Use exact model detection from 1F15 rather than assumptions.**
7. **Keep shared P01/P02 peripherals deselected while changing the bus.**
8. **Preserve the resident recovery path and E580 signature contract.**
9. **Prefer resident flash programming routines for self-update.**
10. **Mark unknown behavior as `[OPEN]` until it is traced or bench-tested.**
11. **Re-mask Timer1's interrupt-control register (`TMIC1`, `FFD0H`)
    explicitly before ever calling `EI`, unless a real ISR is installed at
    vector `0010H`.** The mask-ROM baseline (`1F12`) leaves it unmasked and
    at highest priority by design -- it is the only one of 15 possible
    maskable interrupt sources left live after boot. See section 19.2.
12. **Unmask `EXIC0` (`FFC8H`) deliberately before relying on panel-link
    interrupts.** `1F12`'s baseline masks it along with every other source;
    the stock application must unmask it itself during panel bring-up, and
    replacement firmware must do the same. See section 19.1/19.2.
13. **Service or deliberately disable the watchdog (`WDM`, `FF42H`) --
    never leave it unaddressed.** Both stock ROMs kick it continuously
    (`MOV WDM,#0x00` then `MOV WDM,#0x86`) throughout normal execution; going
    quiet on it resets the CPU via the nonmaskable vector `0x000A` within
    5.5-349.3 ms. See section 19.3.

The goal is not merely to make the radio do something. The goal is to build a
hardware abstraction whose behavior is known well enough that later VFO,
memories, scanning, PL/DCS, alignment, and programming code can be written
without rediscovering board-level details each time.

The current state is sufficient to begin a conservative replacement firmware
that boots, identifies the radio, preserves calibration, talks to the front
panel, displays text, receives key events, programs receive frequency, and only
later enables controlled transmit. A feature-complete replacement still needs
the remaining DAC/4094 semantics, full panel dictionary, and measured
RX/TX/squelch sequencing, but the basic firmware architecture is no longer
blocked by unknown button or LCD wiring.

---

# 24. Unknown/unmapped hardware I/O running log

Use this table when disassembly or bench work reveals a pin/SFR toggle whose
meaning is not yet understood. Once a function is resolved, move it into the
proper section instead of leaving stale speculation here.

| Pin / SFR.bit | Observed context | Suspected function | Resolved to | Status |
|---|---|---|---|---|
| `P35/PWM1 / DATA2` | `PMC3.5` enabled at boot as part of a 6-pin bulk config write in `1F0F`; no code references `0xFF17` (PWM1) or `0xFF03.5` afterward | option/data function | - | `[OPEN]` (searched, see section 4) |
| `4094 AC` | Q1 / FE28.1; clears together with TR at 4 sites when STT sets | audio/control, coupled with TR/STT | - | `[OPEN]` (relationship traced, see 8.2.1) |
| `4094 TR` | Q2 / FE28.2; complementary with STT at every site found | T/R-related control | - | `[OPEN]` (relationship traced, see 8.2.1) |
| `4094 STT` | Q3 / FE28.3; complementary with TR at every site found | control/state | - | `[OPEN]` (relationship traced, see 8.2.1) |
| `4094 RLO` | Q4 / FE28.4; gated by EEPROM `0x28` bit0 + ROM table `0xE66B` bit1 + two port bits, not a simple toggle | config/accessory-gated control | - | `[OPEN]` (gating traced, see 8.2.1) |

When adding a new entry, record the exact code address, execution context
(interrupt, polling loop, startup, RX/TX transition, etc.), observed logic
level, and any correlated schematic net.

---

# 25. Recommended replacement-firmware architecture

Do not build the application as one monolithic loop around magic addresses.

A useful split is:

```text
platform/
    cpu_78k3
    maskrom_abi
    interrupts
    timers

drivers/
    panel
    pll_mc145190
    dac_m62363
    expander_4094
    eeprom_at24c02
    flash_at29c256
    accessory_io
    audio_signaling

radio/
    rx
    tx
    squelch
    pl_dcs
    scan
    vfo
    memories
    repeater_offsets
    power_control

ui/
    keys
    display
    menus
    beeps
```

Keep three concepts separate:

1. physical hardware names;
2. Kenwood commercial feature names;
3. replacement/ham-radio policy.

For example, a physical AUX key should first become `KEY_AUX`; the application
can bind that event to monitor, power, reverse, or another function. Likewise,
the physical `HOR` sink output should be represented as an output capability,
with "horn alert" being only one possible policy.

---

Part of the [TK-x4x hardware reference](README.md) · previous: [Resident mask ROM services and interrupts](Resident_ROM_and_Interrupts.md) · next: [Sources and revision history](Sources_and_History.md)
