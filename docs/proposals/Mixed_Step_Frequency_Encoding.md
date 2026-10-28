# Mixed 5 kHz / 6.25 kHz channel steps: firmware patch design

**Status: research and design notes only. Nothing here is implemented or
bench-verified as a mixed-step firmware modification.** This is intended for
future work on stock-derived TK-840 (firmware 8355) and TK-940/941 (AB00)
images, not for the public-release CHIRP driver or TKx4xToolkit. It records
ideas, arithmetic, existing entry points, known restrictions, and open tests
so the reasoning does not need to be reconstructed.

References:
- [TK-x4x Hardware Communication](../hardware/README.md),
  especially sections 6.2, 6.4–6.6.
- [TK-840 Codeplug Format](../codeplug/TK-840_Codeplug_Format.md), section 8.
- Locally generated, fully commented disassembly listings for TK-840
  firmware 8355 (`tk840_8355.lst`) and TK-940/941 firmware AB00
  (`tk941_ab00.lst`). These listings are not included in this repository
  because they incorporate copyrighted Kenwood firmware bytes.
  **Do not publish the generated listings or firmware images.** Distribute
  disassembler logic and independent annotations instead.

## 1. Objective and constraints

Support channels preprogrammed at either 5 kHz or 6.25 kHz grid points,
including frequencies between Kenwood's stock 12.5 kHz raster points.
A channel need not change frequency continuously: the existing radio
selects a stored channel and then tunes RX or TX.

Prefer modifying the existing firmware with compact hooks in free Flash,
leaving its channel management, panel, scanning, DAC calibration, VCO
selection, settling, PLL lock checks, and communications operational.
Existing 19-byte conventional records are retained, including independent
little-endian 16-bit frequency words at record offsets **+1 (RX)** and
**+3 (TX)**. The record lookup is unchanged.

A new codeplug encoding is intentionally *not* compatible with the stock
frequency interpretation. Firmware, CHIRP, and Toolkit must all agree on
the format, its model/band bases, absent-frequency sentinel, and migration.

## 2. Verified stock channel-to-PLL flow

The system number is in `FE23h`; channel/group number is in `FE24h`.
`channel_activate` calls `channel_load` and then `tune_rx`.
**`channel_load` does not cache either frequency in RAM.** It caches
tones/options; each RX or TX tuning operation looks up its record again
and reads the appropriate frequency word from Flash.

| Task | TK-840 8355 | TK-940/941 AB00 |
|---|---|---|
| `channel_activate` | `8DEDh` | `8DD2h` |
| `tune_rx` | `9C42h` | `9BE0h` |
| `conv_tune_rx` | `9C59h` | `9BF7h` |
| `conv_rx_word` | `9C80h` | `9C90h` |
| `conv_tx_word` | `9C8Dh` | stock lacks corresponding conventional TX reader |
| `conv_channel_tx` | `9C66h` | `9C61h` |
| `pll_tune_rx` | `9B23h` | integrated into `conv_tune_rx` / `tune_rx_word` |
| `pll_tune_tx` | `9AC1h` | integrated into `conv_channel_tx` |
| `pll_write_r` | `9BA7h` | `9B37h` |
| `pll_write_na` | `9BBCh` | `9B53h` |

**Stock encoding and TX behavior:** TK-840 conventional words use a
300 MHz base; TK-940/941 AB00 words use an 851 MHz base. The firmware
masks bit 15 and uses a 12.5 kHz raster. Stock AB00 ignores the stored
TX word and derives TX from RX (−45 MHz on TK-940, −39 MHz on TK-941).
The existing Enhanced TX Patch uses the TX word but still has the
851 MHz conventional lower bound. **Full explanation, original 441.4
MHz misinterpretation, dealer-test 812 MHz base, and CHIRP limits:**
[TK-x4x Frequency Encoding and Limits](../codeplug/Frequency_Encoding_and_Limits.md).

A proposed mixed-step replacement would change these stock assumptions;
its encoding and PLL calculation must be coordinated across firmware,
CHIRP, and Toolkit rather than stacked blindly on the existing patch.

Both stock images set the PLL reference divider once at startup. Both
have separate dealer/test tuning paths and LTR paths, so conventional
channel hooks alone are not a complete audit.

## 3. PLL hardware and programmable reference steps

PLL is the MC145190 family; 12.8 MHz reference crystal; a serial C
configuration register (8 bits), R/reference register (16 bits) and
A/N register (24 bits).

| Desired PLL step | R divider | R programming word | Notes |
|---|---:|---|---|
| 12.5 kHz (stock) | 1024 | `0x4400` | Stock image setting |
| 6.25 kHz | 2048 | `0x4800` | Exact integer division |
| 5 kHz | 2560 | `0x4A00` | Exact integer division |
| 2.5 kHz | 5120 | `0x5400` | Exact divider, but see PLL range limitation below |

