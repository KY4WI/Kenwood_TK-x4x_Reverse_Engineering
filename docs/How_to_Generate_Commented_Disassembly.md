# How to generate a commented disassembly

The repository distributes **annotations and symbols**, not Kenwood ROM or
firmware bytes. To reproduce a listing locally, obtain your own lawful image
and build [NEC 78K/III Disassembler](https://github.com/KY4WI/NEC_78KIII_Disassembler)
with `make`. Put its `78k_disasm` executable on your `PATH` or use a
full executable path. Run the commands below from this repository's root.

## Common 8 KB mask ROM

Use a raw **8192-byte** image based at CPU address `0000h`:

```sh
78k_disasm mask_rom.bin -d annotations/maskrom/tkx4x_maskrom.inc \
  --listing maskrom_commented.lst
```

The mask-ROM annotation database validates CRC-32 `4BCFCF73`. If it
rejects the image, verify the dump; do not routinely bypass the CRC check.
To generate assemblable source without address/hex-byte columns:

```sh
78k_disasm mask_rom.bin -d annotations/maskrom/tkx4x_maskrom.inc \
  --source maskrom_source.s
```

The result includes the published labels, code/data ranges, and research
comments. See [mask-ROM annotations](../annotations/maskrom/README.md).

## TK-940/TK-941 AB00 firmware: annotated TOT routines

Use an unmodified, stock **26112-byte** (`0x6600`) AB00 application
firmware image, loaded at CPU address `8000h`:

```sh
78k_disasm -b 0x8000 firmware_ab00.bin \
  -d annotations/firmware_ab00/tk94x_ab00_tot.inc \
  --listing ab00_tot_commented.lst
```

This CRC-locked database contains **only the analyzed transmit time-out
timer routines**. It does not produce a fully commented application
firmware disassembly, and it does not match patched AB00 or other
revisions. See [AB00 TOT notes](../annotations/firmware_ab00/README.md).
If you start with Intel HEX, first export the correct raw application
area, excluding the separate HEX checksum/trailer bytes.

## Other application firmware: first-pass listing

For a raw `0x6600`-byte application image with no corresponding annotation
database, use the known mask-ROM API names to get started:

```sh
78k_disasm -b 0x8000 firmware.bin --vectors 0x8000 --callt 0x8040 \
  -i annotations/maskrom/tkx4x_maskrom_symbols.inc \
  -t 0x8080 -f --listing firmware_initial.lst
```

This produces an **initial automatic analysis**, not a verified full
commented firmware listing. To make a reusable database:

```sh
78k_disasm --extract firmware_initial.lst --db firmware_annotations.inc
```

Edit your local listing, then re-extract annotations to a **new** path.
For the full editing/extraction process, see the disassembler README.
The disassembler refuses to overwrite existing outputs.

## Copyright

Your generated listing or source contains Kenwood instruction bytes.
Keep firmware, mask-ROM dumps, generated listings and reconstructed source
out of this repository. Only contribute original comments, symbols,
annotation databases and independently derived research.
