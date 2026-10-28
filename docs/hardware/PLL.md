<!-- PLL and frequency generation -->
Part of the [TK-x4x hardware reference](README.md) · previous: [I/O map and the shared serial bus](IO_Map_and_Serial_Bus.md) · next: [D/A converter, output expander and calibration EEPROM](DAC_Expander_EEPROM.md)

# 6. PLL and frequency generation

## 6.1 Hardware

Synthesizer:

```text
Motorola MC145190 family
Reference oscillator: 12.8 MHz
P00 = ENABLE
P01 = CLOCK
P02 = DATA
P32 = lock/unlock indication
```

`[SM][FW][DS]`

The PLL drives the TX local oscillator and receiver first local oscillator
through the VCO system.

## 6.2 Serial protocol

The interface uses MSB-first serial transfers. ENABLE is taken low while bits
are shifted and raised high to latch the completed transfer. `[FW][DS]`

```text
C/configuration register   8 clocks
R/reference register      16 clocks
A/N divider register      24 clocks
```

The stock wide-band TK-840 image programs:

```text
R = 0x4400
```

The low 13-bit reference divider is 0x0400 = 1024:

```text
12.8 MHz / 1024 = 12.5 kHz phase-detector/reference step
```

Both stock images (TK-840 8355 and TK-940/941 AB00) send C = `0x6E` once
at start-up, then R = `0x4400`. The TK-840 also sends C = `0x6D` after every
new divider and `0x6C` 1.5 ms later (section 6.4); AB00 never sends `6Dh` or
`6Ch`. What each C bit does is still `[OPEN]`; `6Dh` then `6Ch` looks like a
fast-lock setting followed by the normal one. AB00 instead waits 0.5 ms on
timer 1 after every C, R and A/N latch. `[FW]`

### The A/N divider word

Both images send the 24-bit A/N word with the same routine (TK-840 8355
`9BBCh`, TK-940/941 AB00 `9B53h`), from a 17-bit divider D held in
`FE20h`-`FE22h` (`FE22h` = bit 16):

```text
byte 2 (first) = (D >> 14) OR 30h
byte 1         = (D >> 6) AND FFh      N = D / 64 starts at bit 8
byte 0 (last)  = D AND 3Fh             A = D mod 64 is bits 0-5
```

Bits 6-7 are 0 and bits 20-21 are always 1. Example, D = 32396 (TK-840,
450.000 MHz receive): `31h FAh 0Ch`. `[FW]`

The routine saves AX and HL and pops them back in the same order, so they
come back swapped: afterwards `FE20h`-`FE22h` do not hold D any more. Nothing
reads them before the next tune rebuilds them, so it does no harm, but a
caller must not expect D (or its own HL) back. `[FW]`

An earlier version of this document and the phase1 probe put N at bits
12-21 and A at bits 6-11, a guess from a datasheet; that layout gives the
chip a different divider and cannot lock.

## 6.3 Confirmed bit-bang primitive

Stock firmware contains the equivalent of:

```asm
; CY = next output bit
MOV1 P0.2,CY
SET1 P0.1
NOP
CLR1 P0.1
RET
```

`[FW][RE]`

A C implementation is possible, but the existing compiler emits larger generic
SFR accesses. Bench timing must be checked before relying on C for this loop.

A from-scratch C equivalent, useful for bring-up but not yet a substitute for
bench timing verification, is:

```c
#define PLL_P0          SFR8(0xFF00)
#define PLL_ENABLE_BIT  0x01   /* P00 */
#define PLL_CLOCK_BIT   0x02   /* P01 */
#define PLL_DATA_BIT    0x04   /* P02 */

static void pll_clock_bit(u8 bit)
{
    if (bit)
        PLL_P0 |= PLL_DATA_BIT;
    else
        PLL_P0 &= ~PLL_DATA_BIT;

    PLL_P0 |= PLL_CLOCK_BIT;
    /* stock firmware uses one NOP here */
    PLL_P0 &= ~PLL_CLOCK_BIT;
}

void pll_write_bits(unsigned long value, u8 nbits)
{
    u8 i;

    PLL_P0 &= ~PLL_ENABLE_BIT;

    for (i = 0; i < nbits; ++i) {
        u8 bit = (value >> (nbits - 1 - i)) & 1;
        pll_clock_bit(bit);
    }

    PLL_P0 |= PLL_ENABLE_BIT;
}

void pll_write_c(u8 value)          { pll_write_bits(value, 8);  }
void pll_write_r(u16 value)         { pll_write_bits(value, 16); }
void pll_write_a(unsigned long val) { pll_write_bits(val, 24); }
```

