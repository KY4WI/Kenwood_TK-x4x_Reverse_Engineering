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
notes. A multi-file probe has its own subdirectory and build recipe; its main
source is named after the directory (`panelprobe/panelprobe.c`), so its
image is `panelprobe/panelprobe.bin`. The Makefile discovers these probe
directories automatically.

| Probe | Source | What it tests |
|---|---|---|
| [`benchtest`](benchtest.md) | `benchtest.s` (assembly) | text through a loop vs. the mask ROM's unrolled routine, beep numbers with the audio unmuted, the display's character set, panel bytes `84h`/`87h`, and a hex echo of anything the panel sends (the `PROGRAM` test) |
| `panelprobe` | `panelprobe/` (C) | the panel link: handshake, text, the icon dictionary and the byte picker used for the 2026-10-08 bench sessions (hardware reference 11.10, 11.8) |
| `uartprobe` | `uartprobe/` (C) | first bring-up of the on-chip hardware UART (`P24`/TXD, accessory connector) |
| `baudsweep` | `baudsweep/` (C) | the hardware UART stepped through a ladder of bit rates, to find which ones work |
| [`bankisr`](bankisr/bankisr.md) | `bankisr/` (assembly) | whether a general-register bank (`SEL RBn`) can replace `PUSH`/`POP {ax}` around a real, firing interrupt (the DDS/TMF0 tick) without corrupting the interrupted code's own registers |
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
[**NEC_78K3_Toolchain**](https://github.com/KY4WI/NEC_78K3_Toolchain) --
a public repository with its own build instructions, not something you
need to build yourself from scratch:

```sh
git clone https://github.com/KY4WI/NEC_78K3_Toolchain.git
cd NEC_78K3_Toolchain && ./build.sh    # or: make
# binaries land in bin/: vbcc78k3, vc, vasm78k3_std, vlink
```

The toolchain repository contains the NEC 78K/III backend sources,
the small vasm parser patch, and unmodified upstream source archives.
No separate private vbcc, vasm, or vlink repositories are required.

Clone this repository alongside the toolchain, then build from the
repository root. **No `PROBE=...` setting is needed.** The Makefile
discovers all supported probes and their build recipes:

```sh
git clone https://github.com/KY4WI/Kenwood_TK-x4x_Reverse_Engineering.git
cd Kenwood_TK-x4x_Reverse_Engineering
make list          # list every discovered probe
make panelprobe    # build only panelprobe
make               # build ALL probes
```

You can also run `make -C tools/probes panelprobe`. A probe's binary
appears beside its source, such as
`tools/probes/panelprobe/panelprobe.bin`. Each image is exactly
26,112 bytes (application flash `0x8000-0xE5FF`). The Makefile
automatically uses the toolchain's `bin/` if both repositories are
cloned as siblings. Otherwise, put those tools on PATH or override them:

```sh
make panelprobe VBCC=/path/to/vbcc78k3 \
                VASM=/path/to/vasm78k3_std \
                VLINK=/path/to/vlink
```

`make clean` removes intermediate build files without removing the
checked-in test images. Rebuilds are incremental by probe; the first
`make` in a fresh checkout builds everything.

Each probe has a tested compilation recipe because startup code,
linked modules, RAM layout and assembly requirements vary. For C probes
that recipe compiles all **relevant** `.c` modules with `-O=287`
before assembling and linking them. Compiling every unrelated `.c`
file together would cause conflicting entry points and hardware setups.
The published toolchain already includes the compiler/assembler fixes
needed by these programs.

### Installing a probe on a radio

**Warning:** These are experimental firmware images, not complete
radio operating firmware. Verify that a particular probe is suitable
for your hardware before installing it.

1. Back up your radio's original firmware **and** channel data using
   [TK-x4x Toolkit](https://github.com/KY4WI/Kenwood_TK-x4x_Toolkit).
   Keep that backup somewhere safe.
2. Build the desired probe and locate its `.bin` file (26,112 bytes).
3. Use the Toolkit's **Advanced / Special Options → Development Firmware
   Mode** to load and write that binary. Follow its instructions for
   entering the radio's flashing `PROGRAM` mode and confirming the
   write. **Do not use the normal Guided Firmware Workflow** for custom
   probe images.
4. Run the probe and compare its behavior with the probe's own notes.
   Restore the original firmware before returning the radio to
   normal operation. Development mode does not perform normal-mode
   firmware readback verification; check the radio's displayed checksum
   when prompted.

The Toolkit [development-mode documentation](https://github.com/KY4WI/Kenwood_TK-x4x_Toolkit#development-firmware-mode)
covers flash-page baselines, verification limitations and recovery.
The build commands never initiate a firmware write.

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
   Then adjust the copied `build.sh` once (`template.ld` -> `mytest.ld`,
   `template.bin` -> `mytest.bin`, and the `template` entry in
   `CSOURCES`/`SOURCES` -> `mytest`). The Makefile discovers your new
   directory automatically: run `make mytest` at the repository root.
   The source and image names also satisfy the [image check](#images).

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

## Research artifacts

These probes preserve original source and associated bench observations for
reproducing hardware experiments. The shared `common/` helpers implement
resident-mask-ROM calls, UART/debug output, and watchdog services. Functions
validated on hardware may also be incorporated into the separate
[custom firmware project](https://github.com/KY4WI/Kenwood_TK-x4x_Custom_Firmware).

The C probes depend on the corrected NEC 78K/III compiler and assembler
backends in the [published toolchain](https://github.com/KY4WI/NEC_78K3_Toolchain).
Build using the tools documented above; previous compiler code-generation
bugs are described where relevant to the bench results.
