# TK-x4x frequency encoding, transmit behavior, and limits

**Scope:** Kenwood TK-840 (analyzed firmware 8355) and TK-940/TK-941
(analyzed shared firmware AB00). This is the primary reference for the
**stock conventional frequency-word format**, the two stock 940/941
transmit offsets, the Toolkit's **Enhanced TX Patch**, the separate
dealer/test frequency coding, and the CHIRP driver's programming limits.

**Crucial distinction:** The lowest frequency *representable in a codeplug
field* is not necessarily the lowest RF frequency the radio can transmit.
The stock TK-940 proves the distinction: its normal codeplug RX word starts
at 851 MHz, but its firmware derives a roughly 806 MHz transmit frequency
from that word.

## 1. Conventional-channel frequency words

A conventional channel has independent, little-endian 16-bit words:
RX at record offset **+01/+02**, TX at **+03/+04**. The stock frequency
readers mask bit 15 before doing arithmetic:

```text
code = word & 0x7FFF
frequency_hz = base_hz + code * 12,500
```

| Model / conventional word | Base | Normal stock TX operation |
|---|---|---|
| TK-840 RX and TX | **300.000 MHz** | Reads the channel's TX word directly |
| TK-940 RX and stored TX | **851.000 MHz** | Usually uses RX − 45 MHz, **not** stored TX |
| TK-941 RX and stored TX | **851.000 MHz** | Usually uses RX − 39 MHz, **not** stored TX |

The stock raster is **12.5 kHz**. Bit 15 is *not* part of the
frequency value. Existing bit-15 values are preserved by the CHIRP driver;
it normally sets the bit for newly created valid channel words.

