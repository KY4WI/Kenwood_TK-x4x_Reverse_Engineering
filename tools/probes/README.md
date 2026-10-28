# Hardware probes

Images flashed into the radio (application flash, `8000h-E5FFh`, 26112
bytes) to find out how the hardware behaves. What they show goes into
the documents in [`docs/`](../../docs/). Once a function has passed a
bench test here, a clean copy goes into the custom firmware
([Kenwood_TK-x4x_Custom_Firmware](https://github.com/KY4WI/Kenwood_TK-x4x_Custom_Firmware)),
which holds only proven code.

## Layout

A small probe is a single source file in this directory, with its image
next to it: `benchtest.s` and `benchtest.bin`, plus `benchtest.md` for its
notes. A probe that needs more than a file or two gets its own
subdirectory, with its own `build.sh`; its main source is named after the
directory (`panelprobe/panelprobe.c`), so its image is
`panelprobe/panelprobe.bin`.

| Probe | Source | What it tests |
|---|---|---|
| [`benchtest`](benchtest.md) | `benchtest.s` (assembly) | text through a loop vs. the mask ROM's unrolled routine, beep numbers with the audio unmuted, the display's character set, panel bytes `84h`/`87h`, and a hex echo of anything the panel sends (the `PROGRAM` test) |
| `panelprobe` | `panelprobe/` (C) | the panel link: handshake, text, the icon dictionary and the byte picker used for the 2026-10-08 bench sessions (hardware reference 11.10, 11.8) |
| `uartprobe` | `uartprobe/` (C) | first bring-up of the on-chip hardware UART (`P24`/TXD, accessory connector) |
| `baudsweep` | `baudsweep/` (C) | the hardware UART stepped through a ladder of bit rates, to find which ones work |
| `stage1` | `stage1/stage1.s` (assembly) | the first image of our own: boots through the mask ROM's start-up without touching RF |
| `phase1` | `phase1/` (C) | model identification, EEPROM/calibration and experimental receive-only PLL/panel bring-up. The model display (`840-B1`) ran on bench hardware; the corrected 450 MHz PLL programming sequence was prepared after an UNLOCK result, but successful lock on that corrected build is not yet documented. Debug UART output is on CN3 (115200 bps, 8N1). This is a probe, not production radio firmware. |

The C probes carry their whole history (each BETA step and why) in the
comments at the top of their main source and `crt0.s`.
Where those comments cite "the hardware doc" with a section number, they
mean the [hardware reference](../../docs/hardware/README.md); its section
numbers have not changed.

From 2026-10-16, test images beep when they start (`BEEP_LOW_HIGH_LOW`;
the stock firmware's power-on beep is `BEEP_POWER_ON`), so a reset can be
heard. Images that drive the panel while printing send their debug output
on the hardware UART (`common/debug_uart.c`): the mic-port version
(`common/debug_serial_asm.s`) polls timer 1 for its bit times, which
panel-link interrupts can stretch, and it drives the mic PTT line.

## Images

Each probe's image is kept next to its source so it can be flashed
without building. **A `.bin` is accepted only if a source file of the same
name sits beside it** (`NAME.c`, `NAME.s`, `NAME.S` or `NAME.asm`) and it
is 26112 bytes. `.gitignore` lets `.bin` files under `tools/probes/`
through; [`check_images.sh`](../check_images.sh) enforces the rest and runs
on every push, so accidental Kenwood images and dumps can be caught by filename/format.
This is **not** a guarantee that binaries match their sources or that renamed
proprietary material cannot be committed. Run it before
committing, and rebuild and commit the image whenever its source changes.

## Building

The tools come from the KY4WI toolchain repositories; have them on `PATH`,
or name them with environment variables:

```sh
VASM=/path/to/vasm78k3_std ./build.sh benchtest     # single-file assembly probes
VBCC=/path/to/vbcc78k3 VASM=/path/to/vasm78k3_std VLINK=/path/to/vlink \
    ./panelprobe/build.sh                            # a probe in a subdirectory
```

- `build.sh NAME` assembles `NAME.s`, `NAME.S` or `NAME.asm` here into
  `NAME.bin`. It needs a vasm with the two-short-address fix (KY4WI/VASM
  `3e822b7`, 2026-10-14). A single-file C probe would need a crt0 and a
  linker script, so for now C probes get a subdirectory.
- The C probes need vbcc (KY4WI/vbcc `2945d2e` or later), vasm and vlink.
  Earlier vbcc passed a string's contents where its address was meant,
  the cause of the old "PQRSTUVW" display.
  Confirmed working with vbcc rev `2945d2e`, vasm 2.0f rev `3e822b7`,
  vlink 0.18a; vbcc `770dbce` and vasm `9756920` only add their revision
  to the banner and build the same bytes.
  Shared code is in [`common/`](common/): `resident_api` (C wrappers for
  the mask ROM's `1Fxxh` entries), `uart` and `debug_uart` (debug output
  on CN3, the hardware UART), `debug_serial` (bit-banged debug output on
  P15, which is the mic jack's PTT line), `buildinfo.sh`, and the shared `watchdog` service wrapper.
- `common/buildinfo.sh` asks vbcc, vasm and vlink for their versions and
  writes `build_tools[]`; phase1 prints it at boot (`BUILT WITH: vbcc rev
  770dbce, vasm 2.0f rev 9756920, vlink 0.18a`), so a bench report names
  the compiler that made the code that ran.
- Everything includes the mask ROM names and `uPD78312A.inc` from
  [`annotations/maskrom/`](../../annotations/maskrom/).
- Panel bytes have names in [`include/`](../../include/README.md)
  (`PANEL_BEEP_STOP` rather than `B4h`); new code should use them.
  `benchtest` does. `panelprobe` predates them and keeps its own
  `PANEL_CMD_...` names in `panelprobe/hardware.h`, some of them from
  before the bench sessions settled what the bytes do.

## History

The probes were written in the custom firmware repository and moved here
(benchtest, panelprobe, uartprobe and baudsweep on 2026-10-15; stage1,
selftest and phase1 on 2026-10-16; selftest was then renamed keytests and
moved to the custom firmware's `examples/`, keeping only its proven parts;
its EEPROM results are now in phase1's output); the custom firmware
repository's
history has every earlier version (the C probes' main sources were
`main.c` there, and phase1 kept its compiled `.s` files instead of
building them). Every probe built byte-for-byte the same as before the
move; the C probes were rebuilt on 2026-10-16 with the VARADR fix (vbcc
`2945d2e`), with no source change.

`common/` started as a copy of that repository's `resident_api.*` and
`debug_serial*`. It is now the working copy: the probes use it, and
pieces that pass a bench test are copied the other way, into the custom
firmware's library.
