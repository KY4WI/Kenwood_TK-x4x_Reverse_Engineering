# Kenwood TK-x4x Reverse Engineering

Copyright (c) 2026 Dave, KY4WI

Reverse-engineering notes, tools and test images for the Kenwood TK-840,
TK-940 and TK-941 (UHF and 800/900 MHz mobile radios built around the NEC
µPD78312A CPU). The aim is to document the radios well enough to program,
repair, patch and eventually replace their firmware.

This repository holds documentation and research results only. It does
**not** contain Kenwood firmware, mask ROM dumps or disassembly listings.
Kenwood firmware is copyrighted: please do not upload firmware files, or
links to them, in issues, discussions or pull requests.

## Start here

| If you want to... | Read |
|---|---|
| understand the hardware and how firmware drives it | [Hardware reference](docs/hardware/README.md) |
| read or write a radio over its programming cable | [Programming protocol](docs/programming/Programming_Protocol.md) |
| decode a codeplug (channels, systems, options) | [TK-840 codeplug format](docs/codeplug/TK-840_Codeplug_Format.md) |
| know what frequencies the radios can be programmed to | [Frequency encoding and limits](docs/codeplug/Frequency_Encoding_and_Limits.md) |
| recover a radio whose firmware will not start | [Hard-brick recovery](docs/hardware/Hard_Brick_Recovery.md) |
| recreate a commented disassembly from your own ROM/firmware | [How to generate a commented disassembly](docs/How_to_Generate_Commented_Disassembly.md) |
| write your own firmware | [Replacement firmware guide](docs/hardware/Replacement_Firmware_Guide.md), then the [hardware probes](tools/probes/README.md) |

## Documents

**Hardware** ([`docs/hardware/`](docs/hardware/README.md)) - one reference
in ten chapters, plus companion documents:

- [CPU, memory map and boot](docs/hardware/CPU_Memory_and_Boot.md),
  [I/O map and serial bus](docs/hardware/IO_Map_and_Serial_Bus.md),
  [PLL](docs/hardware/PLL.md),
  [D/A converter, output expander and EEPROM](docs/hardware/DAC_Expander_EEPROM.md),
  [flash programming](docs/hardware/Flash_Programming.md),
  [front panel](docs/hardware/Front_Panel.md),
  [accessory connector, audio and inputs](docs/hardware/Accessory_Audio_and_Inputs.md),
  [resident mask ROM and interrupts](docs/hardware/Resident_ROM_and_Interrupts.md),
  [replacement firmware guide](docs/hardware/Replacement_Firmware_Guide.md),
  [sources and history](docs/hardware/Sources_and_History.md).
- [Front panel link](docs/hardware/Front_Panel_Link.md) - the serial link to
  the panel CPU: timing, resident RAM state, mask ROM entries.
- [Resident mask ROM API](docs/hardware/resident_api/README.md) - each `1Fxx`
  entry point: registers, calling convention, side effects.
- [Hard-brick recovery](docs/hardware/Hard_Brick_Recovery.md) - forcing the
  firmware loader when the flash is bad.

**Programming** ([`docs/programming/`](docs/programming/))

- [Programming protocol](docs/programming/Programming_Protocol.md) - the
  serial handshake, read and write transactions, the tuning session, the
  firmware loader and checksums.
- [Microphone-port serial](docs/programming/Mic_Port_Serial.md) - how the
  stock software serial ports work, their speed limits, and a design for
  faster programming.

**Codeplug** ([`docs/codeplug/`](docs/codeplug/))

- [TK-840 codeplug format](docs/codeplug/TK-840_Codeplug_Format.md) - the
  channel and system data layout, decoded and tested on a TK-840.
- [Frequency encoding and limits](docs/codeplug/Frequency_Encoding_and_Limits.md) -
  how frequencies are stored, the 851 MHz conventional base, model TX
  offsets, the Enhanced TX Patch, dealer test frequencies and CHIRP
  ranges.

**Proposals** ([`docs/proposals/`](docs/proposals/)) - ideas worked out on
paper but not built or tested:

- [Mixed 5 / 6.25 kHz channel steps](docs/proposals/Mixed_Step_Frequency_Encoding.md)
- [Fast codeplug programming](docs/proposals/Fast_Codeplug_Programming.md)
  (the later [microphone-port document](docs/programming/Mic_Port_Serial.md)
  supersedes parts of it)
- [Planned radio diagnostics](docs/proposals/Planned_Radio_Diagnostics.md)

**Reference material:** [NEC CPU and assembler manuals](docs/NEC/README.md)
and [Kenwood and related reference documents](docs/other_docs/README.md).
All third-party PDF manuals are linked rather than bundled.

## Generate a commented disassembly

See [the step-by-step guide](docs/How_to_Generate_Commented_Disassembly.md)
for mask ROM and application firmware, using images you supply yourself.
The generated listings contain copyrighted code and are not distributed here.

## Annotations

Comments and names for code you supply yourself, used with the
[NEC 78K/III disassembler](https://github.com/KY4WI/NEC_78KIII_Disassembler).
They contain no Kenwood code.

- [`annotations/maskrom/`](annotations/maskrom/README.md) - the common mask
  ROM: names, code and data ranges, comments, and a symbol include for
  firmware writers.
- [`annotations/firmware_ab00/`](annotations/firmware_ab00/README.md) - the
  transmit time-out timer in TK-940/941 firmware AB00, and the historical
  two-byte TOT patch.

## Tools and test data

- [`include/`](include/README.md) - names for understood hardware values
  (front-panel commands, keys, beeps) as a C header and a matching assembler
  include.
- [`tools/probes/`](tools/probes/README.md) - small test images flashed into
  the radio to try out the hardware; each image is kept next to its source.
- [`tools/experimental/`](tools/experimental/README.md) - a read-only
  tuning-mode probe for finding the calibration bytes (not yet bench tested).
- [`tools/legacy/`](tools/legacy/README.md) - the first downloader, uploader
  and patcher, kept as protocol references. Use the Toolkit for real work.
- [`channel_tests/`](channel_tests/README.md) - synthetic test codeplugs and
  CHIRP images for checking TX inhibit, receive-only channels, talk around
  and TX offsets, with the expected results.
- [`tools/check_images.sh`](tools/check_images.sh) - the rule for binary
  files in this repository, run on every push: only probe images with their
  source beside them, and channel tests that match their generator. It is a
  safeguard, not proof that every probe binary exactly matches its source.

## Scope

The principal research targets are the TK-840, TK-940 and TK-941. The TK-840N
is a related, documented variant but has **not been bench-tested here**;
do not assume TK-840 firmware patches or advanced operations apply to it.
The TK-940 and TK-941 share the programming and firmware architecture;
their RF hardware sets the usable band.

The TK-841 appears in some third-party compatibility lists, but its band and
model identity have not been confirmed from primary documentation or a real
radio, so it is not treated as a supported target.

Research here is a working record. Items marked open, inferred, experimental
or unverified should be treated that way. Each document's evidence notation
says how a fact was established (service manual, schematic, firmware, mask
ROM, datasheet, bench test).

## Related repositories

- [Kenwood_TK-x4x_Toolkit](https://github.com/KY4WI/Kenwood_TK-x4x_Toolkit) -
  the user-facing backup, restore, firmware validation/transmutation,
  channel and patch utility.
- [Kenwood_TK-x4x_Chirp_Driver](https://github.com/KY4WI/Kenwood_TK-x4x_Chirp_Driver) -
  CHIRP support for the family.
- [NEC_78KIII_Disassembler](https://github.com/KY4WI/NEC_78KIII_Disassembler) -
  the disassembler the annotations are written for.

## License

Except where otherwise noted, the original code, documentation and
annotations in this repository are licensed under the **GNU General Public
License, version 3 only (GPL-3.0-only)**. See [LICENSE](LICENSE).

The `annotations/maskrom/uPD78312A.inc` register definitions keep their
separate MIT license, as stated in that file. None of these licenses grant
any rights to Kenwood firmware or other third-party material.