**Special value:** if the *lower* 15 bits equal `0x7FFF`, the frequency
is absent. Thus `0x7FFF` and `0xFFFF` both mean *no frequency*, rather
than the mathematical endpoint of the raster. In particular, an absent
TX-frequency word is not itself a reliable TX interlock; use the separate
**TX Inhibit** setting. See the [CHIRP driver's TX Inhibit policy](https://github.com/KY4WI/Kenwood_TK-x4x_Chirp_Driver#tk-940--tk-941-transmit-behavior).

Examples of stored values, with bit 15 masked away:

| Model | Code | Frequency represented |
|---|---:|---:|
| TK-840 | 0 | 300.0000 MHz |
| TK-840 | 12,000 | 450.0000 MHz |
| TK-940/941 | 0 | **851.0000 MHz** |
| TK-940/941 | 3,920 | 900.0000 MHz |
| TK-940/941 | 4,080 | 902.0000 MHz |

On a TK-940/941, a codeplug frequency **below 851 MHz cannot be encoded**
using this stock conventional-word interpretation. This is a
firmware/format limitation, not proof that the RF circuits cannot operate
there.

## 2. What actually determines normal TX?

The TK-840's stock firmware reads and uses the TX word. The stock TK-940
and TK-941 **ignore the stored TX word** for normal conventional
transmissions. With Talk Around **off**, the stock firmware instead uses:

```text
TK-940 normal TX = stored RX frequency - 45 MHz
TK-941 normal TX = stored RX frequency - 39 MHz
```

These are code paths in the **shared AB00 firmware**; the model-strap
input selects which fixed offset to use. With **Talk Around on**,
stock firmware transmits at the stored RX frequency instead.

For example:

| Radio and mode | Stored RX | Normal RF TX |
|---|---:|---:|
| TK-940 stock, Talk Around off | 851 MHz | **806 MHz** |
| TK-941 stock, Talk Around off | 941 MHz | **902 MHz** |
| Either stock 94x, Talk Around on | 902 MHz | **902 MHz** |

This explains why the TK-940 can transmit in its original **806–825 MHz**
TX band even though the conventional codeplug has an **851 MHz base**:
it computes the lower TX frequency *after* decoding the RX word.

### The Enhanced TX Patch

The Toolkit patch changes the TK-940/941 conventional transmit path
to **read the channel's own TX word**, just as the TK-840 already does.
The patched path uses the same **851 MHz + code × 12.5 kHz** interpretation
as conventional RX. It **bypasses Talk Around** and no longer selects
the −45/−39 MHz stock offset. (The Talk Around bit still shows the TA
icon and still adds 36 ms before keying; it no longer changes the
frequency.)

Therefore, **the current patch cannot program conventional TX below
851 MHz**, including the TK-940's original 806–825 MHz transmit band.
It is appropriate for independently programmed conventional RX and TX
**at or above 851 MHz**, subject to the actual model's RF coverage and
PLL lock range. The separate TX Inhibit behavior still matters.

Changing this lower bound would require a **different frequency
encoding and matching firmware changes** (and corresponding CHIRP/Toolkit
support), not simply widening the CHIRP frequency selector.

**AB00 patch implementation evidence:** For the analyzed TK-941 image
with firmware byte sum `B5ECh`, the normal-channel TX patch replaces
16 bytes at `9C6Eh–9C7Dh` (firmware-file offset `1C6Eh`). The patch
finds the channel record with `CALL DE9Ah`, advances to TX word offset
`+03`, loads the word, masks bit 15, and rejoins the existing tuning
path. The patch also changes the separate dealer-test TX path. The
**authoritative executable patch windows for each recognized firmware
revision** are in the Toolkit's `fw_db[]`; do not assume AB00 addresses
apply blindly to every revision.

## 3. Dealer/alignment test frequencies are separate

The dealer/alignment frequency table is a **different data structure**
from conventional channels: four bytes per slot (RX word followed by TX
word), beginning at flash address `E680h`.

For TK-940/941 test-table **frequency words** the bases are:

| Test-table field | Frequency base |
|---|---:|
| RX | **851.000 MHz** |
| TX | **812.000 MHz** |

The stock AB00 dealer transmit code normally derives TX from the test
RX word and the selected fixed offset (or uses RX when its test
Talk Around selection is enabled). The Toolkit's dealer-test TX patch
instead reads the **test TX word**, applying the **812 MHz** base.
This is **not** an alternative encoding automatically available to
ordinary channel TX fields.

The 812 MHz test base by itself does not establish hardware band
coverage or guarantee that a transmitter will lock on every
mathematically expressible frequency. See
[Hardware Communication §6.6](../hardware/PLL.md#66-from-channel-to-synthesizer)
for the exact dealer and conventional code paths.

## 4. What range does CHIRP permit?

As verified against the current
[CHIRP driver](https://github.com/KY4WI/Kenwood_TK-x4x_Chirp_Driver/blob/main/tk840.py):

| Model | Driver's conventional frequency-selection range |
|---|---|
| TK-840 | **370.000–522.000 MHz** |
| TK-940 | **851.000–999.9875 MHz** |
| TK-941 | **851.000–999.9875 MHz** |

For TK-940/941, the **851 MHz lower bound** is required by the
confirmed stock conventional encoding. The **999.9875 MHz upper bound**
is the **driver's configured UI/programming limit**, *not* the upper
mathematical limit of the 15-bit representation and *not* a measured
RF or PLL limit.

With `0x7FFF` reserved as the absent-frequency sentinel, the largest
ordinary code value is `0x7FFE` (32,766), which would *mathematically*
decode on a TK-940/941 as:

```text
851 MHz + 32,766 × 12.5 kHz = 1260.575 MHz
```

**Do not interpret this as an achievable radio frequency.** It says
only what the bit field can represent; the actual RX and TX limits
depend on VCO, PLL, filters, front-end and PA hardware, calibration,
firmware and the specific model. The programmed RF range has **not**
been exhaustively measured over these endpoints. The proposed
[PLL sweep diagnostic](../proposals/Planned_Radio_Diagnostics.md) is intended to
measure individual radios' PLL lock boundaries.

The TK-840's 300 MHz coding base likewise **does not imply** that a
TK-840 can receive or transmit at 300 MHz; its current CHIRP range
is narrower.

## 5. Scope, evidence, and follow-on research

These are **firmware-traced** stock behavior findings for the analyzed
8355 and AB00 images, not a certification of every RF implementation
or untested firmware revision. A software-encodable frequency, a
frequency accepted by CHIRP, a PLL-locked frequency, and a frequency
at which a radio can safely transmit are **four distinct things**.

- [Hardware Communication §6.4](../hardware/PLL.md#64-divider-math-and-tuning):
  actual PLL divider/base arithmetic, model strap and firmware routines.
- [Hardware Communication §6.6](../hardware/PLL.md#66-from-channel-to-synthesizer):
  reading channel words, Talk Around, and conventional/dealer patch sites.
- [TK-840 Codeplug Format §8](TK-840_Codeplug_Format.md#8-frequency-words):
  record offsets, byte layout, sentinel and high-bit behavior.
- [Mixed-Step Frequency Encoding](../proposals/Mixed_Step_Frequency_Encoding.md):
  **research proposal only** for alternative 5/6.25 kHz channel
  representations, with firmware and PLL issues still to resolve.
- [TK-x4x CHIRP driver](https://github.com/KY4WI/Kenwood_TK-x4x_Chirp_Driver):
  software range enforcement, encoding tests and current TX Inhibit
  handling. No change to the driver is implied by this document.