For production code, the bit clock is a good candidate for assembly because
the stock firmware's exact `MOV1 / SET1 / NOP / CLR1` sequence is already known.

## 6.4 Divider math and tuning

```text
D = 64*N + A
fVCO = 12.5 kHz * D
```

**TK-840 (image 8355).** A channel is stored as a 15-bit value in 12.5 kHz
steps above 300.000 MHz (codeplug word `BAB6h`, bit 15 cleared: `3AB6h` =
15030 = 487.875 MHz). The code adds a base and sends D:

| Path | Routine | D = | VCO | STR (expander bit 6) |
|---|---|---|---|---|
| transmit | `pll_tune_tx` `9AC1h` | channel + `5DC0h` (300.000 MHz) | on the channel | cleared |
| receive | `pll_tune_rx` `9B23h` | channel + `4FACh` (300.000 - 45.050 MHz) | 45.050 MHz below | set |

Then it commits the expander, sends C = `6Dh`, waits 1.5 ms on timer 1
(`MD1 = 05DCh`, 1 µs per count), and sends C = `6Ch`. A channel's transmit
tune then waits three 3 ms timer periods and polls P32 until it reads 1 or
a timeout flag (`FE7Ch.7`) drops (`E2E0h`); the dealer test-frequency path
waits 10 ms and then tests P32 (`9C0Dh`, see 6.5). On the TK-840, D is
always below 65536 (`FE22h` = 0): 450.000 MHz transmit is D = 36000
(N = 562, A = 32), receive is D = 32396. `[FW]`

The TK-840 transmits on the channel's own TX word: there is no fixed
transmit offset in this image and nothing to patch (6.6). `[FW]`

**TK-940/941 (image AB00).** D needs 17 bits (about 72000-76000 at
900 MHz). For the confirmed conventional word interpretation,
stock TX offsets, patched TX lower bound, and the distinct dealer-test
frequency base, see [Frequency Encoding and Limits](../codeplug/Frequency_Encoding_and_Limits.md).
The PLL calculation here clears word bit 15 and counts 12.5 kHz steps
above 851.000 MHz, adds a 16-bit base, and puts the carry in `FE22h`
as D bit 16. `FEDAh.0`, one of the model straps, is set on the TK-940
and clear on the TK-941 (mask ROM model detection, `1F15h`). `[FW]`

| Path | Word used | Base | VCO |
|---|---|---|---|
| receive, TK-940 | RX | `FBDDh` | RX - 45.0375 MHz |
| receive, TK-941 | RX | `FDBDh` | RX - 39.0375 MHz |
| transmit, Talk Around off, TK-940 | RX | `FBE0h` | RX - 45.000 MHz |
| transmit, Talk Around off, TK-941 | RX | `FDC0h` | RX - 39.000 MHz |
| transmit, Talk Around on | RX | `109F0h` | RX |
| transmit, Enhanced TX Patch | TX | `109F0h` | TX |

The stock conventional TX path uses RX (and its model-specific offset)
rather than the channel's TX word; the receive bases put the first LO
45.0375 or 39.0375 MHz below the channel. The [frequency reference](../codeplug/Frequency_Encoding_and_Limits.md)
explains why this is compatible with sub-851 MHz stock TX. The patch
sites are described in §6.6. `[FW]`

Arithmetic stays 16-bit: add the base to the channel in 16 bits and keep
the carry as bit 16, as the stock code does.

## 6.5 Safe programming sequence

Never change PLL frequency while RF KEY is asserted.

Recommended sequence:

1. Clear `P17/KEY`.
2. Program the divider (section 6.4), then select the VCO with the 4094's
   `STR` (set = receive, clear = transmit) and commit it.
3. C = `6Dh`, wait 1.5 ms, C = `6Ch`.
4. Wait for lock indication from `P32/CI1` (1 = locked).
5. Apply calibrated DAC values.
6. Assert `P17/KEY` only when all transmit safety checks pass.
7. If lock is lost while transmitting, immediately clear `KEY`.