The high R control bits are shown carried over from the verified stock
`0x4400` value; final R/C programming and lock stability require
bench validation. A **6.125 kHz** reference, if that literal value was
intended instead of 6.25 kHz, is *not* produced by an integer divider
of 12.8 MHz.

With `D = 64*N + A`, the synthesizer output is `fVCO = D * fref`.
The low 6 bits of D are A, with the rest being N.

### Important: the stock A/N writer already carries wider D values

Both `pll_write_na` routines read `FE20h`–`FE22h` and clock a 24-bit
A/N word:

```text
first byte  = (D >> 14) OR 0x30
second byte = (D >> 6) & 0xFF
third byte  = D & 0x3F
```

At the instruction level, the first byte incorporates more high-divider
bits than stock arithmetic supplies: **the writer is capable of carrying
18-bit D (FE22h values 0–3), not just 17-bit D (0–1)**. The original
frequency arithmetic generates at most 17 bits (and 16 on the TK-840),
but that is a property of the *caller*, not the serial writer. The
existing writer should therefore be reusable pending a synthetic D=0x2xxxx
or D=0x3xxxx bitstream verification. The 18-bit maximum is 262143.

The writer destroys/overwrites the scratch values in `FE20h`–`FE22h`
and swaps saved registers when restoring them. Newly inserted code must
not assume D remains there after `pll_write_na` returns.

Examples:
- 450 MHz TK-840 TX at 5 kHz needs D=90000 (17-bit), not 36000.
- 900 MHz TX at 5 kHz needs D=180000 (18-bit).
- 900 MHz TX at 6.25 kHz needs D=144000 (18-bit).
All fit within the writer's 18-bit field.

### Switching R dynamically

The stock `pll_write_r` has a hardcoded `movw FE20h,#4400h` at its
entry, followed by a generic 16-clock serialization loop:
- TK-840: hardcoded store at `9BA7h`, serialization begins `9BABh`.
- AB00: hardcoded store at `9B37h`, serialization begins `9B3Bh`.

One option is to put `0x4800` or `0x4A00` in `FE20h` and call the
serialization body after the hardcoded store; another is to patch/replace
the entry point so the word is supplied dynamically. Do **not** call the
original entry without patching it: it will reset the reference to
12.5 kHz. Program R before A/N for the selected raster, restore the
expected scratch state, and retain the stock C/control sequence and
locking/settling work where feasible.

The TK-840 additionally sends C=`6Dh`, waits 1.5 ms, then C=`6Ch`
after each tune; **AB00 does not**, and instead includes a 0.5 ms
wait after serial writes. Don't accidentally copy TK-840's C fast-lock
sequence into AB00.

A cached/shadowed R setting can avoid unnecessary reference changes
within a run of channels on the same grid, but the initial state and
all calibration/dealer/LTR paths must be considered before relying on it.

## 4. Two proposed 16-bit channel encodings

### Option A: bit 15 as step selector (simplest)

```text
bit 15 = step flag: 0 -> 5 kHz, 1 -> 6.25 kHz
bits 14..0 = integer frequency offset from a model-specific base
f = base + (word & 0x7FFF) * selected_step
```

Each RX and TX word can independently select its step. Arithmetic is
particularly simple: extract selector and low 15-bit count, multiply
or shift/add, then add model-specific VCO base.

| Grid | 15-bit offset count, 0–32767 | Coverage from base |
|---|---|---|
| 5 kHz | 32768 points | 0–163.835 MHz |
| 6.25 kHz | 32768 points | 0–204.79375 MHz |

This covers the TK-840's 370–522 MHz development range with a 370 MHz
base. It covers each 940/941's expected narrower RF split with a
suitable **per-model** base, but a single 5 kHz mapping from 800 MHz
only extends to 963.835 MHz; it does not cover all 800–1000 MHz.
Using different 940 and 941 bases sacrifices simple interchangeability
of their raw codeplugs.

A disabled-frequency sentinel must be defined. Reserving
`0x7FFF` and `0xFFFF` can preserve both legacy representations
without affecting the intended model range when appropriately based.

### Option B: blocks of 25 kHz, three-bit position (common 800–1000 MHz)

5 kHz and 6.25 kHz grids coincide every 25 kHz. There are **eight
distinct** grid points in each 25 kHz interval (the common 0 kHz point
appears once). Encode:

```text
block    = word >> 3           // 13 bits, 0..8191
position = word & 7            // 3 bits, 0..7
f        = base + block * 25 kHz + position_offset[position]
```

