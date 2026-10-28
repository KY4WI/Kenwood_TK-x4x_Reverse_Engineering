# benchtest

A front-panel bench test image for the TK-x4x, written entirely in
assembly (`benchtest.s`, next to this file): no C compiler, no CALLT, and `CCW` `TPF` left
clear, so the mask ROM's own interrupt vectors run the panel link exactly
as they do in the mask ROM's own modes.

It answers four questions:

1. Does text garble because of **a send loop**, or because of something
   else? One key sends text through our own loop, another through the mask
   ROM's unrolled routine.
2. What do the **beep numbers** (`B3h` + `01h`-`0Dh`) sound like with the
   audio unmuted?
3. Which **characters** does the display actually draw?
4. What does the **panel send** when a PC types `PROGRAM` into the mic
   jack, and what do panel bytes `84h` and `87h` show?

## Keys

| Key | Test | Display |
|---|---|---|
| Sys UP | text through **our loop**: `02h`, then one `CALL 1F2Dh` per character in a `DBNZ` loop | `LOOP  OK` |
| Sys DN | text through the **mask ROM's unrolled** `panel_text8` (`15C8h`) | `ROM   OK` |
| Grp UP | beep number + 1 (`01h`-`0Dh`, wraps) | `BEEP nn` |
| Grp DN | beep number - 1 | `BEEP nn` |
| Scan | unmute the audio, then send `B3h` + the beep number | `PLAY nn` |
| Aux | send `B4h` (the stock beep stop), mute the audio again | `STOP B4` |
| Vol UP | next page of the character table: the range for a second, then the 8 characters | `C 20-27`, then the characters |
| Vol DN | the panel byte for a second, then send it alone; alternates `84h`, `87h` | `PNL  84`, then whatever the panel shows |

Each key first blanks the display for 0.3 s, so a repeated press visibly
redraws. The start-up screen is `BENCH  1`, sent through the mask ROM's
routine.

**Any other byte from the panel** (not one of the eight keys' press or
release codes) is shown in hex, the last four, newest on the right.
`FF` means empty. Example: `FFFF55A0` after `55h`, then `A0h`.

## What to record

**1. Loop vs. no loop.** Press Sys UP and Sys DN a few times each.

| Sys UP | Sys DN | Means |
|---|---|---|
| `LOOP  OK` | `ROM   OK` | A loop is fine. The old garbling was in the C code (compiler output), not in looping. |
| `LMNOPQRS` (garbled) | `ROM   OK` | Something about a loop itself matters, even in hand-written assembly. |
| garbled | garbled | Neither: look at the image's start-up, compared with the stock one. |

The garbled form seen before was "first character right, then +1 per
position", so `LOOP  OK` would show as `LMNOPQRS` and `ROM   OK` as
`RSTUVWXY`.

Result from the bench: both showed correctly, so loops were never the
problem. The cause was in the compiler: vbcc78k3 passed a string's first
two bytes instead of its address, and reading that unmapped address
gives back its own low byte, hence "+1 per position". Fixed in
the earlier vbcc backend revision `2945d2e`; the C probes are rebuilt with it.

**2. Beeps.** For each number `01`-`0D` (Grp UP/DN, then Scan):

- the sound: high, medium or low tone, one beep or several, how long;
- whether it stops by itself, or keeps going until Aux (`B4h`).

Five of the stock beep routines (`03`, `04`, `05`, `07`, `09`) set no
length, so those are likely the ones that need `B4h`. The audio stays
unmuted from Scan until Aux; if the speaker hisses meanwhile, that is
the receiver audio, not a beep.

**3. Characters.** Press Vol UP twelve times (codes `20h`-`7Fh`, 8 per
page). For each page write down what each of the 8 positions shows. Of
interest: lower case `61h`-`7Ah`, and `!` `"` `&` `'` `.` `:` `;` `<`
`>` `?` `[` `]` `^` `` ` `` `{` `|` `}` `~`, which the stock firmware
or KPG treat specially.

**4. Panel bytes.**

- Vol DN: what `84h` and `87h` show (both untried; `85h` showed
  `"- SCAN -"` and `86h` `"AUTONTEL"`).
- With a PC on the mic jack at **1200 baud, 8N2**, send `PROGRAM`. The
  expected display is `FFFF55A0` (the panel asking for programming mode).
  This image does not act on it, so the radio stays in the test. Anything
  else that appears is just as useful.
- Hold a key down for a few seconds: if the panel sends repeat codes,
  they show up in the hex echo.

## Safety

- Nothing here transmits, writes flash or writes the EEPROM.
- The volume DAC (channel 5, the stock volume control) is set once at
  start-up to `5Bh`, entry 20 of the mask ROM's 32-step volume table.
  DAC settings are lost at power-off.
- `B3h` is only ever sent with a number `01h`-`0Dh`; `B3h` alone or with
  `0Eh` locks the panel up.
- `84h` and `87h` have never been tried. If the panel stops responding,
  power-cycle the radio.

## Build

From this directory:

```sh
VASM=/path/to/vasm78k3_std ./build.sh benchtest
```

It needs a `vasm78k3_std` with the two-short-address fix (an earlier vasm backend revision
`3e822b7`, 2026-10-14): `rx_pop` and the echo history use
`MOV saddr,saddr'`. It includes the mask ROM names and `uPD78312A.inc` from
the repository's `annotations/maskrom/`. The image is exactly
26112 bytes (`8000h-E5FFh`) with the `KENWOOD` signature at `E580h`; the
built image is `benchtest.bin`, next to this file.

The CPU-only emulator (`emu78k3`) was used to check the hex and text
helpers. It has no interrupts or peripherals and does not yet have the
`MOV saddr,saddr'` operand-order fix, so it cannot run the panel parts.