`[RE]`

**P32 logical polarity (moderate-strong, from firmware tracing):** the stock
TK-840 firmware bit-tests `0xFF03.2` (P32) in exactly two places (`0x9C21` and
`0xE304`), both reached as the final step right after the divider is
reprogrammed: `9C21h` after a dealer test-frequency transmit tune, `E304h`
after a channel's transmit tune (AB00: `9BBFh` and `9D5Ah`). In both, **taking
the branch on P32=1** is what lets the routine finish promptly (it then
touches an unidentified pair of bits at `sfr[0xD2]`, the timer-2 interrupt
control, and returns). At `E304h` P32=0 means another try until a timeout
flag drops; at `9C21h` the retry loop only runs while the timer flag is set,
which it has just been cleared, so P32 is in effect tested once. Either way
the routine goes on regardless once it gives up. This is consistent with
**P32 = 1 meaning "PLL locked"** in the firmware's own logic, though what
`sfr[0xD2]` physically drives was not identified (no other reference to it
exists anywhere in either ROM), so this confirms *sequencing* behavior more
than it confirms "this is specifically the KEY-enable gate." `[FW][RE]`

The older communication draft contained a provisional C `pll_locked()` sketch
that treated `P32` as an ordinary port bit. That is now supported rather than
merely plausible: a `pll_locked()` helper reading `SFR8(0xFF03) & 0x04` and
treating a nonzero result as locked matches both traced call sites. The
remaining `[OPEN]` item is the *electrical* polarity at the CI1 pin itself
(does the PLL's own LD output directly drive high-for-locked, or is there an
inverting buffer stage) -- bench confirmation is still worthwhile before
final production use, but the firmware-logic side of this question is closed.

## 6.6 From channel to synthesizer

Traced in both stock images (TK-840 8355, TK-940/941 AB00); the two use the
same code for everything up to the bases in 6.4. Routine names are the ones
in the master listings. `[FW]`

**The working channel** is a System number in `FE23h` and a Group number in
`FE24h`. Changing channel ends in `channel_activate` (TK-840 `8DEDh`, AB00
`8DD2h`): load the channel, then tune the receiver.

