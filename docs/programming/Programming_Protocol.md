# TK-x4x Serial Programming Protocol

Technical reference for the Kenwood TK-x4x serial programming interface. Read transactions and ordinary codeplug writes were exercised on TK-840 hardware. Use [TKx4xToolkit](https://github.com/KY4WI/Kenwood_TK-x4x_Toolkit) for current radio programming.

For CPU address-space layout, flash, boot, recovery, and electrical details, see [TK-x4x Hardware Communication](../hardware/README.md). For low-level resident routines, see the [resident API research](../hardware/resident_api/README.md).

## Entering the ordinary programming session

The observed TK-840 serial exchange uses **8 data bits, no parity, 2 stop bits** at each speed:

1. Start at **1200 baud, 8N2**. PC sends the ASCII string `PROGRAM`.
2. Radio sends `06` (ACK).
3. Change the serial port to **9600 baud, 8N2**.
4. PC sends `02` (identification request).
5. Radio returns seven identification bytes; PC acknowledges with `06`.

### What the radio does (from the mask ROM and the TK-941 `AB00h` image)

The session above is run by the mask ROM's KPG monitor (entry `1F00h`),
not by the application. `[MASK][FW]`

1. **Entry.** The application never times a 1200-baud input on the
   programming lines: its receiver on `P2.2` is switched off (`FED7.1`
   clear) except inside its own tuning session, below. The request arrives
   from the front panel as panel byte `0xA0`, and the application branches
   to `1F00h`. Four places in `AB00h` look for it. Three of them take it
   only right after a `0x55` (which sets `FEDB.7`; any byte they don't
   handle clears it). `[FW]` The panel's own CPU listens for the PC's
   1200-baud `PROGRAM` and sends `55 A0`: with the `benchtest` probe
   (`tools/probes/`) showing panel bytes in hex, `PROGRAM` sent into the mic
   jack shows `55 A0`. `[BENCH]` (2026-10-16)
2. **Take-over.** The monitor disables interrupts, clears `CCW` `TPF` (so the
   mask ROM's own vectors are used), resets the stack and re-runs the whole
   start-up: SFRs, ports, resident RAM, interrupt controls, the output
   expander. The radio stops working as a radio from here on. `[MASK]`
3. **ACK at 1200, then 9600.** It sends `0x8E` and `"PROGRAM "` to the
   panel, sends `06` at **1200 baud** on `P1.5`, waits until it has gone, and
   switches to **9600 baud** for the rest of the session. Transmit is on
   `P1.5`, bit-timed by timer 1 (`MD1 = 0067h`, 104 µs); receive is on `P2.2`,
   whose falling edge (external interrupt 1) starts a byte that timer 0
   (`MD0 = 0067h`) then samples. 1200 baud uses `0340h` (833 µs). `[MASK]`
4. **Commands.** `02` (identification: six model-ID bytes from
   `FEC5h` down to `FEC0h`, then `Y` or `N` by `FEDE.4`, the state byte
   above), `03`, `04`, `R` and `W` as above, and `E`. `[MASK]`
5. **Exit.** After `E` the monitor sends `06` and then spins without
   servicing the watchdog, so the watchdog resets the radio after about
   350 ms and it restarts into the application. `[MASK]`

A replacement firmware gets the same programming support, with the stock
protocol, by doing what the stock application does: branch to `1F00h`
when the panel sends `0xA0` (after `0x55`).

### The application's tuning session (panel byte `A2h`)

The application has a second session on the same lines, in flash rather
than the mask ROM (`tuning_session`, `BB63h` in the TK-941 `AB00h` image).
`[FW]`

| Panel bytes | Names | What the application does |
|---|---|---|
| `55 A0` | `PANEL_REQ_PREFIX`, `PANEL_REQ_PROGRAM` | branch to the mask ROM KPG monitor: channel programming, display `"PROGRAM "` |
| `55 A2` | `PANEL_REQ_PREFIX`, `PANEL_REQ_TUNING` | `tuning_session`, display `" TUNING "` |
| `A1` | `PANEL_REQ_ERASE_TUNE` | **erase the whole codeplug** (`E600h-FDFFh` written with `FFh`), then `tuning_session`. Not guarded by `55h`. |

(The names are in [`include/panel_codes.h`](../../include/panel_codes.h).)

One of the four dispatchers (`9F3Bh`) takes `A0h` and `A2h` without the
`55h`. The DOS host `KPGPRN.EXE` explicitly sends ASCII `TUNING `
at 1200 baud, waits for ACK `06h`, then switches to 9600 baud. This
is the established PC-side tuning entry string, although the exact
front-panel decode remains untraced. `[DOS][FW][RE]`

What it does, in order: sets the interrupts up again as at start-up
(`TPF`, `EXIF0`), unmasks external interrupt 1 and timer 1, sets the audio
(expander `24h`, volume DAC), beeps (`beep_04`), sends `8Eh` and
`" TUNING "` to the panel, waits about half a second, sends `8Bh` with two
EEPROM values (`FDh`, `FEh`), turns its receiver on, and sends `06h` at
**1200 baud**. Everything after that is at **9600 baud**. `[FW]`

Unlike the KPG monitor it does not take the CPU over: its loop keeps
calling the application's periodic routines and checks for a command byte
between them, so the radio keeps working. It never returns; the radio
leaves it by being reset. `[FW]`

- **Receive:** `P2.2`; its falling edge (external interrupt 1, `isr_exif1`)
  starts a byte that timer 1 samples at `MD1 = 0068h` (105 µs, about 9524
  bit/s). 9600 only. The stop bit is not checked. `[FW]`
- **Transmit:** `P1.5`, `soft_serial_send_byte` (CALLT `09h`): 8 data bits,
  LSB first, one stop bit, timed by timer 1 at `MD1 = 0068h` (9600) when
  `FED7.5` is set or `0341h` (834 µs, 1200) when it is clear. It waits until
  the byte has gone, with interrupts on. `[FW]`
- **Commands:** single bytes below `50h`, some followed by more bytes;
  see the table below. Bytes without a handler are ignored and get no
  reply. `[FW]`

Both sessions send the same identification shape (six model bytes and
`Y`/`N`) to `02h`, so the PC can't tell them apart that way.

#### Tuning-session commands

The radio sends nothing unprompted: every byte it sends answers one from
the PC. All at 9600 baud after the entry `06h`. "Any byte" means the radio
waits for one byte and doesn't check it. Handler addresses are in the
TK-941 `AB00h` image. `[FW]` unless marked.

| PC sends | Radio sends | What it does |
|---|---|---|
| `02` | `M940B1Y` or `M941B1Y` (by model, `FEDA.0`); then reads a byte and answers `06` to `06`, `15` (NAK) to anything else | identification (`BCA7h`) |
| `05` | 10 bytes: EEPROM `F8h`, `F2h`, `F4h`, `F6h`, `F9h`, `FAh`, `FBh`, `F0h`, then (EEPROM `FDh` − 1, clamped to 0-9) and (EEPROM `FEh` − 1, clamped to 0-7); then `06`/`15` as for `02` | read the alignment settings (`BCE3h`); `[RE]` the tuning values |
| `08 0D` | `06`; later `06` again when `08 0E` arrives | **transmit** (`dealer_tx`) until `08 0E`; other commands are served while transmitting (`BD6Dh`) |
| `09 0D` / `09 0E` | `06` | panel byte `90h` / `91h`, `FE78.4` set / clear (`BD8Eh`) |
| `10 n` | 2 bytes: (`E681h+4n` AND `7Fh`), then `E680h+4n`; any byte; `06`. For an empty entry (`7FFFh`): `FF FF`, any byte, `15` | select tuning channel `n` (0-9; saved to EEPROM `FDh`, shown with `8Bh`) and return its 2-byte code from the codeplug table at `E680h` (`BDA5h`). `[OPEN]` the table's format |
| `11 n` | `06` | select adjustment item `n`+1 (1-8; saved to EEPROM `FEh`, panel `9Dh` and `8Bh`) (`BDF1h`) |
| `12 s`, `16 s` ... `1C s` | `06` | eight adjustment items. `s`: `0D` select (sets `FE24h` and mode `FEB8h`), `0E` done, `0A` up, `0B` down, `0F v` set to `v` (mostly a DAC write). `[OPEN]` which item each is (`BE13h`, `BE5Bh`-`BFE4h`) |
| `1D s` | `06`; for `0C` the volume (`00`-`1F`) first, then any byte, then `06` | volume (DAC 5): `0A` up, `0B` down, `0C` read, `0F v` set; up, down and set beep (`C02Ch`) |
| `2B` | `06` | **erase the whole 256-byte EEPROM**, then write the factory `F0h-FFh` block and select channel 1, item 1 (`C08Fh`) |
| `2C` | 10 bytes: EEPROM `E9h` down to `E0h`; any byte; `06` | read an EEPROM block (`C0B2h`) |
| `2D` + 10 bytes | `06` | write EEPROM `E9h` down to `E0h` (`C0C6h`) |
| `37` / `38` | nothing | display `"   OK   "` / `"   NG   "` (`C0D9h`, `C0F5h`) |
| `3B` / `3C` / `3D` | nothing | panel byte `80h` / `81h` / `8Eh` (`C111h`-`C119h`) |
| `45` | `06` | leave: jump to the reset address (`C11Dh`) |

Take care when testing: `2B` erases the radio's alignment values, `08 0D`
keys the transmitter, and panel byte `A1h` erases the codeplug before the
session starts.

#### Calibration backup: what tuning mode actually exposes

The `05h` and `2Ch` replies together provide **20 values, not 20 raw EEPROM
bytes**. Specifically: `2Ch` returns 10 unmodified bytes (`E9h` down to
`E0h`), and `05h` returns eight unmodified bytes (`F8h,F2h,F4h,F6h,F9h,
FAh,FBh,F0h`) plus two **transformed** values derived from `FDh` and `FEh`.
That is **18 exact EEPROM bytes and two clamped selection indices**. The
original `FDh` and `FEh` bytes cannot be reconstructed unambiguously if
outside the expected 1-10 / 1-8 ranges. Preserve the received order; neither
reply is a contiguous dump of all calibration addresses. `[FW][RE]`

**Known coverage gaps:** operational calibration reads in the external firmware
also use `ECh` (DAC 6), `EEh/EFh` (DAC 7) and `F1h` (a possible DAC 4
source). These are **not** returned by `05h` or `2Ch`. Their absence matters
more than the total byte count: a successful 20-value capture is not yet a
complete calibration backup. Other offsets may also matter; see the
[partial calibration field map](../hardware/DAC_Expander_EEPROM.md#941-partial-field-map-new-firmware-traced).

**Restore is not symmetric:** `2Dh` writes only `E9h..E0h`. `10h` and `11h`
select and persist the `FDh` and `FEh` items, but can change the active
alignment state as well. The `12h/16h..1Dh` adjustment handlers need tracing
to establish which other EEPROM values they can persist. There is no
identified tuning command that directly writes back the ten-value `05h`
response. Do **not** claim full calibration restore until every required field
has a verified read/write path and readback verification.

These tuning-handler findings are from analysis of the **TK-941 AB00 firmware**,
not from a completed tuning-mode hardware test. The safe PC-side entry
sequence for `A2h` also remains unresolved. In particular, do not probe
`A1h` or `2Bh` in an attempt to enter or initialize a backup session.


#### DOS host software and TK-840 / TK-940 / TK-941 tuning readbacks

**Safe session entry is now identified from executable code in the Kenwood DOS
host `KPGPRN.EXE` (rather than just its text strings):** send seven ASCII
bytes `TUNING ` (**including the trailing space**) at 1200 baud, expect
`06h`, then use 9600 baud for the tuning commands. The executable transmits
the literal at its file-image offset `0x16925` using its serial-buffer-send
function. It then sends `05h`, receives the model-specific payload, sends
`06h`, and expects a final `06h`. The `05h` host implementations are at
approximately `0x16958` (10 bytes, TK-940/941) and `0x16A2E`
(16/17 bytes, TK-840 variants). These offsets are in the unloaded DOS
EXE image, **not** uPD78312 CPU addresses. The `TUNING ` PC sequence is
a strong explanation of the panel's `55 A2` entry event; the actual panel
firmware is not yet independently decoded. `[DOS][FW][RE]`

**TK-940 / TK-941 AB00:** Correlating the radio's `BCE3h` handler and the
DOS print routine's field selection yields the following *exact raw EEPROM
byte* mapping. The radio sends them in this order (followed by the two
transformed `FDh`/`FEh` selection values):

| Reply index | EEPROM address | KPG-25D / KPGPRN adjustment |
|---:|---:|---|
| 0 | `F8` | Squelch |
| 1 | `F2` | RF power |
| 2 | `F4` | Maximum deviation |
| 3 | `F6` | DQT balance |
| 4 | `F9` | QT fine deviation |
| 5 | `FA` | DQT fine deviation |
| 6 | `FB` | LTR code fine deviation |
| 7 | `F0` | Frequency adjustment |
| 8 | `FD` (converted/clamped) | Selected tuning channel, zero-based |
| 9 | `FE` (converted/clamped) | Selected adjustment item, zero-based |

**TK-840 firmware checksum `8355h`:** Unlike AB00, its actual
`05h` tuning handler at `BCDDh-BD89h` transmits **17 bytes**. The
DOS program has 16- and 17-byte receive paths to support a variant.
The 8355 handler explicitly reads these EEPROM addresses or computes
the following values, in order:

| Reply index | Source | KPG-25D / KPGPRN adjustment |
|---:|---|---|
| 0 | EEPROM `F8` | Squelch |
| 1 | calculated by `C51Ch` | RF power at test frequency |
| 2 | EEPROM `F0` | RF power, low |
| 3 | EEPROM `F1` | RF power, center |
| 4 | EEPROM `F2` | RF power, high |
| 5 | EEPROM `F4` (2-byte read at `F4`) | Maximum deviation |
| 6 | EEPROM `F6` (2-byte read at `F6`) | DQT balance |
| 7 | EEPROM `F9` | QT fine deviation |
| 8 | EEPROM `FA` | DQT fine deviation |
| 9 | EEPROM `FB` | LTR code fine deviation |
| 10 | calculated by `C50Dh` | RX sensitivity at test frequency |
| 11 | EEPROM `ED` | RX sensitivity, low |
| 12 | EEPROM `EE` | RX sensitivity, center |
| 13 | EEPROM `EF` | RX sensitivity, high |
| 14 | EEPROM `EC` | Frequency adjustment |
| 15 | EEPROM `FD` (converted/clamped) | Selected tuning channel |
| 16 | EEPROM `FE` (converted/clamped) | Selected adjustment item |

Thus **8355 supplies 13 raw calibration EEPROM bytes, two computed test-frequency
values, and two transformed selection values** via one `05h` request.
The computed values are not additional EEPROM addresses: helpers
`C51Ch` and `C50Dh` select the `F0h` and `EDh` interpolation paths and
use frequency-dependent adjustment routines. The DOS print function at
approximately `0xDFDA` selects the named values from the buffer filled by
`0x16A2E`. In the shorter 16-byte DOS path, the frequency-adjustment field
is absent from the reply, as confirmed by the different copy-out path; its
radio firmware is **not yet identified**. `[DOS][FW][RE]`

Both DOS programs also expose `Eeprom display` and `Print Tuning Data` in
their menus. **EEPROM Display has not yet been decoded** and cannot be assumed
to dump the external AT24C02; further host-code analysis is needed.
The DOS binaries themselves are copyrighted and remain outside the repo.

**Restore remains unverified.** Reading the current adjustments does not
establish a generic way to write them back. The alignment setter commands
must be mapped to EEPROM addresses and verified without using tuning `2Bh`,
which erases the whole chip. Both the PC handshake and command paths still
need live-radio verification before inclusion in TKx4xToolkit.

The software serial port itself (pins, timing, the stock implementations,
speed limits, and design notes for a faster session) is covered in
[Mic Port Serial](Mic_Port_Serial.md).

The accessory-side hardware UART has a second, separate session in the
application itself (`'O'`/`'C'` at 1200 baud, then a PC-chosen speed, `'S'`
for a read/write loop); see the hardware reference, section 12.4.

The documented model-ID prefixes are `M890B1`, `M890B2`, `M890B3` (TK-840 variants), `M940B1`, and `M941B1`. A seventh state byte can indicate password protection. The identification bytes are not a substitute for checking the radio's actual hardware and firmware revision.

## Reading CPU memory

A verified block request is:

```text
PC -> radio:  52 AH AL 80
radio -> PC:  06                   ACK (may be consumed by serial layers)
radio -> PC:  57 AH AL 80 [128 data bytes]
PC -> radio:  06
radio -> PC:  06
```

`52` and `57` are ASCII `R` and `W`. `AH AL` is the big-endian 16-bit CPU address. The tested transfer length is `0x80` (128 bytes). The verified implementation reads full aligned blocks and saves only requested bytes for unaligned ranges; other transfer lengths have not been verified.

Reading `0xFF00-0xFFFF` (SFRs) can have hardware side effects; RAM is mutable during a dump. Treat apparently unmapped addresses as undefined.

To exit the ordinary session, send ASCII `E`. The tested TK-840 does not reliably send an acknowledgment to this command.

**Important EEPROM side effect:** If the monitor has processed a `W`
command, it sets `FEDF.1` even before validating the requested write. The
subsequent `E` exit may invoke mask-ROM routine `102Bh`, which clears a
48-byte EEPROM region selected by EEPROM[`FFh`] (unless that byte is `FFh`),
unconditionally clears EEPROM[`C0h`..`D1h`], and writes one other EEPROM
byte. See [section 9.4.1 of the hardware reference](../hardware/DAC_Expander_EEPROM.md#941-partial-field-map-new-firmware-traced).
This is **separate from the tuning `2Bh` EEPROM erase**. These are real
write side effects; do not describe the ordinary `W` + `E` sequence as
EEPROM-neutral. `[MASK][FW]`

## Ordinary codeplug writes

The observed codeplug write format is `57 AH AL 80 [128 data bytes]`, using the same 128-byte addressing unit. The ordinary write path is limited to the **codeplug/configuration region `0xE600-0xFDFF`**; it is not a firmware programming mechanism.

Write software should back up the existing contents, compare before writing, and read back every changed block, followed by full-region verification.

## Firmware programming and checksums

Application firmware spans `0x8000-0xE5FF` (`0x6600` / **26,112 bytes**), while codeplug/configuration occupies `0xE600-0xFDFF` (`0x1800` / **6,144 bytes**).

Firmware programming uses the **separate resident loader**, not ordinary `W` commands. The decoded loader accepts addressed ASCII-hex records with 16 data bytes per record, building 64-byte `AT29C256` pages. The first received serial byte selects the ASCII-hex path and is discarded. The original implementation used a carriage return for that byte and a zero-length record to end the stream. Those observations were only partially bench-verified in the original implementation; consult the current Toolkit for supported programming procedures.

Loader access depends on codeplug bit `0xE66C.2`; the original firmware boot signature is `KENWOOD` at `0xE580`. Current handling, interlocks, and recovery procedures are explained in the [hardware reference](../hardware/README.md) and Toolkit documentation.

The firmware **Kenwood checksum** is the low 16 bits of the sum of all bytes in `0x8000-0xE5FF`. It identifies a revision but is not a cryptographic integrity check; the memory-reading utility also reports CRC-16/CCITT-FALSE and SHA-256. Use SHA-256 when comparing exact firmware images.

## Provenance and test scope

These packet details are based on KPG-25D protocol analysis, source-code validation, and TK-840 bench testing. They do not establish that every mode or firmware revision has been hardware-verified. In particular, the presence of TK-940/TK-941 model IDs does not by itself verify every protocol path on those models.