| position | offset within 25 kHz | reference grid |
|---|---|---|
| 0 | 0 kHz | either |
| 1 | 5 kHz | 5 kHz |
| 2 | 6.25 kHz | 6.25 kHz |
| 3 | 10 kHz | 5 kHz |
| 4 | 12.5 kHz | 6.25 kHz |
| 5 | 15 kHz | 5 kHz |
| 6 | 18.75 kHz | 6.25 kHz |
| 7 | 20 kHz | 5 kHz |

For a base of 800 MHz: words 0–7 encode the first eight positions;
word **8 is block 1, position 0** (800.025 MHz), word 9 is
800.030 MHz, etc. *Values 8, 9, 10, 11 are not being packed into the
three-bit position field.*

8192 blocks span 204.8 MHz: sufficient for a common 800–1000 MHz
encoding and unchanged 940/941 codeplug interchangeability (assuming
the hardware can actually tune the frequencies). Frequency 1000.000
MHz itself is block 8000, position 0, encoded as word 64000.
This scheme has no separately assigned high-bit flag: the selected
position implies the reference grid. At position 0 either reference
works mathematically, provided the LO offset also aligns.

A possible internal PLL-divider calculation is:

```text
5 kHz:    D = VCO_base_D_5k   + 5*block + offset_5k[position]
6.25 kHz: D = VCO_base_D_6250 + 4*block + offset_6250[position]
```

The block multipliers need no general multiply routine:
`5*x = (x << 2) + x` and `4*x = x << 2`.
Use a tiny eight-entry offset/grid lookup and perform multi-byte carries.
The VCO base must include model-specific RX IF subtraction, TX selection,
and the chosen reference step. Reserve an unused upper-range code such as
`0xFFFF` for a disabled frequency; do not automatically interpret
legacy `0x7FFF` as disabled unless it is also explicitly reserved, since
its new mixed-grid value can be a real frequency. Convert old codeplugs.

Advantages: 200 MHz common band and only 16 bits per RX/TX, no duplicate
frequency representation at grid intersections. Tradeoff: small lookup
and additional decode arithmetic.

**Neither encoding is yet selected.** The 25 kHz block approach is more
space-efficient; the step-selector approach is simpler and could be
sufficient for separate band-specific builds.

## 5. How to graft new code onto the original firmware

Do not rewrite all the existing control code. Place one or more new
frequency-to-D routines in verified free Flash and redirect execution
from the stock conventional RX/TX paths. Preserve register conventions,
working memory, carry/condition flags, and return addresses.

Conceptual operation:

1. Find the current conventional record through the existing
   `conv_find_group`; read **all 16 bits** of record +1 (RX) or +3 (TX).
   The stock `conv_rx_word` / `conv_tx_word` helpers clear bit 15;
   interception after that would lose Option A's selector.
2. Decode the selected 16-bit encoding into a desired RF channel and
   appropriate R/divider setting. Account for model strap `FEDAh.0`:
   **set means TK-940**, clear means TK-941.
3. Compute RX LO or TX VCO D, **including upper bits in `FE22h`**.
   Set/check a known register/flag contract.
4. Write/change R for the selected grid (when needed), then put D into
   `FE20h`–`FE22h`.
5. Resume the stock code just ahead of `pll_write_na` so its original
   serial writer, settling/lock handling, and remaining TX/RX sequencing
   execute as before.

Candidate continuation sites from the verified listings:

| Conventional path | TK-840 8355 | AB00 |
|---|---|---|
| RX: at `pll_write_na` call | `9B2Bh` inside `pll_tune_rx` | `9C12h` inside `conv_tune_rx` |
| TX: at `pll_write_na` call | `9AC9h` inside `pll_tune_tx` | `9C89h` inside `conv_channel_tx` |

These are **candidate resume points, not patched instructions**. A
replacement must preserve the actual call stack and expected caller
state; a mid-function branch is not interchangeable with a fresh
subroutine call. The TK-840 `conv_tune_rx` also performs later
`C50Dh`/`C51Ch` setup calls that should still execute. AB00's stock
conventional TX path reads RX, not TX; our hook must explicitly read TX
to implement independent frequency programming. Talk Around semantics
must be deliberately specified rather than accidentally discarded.

Keep the dealer alignment/test paths separate: they use `E680h` service
words and, in the TK-840's case, ROM tuning tables. Choices are to retain
their original 12.5 kHz format and explicitly restore `R=0x4400` when
entering that path, or migrate their encoding too. LTR has separate paths
as well. Inventory *all* call sites before testing.

### Space for hooks

Firmware image covers `8000h–E5FFh` (0x6600 bytes). The codeplug is
`E600h–FDFFh`. Verified stock blank portions:

| Revision | Main contiguous unused range | After identification block |
|---|---|---|
| AB00 | `E328h–E57Fh` (600 bytes) | `E5BFh–E5FFh` (65 bytes) |
| TK-840 8355 | `E450h–E57Fh` (304 bytes) | `E5BFh–E5FFh` (65 bytes) |

The mask-ROM boot loader checks **the seven ASCII bytes `KENWOOD` at
`E580h–E586h`**. The adjacent stock identification strings occupy more
space, but the seven checked bytes do not by themselves make the whole
identification block hardware-reserved. Other bytes could potentially be
reused after an explicit reference audit. Use the verified blank areas
first, and reclaim dead instructions or unused strings if required.

Code *could* technically execute from the Flash-backed codeplug region,
but that risks overwrite by ordinary programming operations and requires
coordinated changes to the firmware, driver, and Toolkit. Avoid unless
space constraints force it.

For AB00 focus on one known verified image rather than trying to
implement a universal runtime relocator for multiple historical
revisions. Existing firmware loader in the internal mask ROM remains
available for recovery.

## 6. Major issue: exact 5 kHz RX on TK-940/941

**This is a hardware/PLL divisibility problem, not a mere instruction
size problem.** The two stock AB00 receiver LO offsets are:

- TK-940: RX − **45.0375 MHz**
- TK-941: RX − **39.0375 MHz**

Both are exact multiples of 6.25 kHz, but *not* of 5 kHz.
Thus an RF frequency on the exact 5 kHz grid generally requires an
RX LO sitting between 5 kHz PLL points. For example, TK-941 receiving
900.000 MHz needs an LO of 860.9625 MHz:

```text
860.9625 MHz / 5 kHz    = 172192.5  // not an integer D
860.9625 MHz / 6.25 kHz = 137754    // exact
860.9625 MHz / 2.5 kHz  = 344385    // exact, but TOO LARGE
```

A 2.5 kHz reference is the finest common divider step for a 5 kHz RF
grid and the stock 37.5 kHz-misaligned LO offset, but D=344385 exceeds
the 18-bit maximum 262143. This also affects the TK-940 at its high RX
LO frequencies. Switching to 2.5 kHz is therefore **not** a drop-in
solution on these radios. Higher-resolution programming code alone
cannot solve it.

At points on the *intersection* of the 5 and 6.25 kHz RF grids
(every 25 kHz), 6.25 kHz PLL reference can synthesize the LO exactly.
For other 5 kHz points, possible research directions include a receiver
frequency error of a few kHz (acceptability depends on IF/discriminator
performance), changing the actual IF offset/hardware behavior, or other
PLL capabilities not yet established. Do not promise exact 5 kHz receive
coverage for TK-940/941 pending a demonstrated solution.

The TK-840 RX LO offset is **45.050 MHz**, exactly divisible by both
5 kHz and 6.25 kHz, and therefore does not have this problem.
The 940/941 *transmit* VCO at a 5 kHz RF point does not involve the
receive IF offset; its D can fit an 18-bit 5 kHz setup in the 800–1000
MHz range. These differences should influence which project/radio is
used for initial bench experiments.

## 7. Validation tasks before implementation or release

- Decide one codeplug format; publish an exact byte-level decoder,
  encoder, base selection policy, and disabled-frequency sentinel.
  Detect custom-format codeplugs and provide a deliberate migration
  from the 12.5 kHz stock format. Old stock codeplugs must **not** be
  silently interpreted as mixed-step words.
- Validate 18-bit `pll_write_na` packing across word-boundaries by
  comparing all 24 clocks at representative D values, not just at
  frequencies that happen to lock.
- Confirm `0x4800` and `0x4A00` R programming, C settings, settling
  times, and actual lock on hardware. Establish whether R needs
  reloading every tune or only upon step changes.
- Trace and preserve each RX/TX path, including Talk Around, LTR,
  dealer/test and initial tune. Decide the semantics of the existing
  Enhanced TX Patch versus the new custom patch.
- Verify RAM/register/flag preservation at both hook/return points,
  including `FE20h`–`FE22h` clobbering by the writer and interrupts.
- Investigate the AB00 5 kHz **receive** feasibility independently;
  exact 5 kHz TX and exact 6.25 kHz RX should not be confused with a
  proven full 5 kHz transceiver capability.
- Preserve the `KENWOOD` boot marker and verify all other overwritten
  bytes are unreferenced. Build a revision-specific, auditable binary
  transformation (Kenwood firmware remains supplied by the user).
- Validate actual RF output/input, PLL lock, TX inhibit and channel
  changes with suitable test equipment. Keep an unmodified restorable
  firmware image.

**Implementation preference:** initially prototype the TK-840 8355
channel hook and dynamic PLL reference; then approach AB00 with its
distinct RX-LO limitation and stock fixed-TX path. Neither is part of
the current CHIRP public beta.
