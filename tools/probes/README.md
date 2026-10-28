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
| [`template`](template/) | `template/` (C) | not a hardware test -- a starting point for **your own** probe. Panel-link handshake + "HELLO" on the display + a raw-byte hex echo of whatever the panel sends, built on panelprobe's already-proven boot code. See [Writing your own probe](#writing-your-own-probe) below. |

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

The compiler, assembler and linker come from
[**NEC-78K3_Toolchain**](https://github.com/KY4WI/NEC-78K3_Toolchain) --
a public repository with its own build instructions, not something you
need to build yourself from scratch:

```sh
git clone https://github.com/KY4WI/NEC-78K3_Toolchain.git
cd NEC-78K3_Toolchain && ./build.sh    # or: make
# binaries land in bin/: vbcc78k3, vc, vasm78k3_std, vlink
```

Only the patches against the stock vbcc/vasm/vlink releases live there
(plus the pristine upstream tarballs they apply to) -- the full modified
sources stay in private repositories under their license, same as any
other vbcc/vasm backend.

Then point the probes' build scripts at that `bin/` directory, either on
`PATH` or with environment variables:

```sh
VASM=/path/to/NEC-78K3_Toolchain/bin/vasm78k3_std ./build.sh benchtest   # single-file assembly probes
VBCC=/path/to/NEC-78K3_Toolchain/bin/vbcc78k3 \
VASM=/path/to/NEC-78K3_Toolchain/bin/vasm78k3_std \
VLINK=/path/to/NEC-78K3_Toolchain/bin/vlink \
    ./panelprobe/build.sh                                                # a probe in a subdirectory
```

- `build.sh NAME` assembles `NAME.s`, `NAME.S` or `NAME.asm` here into
  `NAME.bin`. A single-file C probe would need a crt0 and a linker
  script, so for now C probes get a subdirectory.
- **History, for context, not something you need to chase down
  yourself:** earlier vbcc passed a string's contents where its address
  was meant (the cause of the old "PQRSTUVW" display), and an earlier
  vasm swapped the source/destination bytes of the two-short-address
  instructions (`MOV saddr,saddr'` and relatives). Both are already fixed
  in the published toolchain repository -- there is no earlier revision
  to avoid, just build from what's published there.
  Shared code is in [`common/`](common/): `resident_api` (C wrappers for
  the mask ROM's `1Fxxh` entries), `uart` and `debug_uart` (debug output
  on CN3, the hardware UART), `debug_serial` (bit-banged debug output on
  P15, which is the mic jack's PTT line), `buildinfo.sh`, and the shared `watchdog` service wrapper.
- `common/buildinfo.sh` asks vbcc, vasm and vlink for their versions and
  writes `build_tools[]`; phase1 prints it at boot (`BUILT WITH: vbcc 0.1,
  vasm 2.0f 0.1, vlink 0.18a`), so a bench report names the compiler that
  made the code that ran.
- Everything includes the mask ROM names and `uPD78312A.inc` from
  [`annotations/maskrom/`](../../annotations/maskrom/).
- Panel bytes have names in [`include/`](../../include/README.md)
  (`PANEL_BEEP_STOP` rather than `B4h`); new code should use them.
  `benchtest` does. `panelprobe` predates them and keeps its own
  `PANEL_CMD_...` names in `panelprobe/hardware.h`, some of them from
  before the bench sessions settled what the bytes do.

## Writing your own probe

[`template/`](template/) is a copy-and-go starting point: a probe that
already boots, syncs with the panel, shows `HELLO` on the display, and
echoes whatever byte the panel sends next as two hex digits. It is not a
hardware test in its own right -- it exists so you don't have to
re-derive the boot sequence or the panel-link handshake to ask your own
question.

1. **Copy the whole directory**, don't start from an empty one:
   ```sh
   cp -r tools/probes/template tools/probes/mytest
   cd tools/probes/mytest
   mv template.c mytest.c
   mv template.ld mytest.ld
   ```
   Then edit `build.sh` to match (`template.ld` -> `mytest.ld`,
   `template.bin` -> `mytest.bin`, and the `template` entry in
   `CSOURCES`/`SOURCES` -> `mytest`) and `check_images.sh`'s naming rule
   (the bullet in [Images](#images) above) is satisfied automatically,
   since the source and the image share the new name.

2. **The only file you need to understand line by line is your own `.c`
   file.** `crt0.s`, `panel.c`, `panel.h` and `hardware.h` are borrowed
   from `panelprobe/` unchanged, by relative path (see `template/build.sh`'s
   `PANELPROBE=` line) rather than copied -- so if panelprobe's boot code
   or panel-link primitives ever get a fix, every probe built this way
   picks it up automatically, and there is only ever one copy to trust.
   `*.ld` is also unchanged boilerplate (every address in it is a real
   hardware constraint, not something your probe gets to choose) --
   you only need to touch it if your probe needs more than the 32 bytes of
   RAM `ramdata_region` sets aside; its own header comment says where the
   spare RAM is.

3. **The image format is handled for you.** The linker script places
   your code at `0x8000`, stamps the mandatory `KENWOOD` signature at
   `0xE580` and the final pad byte at `0xE5FF`, and `build.sh` checks the
   result is exactly 26112 bytes before it will call the build a success.
   You don't need to think about any of that -- just don't remove the
   `kwsig`/`padbyte` sections from the `.ld` file.

4. **The one rule that isn't enforced by the tools:** a global variable
   you write to at runtime must have **no initializer** (so it lands in
   `.bss`/RAM), never an explicit one (which lands in `.data`/flash, and a
   plain write to flash silently does nothing on real hardware). See
   `template.ld`'s header comment for the full explanation. This is a
   review-time rule, not something vbcc78k3 or the linker will catch for
   you -- getting it wrong compiles cleanly and fails silently on the
   bench.

5. **Build with `-O=287`.** `template/build.sh` already passes it to
   every `vbcc78k3` call; keep it if you add more C files. Without it,
   this backend gives every local variable and intermediate value its own
   permanent stack slot for the life of the function, and `main()` -- which
   starts with only 30 bytes of stack before SP runs into the external
   codeplug flash at `0xFE00` -- overflows before reaching its own first
   statement.

6. **Before committing an image, run the image checker:**
   ```sh
   tools/check_images.sh
   ```
   It rejects any `.bin` under `tools/probes/` that doesn't have a
   same-named source file beside it or isn't exactly 26112 bytes, and runs
   again on every push -- a good last check that nothing proprietary or
   malformed got committed.

7. **If your probe sends its own panel commands**, read `hardware.h`'s
   comment on `PANEL_CMD_B3_PREFIX` (`0xB3`) first. Some panel bytes are
   two-byte commands, and sending the first byte alone with nothing to
   follow locks the panel up until the radio is power-cycled. `panel.c`'s
   existing handshake/display-text/mode-announce calls are all safe as
   written; it's new sends of your own that need the check.

8. **Once a result holds up on the bench**, the usual path is the same
   as every other probe here: write up what you found next to your
   source (a `mytest.md`, the way `benchtest.md` does, or a comment block
   at the top of the C file the way the other C probes do), and once the
   function is proven, a clean copy -- not the probe itself -- goes into
   [Kenwood_TK-x4x_Custom_Firmware](https://github.com/KY4WI/Kenwood_TK-x4x_Custom_Firmware),
   which holds only code that has already passed a bench test.

If you get stuck, `panelprobe/panelprobe.c`'s own header comment has the
full blow-by-blow of 31 real-bench "BETA" rounds -- most things that trip
up a first probe (the stack budget, the `.data`/`.bss` gotcha, the
two-byte panel command, the need for the mode-announce before the display
will show anything) were each found the hard way once already, and are
written up there.

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
