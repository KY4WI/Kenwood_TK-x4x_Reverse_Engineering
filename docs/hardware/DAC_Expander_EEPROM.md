<!-- D/A converter, output expander and calibration EEPROM -->
Part of the [TK-x4x hardware reference](README.md) · previous: [PLL and frequency generation](PLL.md) · next: [External flash programming](Flash_Programming.md)

# 7. M62363FP 8-channel DAC

## 7.1 Role

The M62363FP controls analog adjustment/operating levels including functions
such as:

- reference oscillator/frequency correction,
- TX level/power-related adjustment,
- modulation/deviation level,
- audio level/power.

A partial channel-to-function map is now evidenced from firmware tracing --
see 7.1.1. `[SM][RE]`

## 7.2 Hardware interface

```text
P01 = clock
P02 = data
P07 = DAC enable/latch
```

The mask ROM exposes a resident driver:

```text
1F24   M62363 write
A = channel 1..8
X = 8-bit value
```

`[MASK][RE]`

Stock firmware also has an external branch stub, reached through the CALLT
table (table address `0x0066`, opcode `0xF3` = `callt [0x0013]`, per section
17.1) rather than an ordinary `CALL` -- **21 real call sites** were found this
way, not zero. An earlier attempt to trace this by searching for direct
`CALL 0x1F24` text turned up nothing and wrongly looked like a dead end; the
CALLT indirection was the missing piece. `[FW][RE]`

```text
0040-007E CALLT table, entry at 0x0066 -> 0x808F
0x808F:   BR 0x1F24
```

## 7.1.1 Partial DAC channel map (new, firmware-traced)

Every one of the 21 `callt [0x0013]` (DAC-write) sites was inspected for what
literal channel number (`A`) and value source (`X`) precede the call. This is
now solid enough to design around for channels 2/3/5/6, and a reasonable lead
for 4/7; channels 1 and 8 are not yet understood. `[FW][RE]`

| Channel | Value source | Evidence |
|---:|---|---|
| 2 | AT24C02 EEPROM byte `0xF4` | Two independent call sites confirm this: a blocking read (`1F1B` via `callt [0x000d]`) at `0x8856`, and a cooperative read (`1F45`) at `0xE2BC`. Both immediately precede `mov a,#0x02` then `callt [0x0013]`. |
| 3 | AT24C02 EEPROM byte `0xF6` | Same double-confirmation pattern as channel 2, at `0x8876` (blocking) and `0xE2C8` (cooperative). |
| 6 | AT24C02 EEPROM byte `0xEC` | `0xB8D2`, blocking read via `callt [0x000d]`, same shape. |
| 4 | AT24C02 EEPROM byte `0xF1` **or** `0xF2` (two related call sites), or RAM `0xFEE9`/`0xFEEA` when flag `0xFEDA.6` is set | Region `0xB979-0xB9A6`. The RAM-vs-EEPROM choice is gated by `0xFEDA.6` -- plausibly "live service-mode override" vs "calibrated value" -- but the exact semantics of that flag are not yet confirmed. |
| 7 | AT24C02 EEPROM byte `0xEE` **or** `0xEF`, or RAM `0xFEE9`/`0xFEEA`/`0xFEEB` under the same `0xFEDA.6` gating | Region `0xB90B-0xB938`, structurally identical to the channel-4 block. |
| 5 | **Not EEPROM.** A live 32-entry lookup table at ROM address `0x0D14`, indexed `0x00-0x1F` by RAM byte `0xFEA7`. Four call sites (`0x8EC6`, `0xA4B2`, `0xB3A0`, `0xC137`); three of them explicitly clamp `0xFEA7` to `0x00` or `0x1F` (boundary snapping), and one loads it from `callt [0x000a]` (a user-input read). | This is a strong candidate for **volume**: a UI-driven 0-31 level feeding a lookup table, not a calibration byte. Not yet bench-confirmed against the actual volume keys. |
| 2, 3 | (separately) forced to `X=0x00` | `0xE2D5-0xE2DE`: a short, unconditional two-call sequence writes channel 2 then channel 3 to zero, back to back. Reads like a safe-default/mute step on some transition; context (what precedes/follows this in the caller) was not traced. |
| 1, 8 | not found | No call site was found writing channel 1 as a normal calibration/UI value. Two small helper routines do write channel 1 with the literal extremes `0x00` and `0xFF` (nothing in between observed), consistent with an on/off-style control rather than a graduated level, but the purpose is unconfirmed. Channel 8 has no evidence either way. |

