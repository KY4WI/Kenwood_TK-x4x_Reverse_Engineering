# TK-x4x hardware reference

**Target family:** Kenwood TK-840 / TK-940 / TK-941, which share the NEC
µPD78312A main CPU and its resident mask ROM.

This reference answers one question:

> What does replacement firmware have to do, at the register/API level, to
> operate the hardware safely and usefully?

Unknowns are marked `[OPEN]`. Where newer work replaced an older assumption,
the old assumption is called out rather than silently kept.

The sections are numbered 0-27 throughout, and other documents and source
comments refer to them by number ("section 11.10"); each chapter below holds
a range of them.

| Chapter | Sections |
|---|---|
| [CPU, memory map and boot](CPU_Memory_and_Boot.md) | 0, 1, 2, 3 |
| [I/O map and the shared serial bus](IO_Map_and_Serial_Bus.md) | 4, 5 |
| [PLL and frequency generation](PLL.md) | 6 |
| [D/A converter, output expander and calibration EEPROM](DAC_Expander_EEPROM.md) | 7, 8, 9 |
| [External flash programming](Flash_Programming.md) | 10 |
| [Front panel CPU, display, buttons and beeps](Front_Panel.md) | 11 |
| [Accessory connector, PTT and audio, squelch, model inputs](Accessory_Audio_and_Inputs.md) | 12, 13, 14, 15 |
| [Resident mask ROM services and interrupts](Resident_ROM_and_Interrupts.md) | 16, 17, 18, 19, 20 |
| [Replacement firmware: bring-up, open items, design rules](Replacement_Firmware_Guide.md) | 21, 22, 23, 24, 25 |
| [Sources and revision history](Sources_and_History.md) | 26, 27 |

Related documents in this folder:

- [Front panel link](Front_Panel_Link.md) - the serial link to the panel CPU in
  full detail: timing, the resident RAM state, the mask ROM entries and how
  the application hooks in.
- [Resident mask ROM API](resident_api/README.md) - each `1Fxx` entry point:
  registers, calling convention and side effects.
- [Hard-brick recovery](Hard_Brick_Recovery.md) - getting a radio with bad
  flash back into the firmware loader.

## Evidence notation

- `[SM]` - identified by Kenwood service information.
- `[SCH]` - traced directly from the radio schematic or board/net names.
- `[FW]` - confirmed in the stock external firmware.
- `[MASK]` - confirmed in the common internal mask ROM.
- `[DS]` - confirmed from a component datasheet.
- `[RE]` - reverse-engineered strongly enough to design around and bench-test.
- `[RE, compiled+verified]` - project C/assembly was compiled, linked,
  disassembled, and checked for correct code generation.
- `[BENCH]` - directly exercised on real hardware.
- `[OPEN]` - not yet understood well enough to make a stable firmware contract.

The initial replacement firmware should use the resident mask ROM as a small
hardware-abstraction layer wherever its behavior is already understood. That
keeps the first custom image focused on application behavior instead of
reimplementing every timing-sensitive hardware driver at once.
