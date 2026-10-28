# TK-x4x mask-ROM annotations and symbols

This directory distributes independently written annotations and symbols for the common NEC µPD78312A mask ROM in the Kenwood TK-840, TK-940, and TK-941. It does **not** distribute mask-ROM or firmware code.

Use the [NEC 78K/III disassembler](https://github.com/KY4WI/NEC_78K3_Disassembler) (`78k_disasm`) with a ROM image obtained from your own hardware to produce a commented listing.

## Contents

- `tkx4x_maskrom.inc`: named addresses, code/data ranges, constants, and research comments used by the disassembler.
- `tkx4x_maskrom_symbols.inc`: names as assembler `.equ` definitions for use in other independently written firmware.
- `uPD78312A.inc`: processor register definitions, under its own MIT license.
- `tkx4x_macros.inc`: macros the listing uses for repeated code (so far `KICK_WDT`, the two-write watchdog restart). The database includes it; the symbols file doesn't need it. Requires `78k_disasm` 1.0 or later.

For complete mask-ROM and application-firmware instructions, see
[How to generate a commented disassembly](../../docs/How_to_Generate_Commented_Disassembly.md).

## Generate a listing

With an 8,192-byte mask-ROM dump and the annotated database:

```sh
78k_disasm mask_rom.bin -d tkx4x_maskrom.inc --listing my_maskrom.lst
```

The disassembler validates the dump against the database's CRC-32 (`0x4BCFCF73`). It refuses a mismatched image unless `--ignore-crc` is explicitly specified. Output files must not already exist.

To generate assembler source from your own ROM image:

```sh
78k_disasm mask_rom.bin -d tkx4x_maskrom.inc --source my_maskrom.s
```

The companion 78K/III vasm backend can reassemble this source. The assembler backend is separate from the disassembler.

## Contribute annotations

Prepare a commented listing against your own ROM using `78k_disasm`. After editing that local listing, extract its annotations into new database and symbol files:

```sh
78k_disasm --extract my_maskrom.lst --db updated_maskrom.inc \
    --symbols updated_maskrom_symbols.inc
```

Review the changes before replacing existing annotation/symbol files. Contribute only the resulting annotation and symbol data, not the listing or ROM image. The `[FW]`, `[RE]`, and `[OPEN]` tags distinguish findings from inferences and open questions.

**Copyright:** A disassembly of Kenwood firmware remains subject to the copyright on the underlying code. Do not upload ROM dumps, reconstructed executable listings, or firmware images to this repository.