This directly narrows the "map all eight channels" open item from a cold
start to: confirm 4/7's real distinguishing purpose, confirm 5=volume on the
bench, and find any use of 1/8 at all. It also gives a first real answer for
the EEPROM calibration format (section 9) -- see 9.4.1.

## 7.2.1 Volume control and user audio `[RE]`

The physical volume-up/down controls are panel keys. The main CPU does not read
a potentiometer or dedicated volume GPIO. A volume change therefore has two
parts:

1. receive/decode the panel key event;
2. change the radio's audio-level control state.

Per 7.1.1, **DAC channel 5** is the strong candidate for this: a UI-facing
0-31 index (`RAM 0xFEA7`) through a 32-entry lookup table, clamped at both
ends, distinct from every other channel's calibration-EEPROM sourcing. This
should be treated as a working hypothesis to bench-confirm (press volume
up/down, watch channel 5's calls and the resulting audio level), not yet as
a frozen contract.

Do not define `volume_set(0..255)` as a stable hardware API until this is
bench-confirmed. In particular, do not overwrite calibration-related DAC
channels (2/3/4/6/7) with guessed values merely because a value produces
audible audio.

A sensible eventual interface is:

```c
void dac_write(u8 channel, u8 value); /* resident 1F24 via verified shim */
void volume_step(int direction);
void volume_set_ui_level(u8 level);
```

where `volume_set_ui_level()` performs whatever mapping the stock analog path
actually requires (likely: index into the same 32-entry table, or reproduce
its curve).

## 7.3 Replacement-firmware rule

Do not invent DAC defaults. Calibration/backup values live in the AT24C02.
Read the radio's calibration data and reproduce the known stock initialization
before transmitting. A wrong DAC value can produce frequency error, excessive
deviation, incorrect power, or bad audio levels.

`[OPEN]`: confirm channel 5 = volume on the bench; determine channels 4 vs 7's
real distinguishing purpose and the meaning of the `0xFEDA.6` RAM/EEPROM
gate; find any use of channels 1 and 8 (1's `0x00`/`0xFF`-only pattern is a
lead, not a conclusion).

---

# 8. BU4094/XRU4094 output expander

## 8.1 Interface and resident API

```text
P01 = shared clock
P02 = shared data
P03 = strobe/enable
```

Mask-ROM entry:

```text
1F27   commit complete 8-bit expander state
FE28   complete software shadow byte
```

`[MASK][RE]`

Correct usage is always whole-state/shadow based:

```c
/* conceptual code */
FE28 |= SOME_BIT;
call_mask_1F27();

FE28 &= ~SOME_BIT;
call_mask_1F27();
```

Never emit a one-bit change without preserving the other seven outputs.

The resident routine shifts FE28 MSB first. Because a serial shift register
moves the earliest bit through all stages, after eight clocks the natural map
is FE28 bit N -> QN. `[MASK][RE]`

## 8.2 Current Q-output map

| FE28 bit | 4094 output | Schematic net | Current interpretation | Status |
|---:|---|---|---|---|
| 0 | Q0 | `AM` | Audio mute/control path | `[SCH][RE]` |
| 1 | Q1 | `AC` | Audio/control net | `[SCH][OPEN]` |
| 2 | Q2 | `TR` | T/R-related control | `[SCH][OPEN]` |
| 3 | Q3 | `STT` | Control net; exact function not yet decoded | `[SCH][OPEN]` |
| 4 | Q4 | `RLO` | Control net; exact function not yet decoded | `[SCH][OPEN]` |
| 5 | Q5 | `LOK` | Link-complete/accessory signaling path | `[SCH][SM][RE]` |
| 6 | Q6 | `STR` | TX/RX VCO selection | `[SCH][SM][RE]` |
| 7 | Q7 | `SQ` | Logic squelch/accessory squelch output path | `[SCH][SM][RE]` |

Suggested definitions:

```c
#define EXP_AM   0x01
#define EXP_AC   0x02
#define EXP_TR   0x04
#define EXP_STT  0x08
#define EXP_RLO  0x10
#define EXP_LOK  0x20
#define EXP_STR  0x40
#define EXP_SQ   0x80
```

Do **not** assume expander-bit logic level equals accessory-connector logic
level. Transistor/buffer stages may invert the signal.

## 8.2.1 AC/TR/STT/RLO context, from firmware tracing

55 bit-level `set1`/`clr1` sites on bits 1-4 were traced and clustered by code
proximity, each cross-checked to confirm a real commit (`callt [0x0012]`,
resident `1F27`) follows. `[FW][RE]`

- **`TR` and `STT` are consistently complementary.** Four independent sites
  toggle them as an exclusive pair (`0x9D37`/`0x9D3D`, `0xA207`/`0xA20D`,
  `0xB02F`/`0xB035`, plus the four-site pattern below). Whenever one goes
  high the other goes low, never both together.
- **`AC`+`TR` clear together with `STT` set**, at four sites
  (`0xA0FD-0xA103`, `0xAF78-0xAF7E`, `0xD0F2-0xD0FC`, `0xD18B-0xD1A5`). The
  complementary state (`TR` set / `STT` clear) at `0xA207` is reached with
  the *same three auxiliary bits* (`0xFE78.7`, `0xFF02.6`, `0xFF01.7`) toggled
  to their opposite values from the `AC`-clear block. This is strong evidence
  of two mutually exclusive firmware states -- **not yet named** (no KEY/PTT
  bit was found in the same block to anchor which state is RX and which is
  TX), but real enough to treat `AC`/`TR`/`STT` as one coupled group rather
  than three independent bits.
- **`RLO` is gated, not a simple toggle.** Both sites that set it
  (`~0x8D8D`, `0xB199-0xB1AC`) require: AT24C02 byte `0x28` bit 0 clear,
  *and* a fixed external-ROM table byte at `0xE66B` bit 1 set, *and* port
  bits `0xFF02.2`/`0xFF01.1` both set, before `set1 0xFE28.4` / commit. This
  reads as an optional/config-gated feature bit (an installed-accessory
  check), not an ordinary RX/TX state bit.
- **Reset default** (mask ROM `0x0064: mov 0xfe28,#0x25` = bits 0,2,5 set):
  `TR=1` at power-up, `AC=STT=RLO=0`.

None of this proves the schematic net names (`AC`/`TR`/`STT`/`RLO`) map to
specific physical functions -- it proves how the *bits* relate to each other
in firmware, which is enough to avoid breaking the coupling by accident even
before the physical meaning is confirmed on the bench.

## 8.3 STR and VCO selection

`STR` is one of the most important expander bits for basic radio operation. The
radio uses it to switch power/state between the RX and TX VCO paths.
**Set = receive VCO, clear = transmit VCO**: the TK-840 image sets bit 6 in
`pll_tune_rx` (`9B23h`) and clears it in `pll_tune_tx` (`9AC1h`), each
right after the new divider and before committing the expander. `[FW]`
The expander's reset value `25h` leaves it clear (transmit VCO).

---

# 9. AT24C02 calibration EEPROM

## 9.1 Hardware interface

```text
AT24C02: 256 x 8 bytes
P01     = SCL
P36     = SDA
```

P36 is **bidirectional**. The mask ROM changes `PM3.6` so it can drive SDA low,
release the line, read ACK, and receive data. This is software/bit-banged I2C,
not the µPD78312 serial peripheral. `[MASK][RE]`

The device address used by the resident code is A0/A1, consistent with address
pins strapped to 000.

## 9.2 Resident blocking API

```text
1F18   AT24C02 multi-byte/page write
1F1B   random single-byte read
1F1E   single-byte write
1F21   sequential multi-byte read
```

Important parameters already decoded:

```text
1F1B random read:
  FE29 = EEPROM address
  result -> FE2A and A

1F1E byte write:
  FE29 = EEPROM address
  FE2B = data byte

1F18 multi-byte/page write:
  FE29 = EEPROM address
  HL   = source buffer
  B    = count, non-zero
```

The AT24C02 page size is 8 bytes. `1F18` does **not** protect the caller from
crossing an 8-byte page boundary; crossing it can wrap within the device page.
Split writes accordingly. `[MASK][DS][RE]`

## 9.3 Cooperative API
```text
1F42   cooperative byte write
1F45   cooperative random read
```

These call external address 8235 repeatedly. Use the blocking API for initial
replacement firmware unless cooperative servicing is specifically required.

## 9.4 Calibration policy

Before doing any calibration write experiments:

1. Dump all 256 EEPROM bytes.
2. Save the dump with radio identity/date/checksum.
3. Keep ordinary firmware operation read-mostly.
4. Put calibration writes behind explicit service/alignment functions.
5. Verify every write by reading it back.

The full 256-byte dump above is a conservative safeguard for bench research,
not the end-user feature by itself. The Toolkit's goal is to preserve and
restore **every calibration field the radio actually uses**, with verified
readback. A selected-field backup is sufficient only after field coverage is
established; an incomplete backup must not be advertised as restorable.

## 9.4.1 Partial field map (new, firmware-traced)

Two genuinely different access patterns were found reading calibration
bytes, and they are kept separate below rather than merged into one table,
since it is not yet confirmed whether they describe the same underlying
fields or two unrelated things that happen to share nearby offsets.

**Operational path (single-byte reads feeding DAC writes directly, see
7.1.1):** `[FW][RE]`

| EEPROM byte | Used for |
|---|---|
| `0xEC` | DAC channel 6 |
| `0xF4` | DAC channel 2 |
| `0xF6` | DAC channel 3 |
| `0xF1` or `0xF2` | DAC channel 4 (exact selection condition not fully traced) |
| `0xEE` or `0xEF` | DAC channel 7 (exact selection condition not fully traced) |

**Service/display path (2-byte sequential reads via `1F21` into a fixed
`0xFEAE/0xFEAF` buffer, from a menu/alignment screen that formats the value
to on-screen digits rather than writing hardware):** `[FW][RE]`

| EEPROM bytes | Observed handling |
|---|---|
| `0xEC`/`0xED` | compared/clamped against constant `6` |
| `0xF4`/`0xF5` | compared/clamped against constant `2` |
| `0xF6`/`0xF7` | compared/clamped against constant `3` |
| `0xF8` | single byte -> RAM `0xFEF0` |
| `0xF0`, `0xF1`, `0xF2` | single bytes, formatted to ASCII digits for display only |
| `0xF9`, `0xFA`, `0xFB` | single bytes -> RAM `0xFE54`/`0xFE56`/`0xFE58`, also formatted for display |
| `0xFF` | blank-EEPROM sentinel -- mask-ROM routine `0x102B` treats `0xFF` at this address as "never formatted," and bulk-fills `0xC0-0xD1` (18 bytes) with `0xFF` in response |

It is a striking coincidence that `0xEC`/`0xF4`/`0xF6` show up on **both**
lists -- once as a single calibration byte feeding a DAC channel directly,
and once as the first byte of a 2-byte field compared against a small
constant (6/2/3) on a display screen. The most likely explanation is that
the display screen is simply showing the *same* calibration byte the DAC
path uses, with a second, adjacent byte carrying a format/units tag that
happens to equal the channel-numbering-like constants seen (`6`, `2`, `3`) --
but this is speculation, not confirmed. Do not build calibration-write
tooling against this table without first reading back a real EEPROM dump at
these exact offsets and confirming the relationship on the bench.

The mask ROM has a **separate**, potentially destructive EEPROM cleanup
routine at `0x102B`. The ordinary programming monitor's `W` handler sets
`0xFEDF.1` at `0x0EC6` **before** validating the target address/length. The
monitor checks this flag when it receives ASCII `E` (at `0x0DDA`), and if
set, calls `0x102B` before exiting. It is not a normal power-on operation
and is **not** the application's tuning-mode `2Bh` erase. `[MASK][FW]`

The full routine is now resolved in the mask-ROM disassembly:

1. Read EEPROM address `0xFF` as the starting address. If its value is not
   `0xFF`, write `0xFF` to **48 consecutive EEPROM locations** beginning
   there, with 8-bit address wraparound.
2. **Always** write `0xFF` to addresses `0xC0–0xD1` (18 bytes).
3. Read EEPROM address `0xFF` **again**. Write the byte stored in external
   ROM at `0xE66F` to EEPROM address
   `(current EEPROM[0xFF] + 0x27) & 0xFF`.

The second read is significant: the preceding 48-byte fill can itself
overwrite EEPROM address `0xFF`, changing the last write's destination. This
resolves the previously open 48-iteration loop and address arithmetic.
The purpose of this cleanup and the meaning of the `0xFF` pointer still
need validation before interpreting these locations as calibration fields.
`[MASK][FW][RE]`
## 9.4.2 Stock tuning-mode access versus calibration coverage

The stock application's tuning-mode commands provide a potential no-modification
route over the microphone programming connector. See the
[tuning-session command table](../programming/Programming_Protocol.md#tuning-session-commands)
for the exact reply order and handshakes. This analysis is from the TK-941
`AB00h` application; tuning-mode entry and these commands have not yet been
bench-verified on the TK-840 or other firmware revisions. `[FW][RE]`

| Tuning command | Exposed EEPROM data | Read/write status |
|---|---|---|
| `05h` | Eight raw bytes: `F8,F2,F4,F6,F9,FA,FB,F0`; two clamped indices derived from `FD,FE` | Read only; no matching bulk restore command identified |
| `2Ch` | Ten raw bytes: `E9` down to `E0` | `2Dh` writes those same ten addresses |
| `10h`, `11h` | Persist selected channel/item at `FD`/`FE` | Changes state; not a generic EEPROM write |
| `12h`, `16h..1Dh` | Alignment adjustments | EEPROM persistence and field mapping not yet fully traced |

These two read replies yield **18 original EEPROM bytes plus two derived
values**, not a raw 20-byte calibration image. Notably, known operational
calibration addresses `EC`, `EE/EF`, and potentially `F1` are missing.
They must be traced through the adjustment handlers or another safe read
path before a reliable calibration-backup-and-restore feature can be declared
complete. The resident EEPROM routines `1F1Bh` (one byte) and `1F21h`
(sequential read) are fully capable of reading these locations **from running
firmware**, but their existence does not make them directly callable through
the stock KPG monitor's external serial command protocol. `[MASK][FW][RE]`

**Destructive commands:** tuning `2Bh` erases the entire EEPROM before
writing defaults; the separate panel `A1h` path erases the codeplug. Do not
use either in an exploratory or backup-only host implementation.

## 9.4.3 DOS printout mapping and TK-840 8355 differences

The actual KPG-25D companion DOS executable (`KPGPRN.EXE`) and the
locally held TK-840 `8355h` firmware identify individual calibration
adjustments and their backing EEPROM bytes. The full readback table
is in [the serial protocol reference](../programming/Programming_Protocol.md#dos-host-software-and-tk-840--tk-940--tk-941-tuning-readbacks).

- The **TK-940/941 AB00 `05h` response** exposes eight raw calibration
  bytes: `F8` squelch, `F2` RF power, `F4` maximum deviation,
  `F6` DQT balance, `F9` QT fine deviation, `FA` DQT fine deviation,
  `FB` LTR fine deviation, `F0` frequency adjustment, plus derived
  `FD`/`FE` selection values.
- The **TK-840 8355 `05h` response** exposes thirteen raw calibration
  bytes: `F8, F0, F1, F2, F4, F6, F9, FA, FB, ED, EE, EF, EC`.
  The remaining four reply positions are two **computed test-frequency**
  readings (helpers `C51Ch` and `C50Dh`, for RF power and receiver
  sensitivity) and the two transformed `FD`/`FE` selection values.
  The four-frequency-point distinctions are directly visible in the
  DOS print layout. `[DOS][FW][RE]`
- This is **not evidence that every calibration value has a verified
  restoration method**. The DOS alignment setter paths and their actual
  EEPROM write locations still need tracing and readback tests.

The statements elsewhere in this document that `EC/EE/EF` are missing
from the `05h` response specifically describe **TK-941 AB00**. They
are **not** missing in TK-840 `8355h`, whose radio-side `05h` handler
explicitly transmits them. Calibration protocols are firmware-dependent.

---

Part of the [TK-x4x hardware reference](README.md) · previous: [PLL and frequency generation](PLL.md) · next: [External flash programming](Flash_Programming.md)