**Finding the record** (`system_header`, `system_group_count`,
`conv_find_group`). The System's pointer-table word is at `E600h + 2 * (n -
1)`; its low 14 bits plus `C000h` give the header address in flash (the
codeplug's pointers are offset by `2600h`, and `E600h - 2600h = C000h`). The
record count is at `E640h + n`; 0 or more than 250 counts as invalid.
Records are 19 bytes after the 14-byte header, and the search compares each
record's first byte with the wanted Group number. Header byte 0 bit 0 marks
an LTR System, which takes its own path (`[OPEN]`).

**Loading** (`channel_load`, CALLT slot `16h`) copies only the record's tone
and option bytes into RAM:

| RAM | From the record | Meaning |
|---|---|---|
| `FE8Eh` | `+05` word | TX tone (encode) |
| `FE8Ch` | `+07` word | RX tone (decode) |
| `FE9Eh` | `+09` (flags A) | bit 1 = Talk Around, active low |
| `FE9Eh.6` | `+0A` bit 0 (flags B) | Data, replacing flags A bit 6 |

The frequency words (`+01` RX, `+03` TX) stay in flash: the tuning code reads
the record again every time it tunes, and clears bit 15 of the word it
reads.

**Receive** (`tune_rx`, `conv_tune_rx`): RX word plus the receive base (6.4),
then the A/N word.

**Transmit** (`conv_channel_tx`, called when the transmitter is keyed, after
the expander is set for transmit; TK-840 `9C66h`, AB00 `9C61h`):

- TK-840: the record's TX word plus `5DC0h`, then C = `6Dh`, 1.5 ms, C =
  `6Ch` and the lock wait.
- AB00, stock: the record's **RX** word plus a base chosen by the Talk
  Around bit and the model (6.4), then the lock wait. The TX word is never
  read.

**Dealer test frequencies** (`dealer_tx`, `dealer_tune_rx`): the test table
at `E680h`, 4 bytes per slot (RX word, TX word), slot number in `FE23h`.
The TK-840 transmits on the slot's TX word (or its RX word when `FE78h.4`
is set); stock AB00 transmits on the slot's RX word less the same fixed
offset (or on it, with `FE78h.4`). The TK-840's panel-tuning mode uses
frequencies from a ROM table instead (`C426h`-`C449h`).

**The Enhanced TX Patch** (Kenwood_TK-x4x_Toolkit, `apply_tx_fix`)
changes two places in AB00:

- `conv_channel_tx` reads the record's TX word and adds `109F0h`: the
  VCO goes on the programmed transmit frequency. Talk Around and the model
  are no longer looked at.
- `dealer_tx` reads the slot's TX word and adds `FDC0h` (812.000 MHz)
  whatever the model; `FE78h.4` is no longer looked at.

The patch's **conventional TX word** retains the 851 MHz coding base,
while the **dealer-test TX word** uses 812 MHz. Consequently, the patched
normal-channel path cannot encode the TK-940's original 806–825 MHz
TX band. For the full distinction between representable codeplug words,
stock synthesized TX, patched TX, CHIRP limits, and RF hardware limits,
see [Frequency Encoding and Limits](../codeplug/Frequency_Encoding_and_Limits.md).
The TK-840 already uses its conventional TX word and needs no such
patch. `[FW]`

**Talk Around.** A conventional record's flags A bit 1 (`0` = on) reaches
`FE9Eh.1` through `channel_load` and nowhere else: no key or menu changes
it. Every place that reads it:

| Where | TK-840 (8355) | TK-940/941 (AB00) |
|---|---|---|
| `conv_channel_tx` | not read: TX word | on: RX word + `109F0h`, the RX frequency; off: the fixed offset |
| before KEY, after the transmit tune | 3 x 3 ms either way (`AF84h`) | on: 12 x 3 ms = 36 ms (`B040h`, `wait_tm1_periods`); off: none |
| `show_ta_icon` (channel display) | `PANEL_TA_ON` / `_OFF` (`A864h`) | the same (`A8FDh`) |

On a TK-840 conventional channel the bit only shows the TA icon. On a
stock TK-940/941 it moves transmit onto the receive frequency, and the
extra 36 ms lets the transmit VCO settle after the longer move (that
reason is `[RE]`). The Enhanced TX Patch removes the first effect and
keeps the other two: with the patch, Talk Around shows the icon and delays
keying by 36 ms but no longer changes the frequency. The other `FE9Eh.1`
tests in both images (TK-840 `CADFh`, `D73Dh`; AB00 `C786h`) are LTR code,
which reuses `FE9Eh` as five match bits (`10h`-`01h`). `[FW]`

Talk Around is not a TK-940/941-only feature. Both images honor it in two
other places, the TK-840 included:

- **LTR.** `ltr_group_load` copies an LTR Group record's flags A (`+3`)
  into `FE9Ah`, so its bit 1 is that Group's Talk Around, then forces it
  off when the Group's ID (`+2`) is inside the System's ID range in header
  bytes `+4`-`+5` (`ltr_tel_id`; the same test lights the TEL icon, so
  `[RE]` that range is the telephone IDs). `ltr_tx` then transmits on the
  repeater entry's RX word with Talk Around on, and on its TX word (TK-840)
  or the fixed offset (AB00) with it off. The patch does not touch
  `ltr_tx`. LTR Group records are 13 bytes after the System's 5-byte
  repeater entries (`ltr_find_group`); the rest of the LTR layout is
  `[OPEN]`.
- **Dealer test mode.** `FE78h.4` is that mode's Talk Around: the AUX
  key (`9Ch`) toggles it there (AB00 `9F4Ah`, TK-840 `9EC9h`), it is kept in EEPROM `D1h` bit 0 (`0` = on), and PC
  commands `09 0D` / `09 0E` set and clear it. Each change sends the TA
  icon byte. `dealer_tx` uses it as above; the patched AB00 `dealer_tx`
  ignores it. `[FW]`

---

Part of the [TK-x4x hardware reference](README.md) · previous: [I/O map and the shared serial bus](IO_Map_and_Serial_Bus.md) · next: [D/A converter, output expander and calibration EEPROM](DAC_Expander_EEPROM.md)
