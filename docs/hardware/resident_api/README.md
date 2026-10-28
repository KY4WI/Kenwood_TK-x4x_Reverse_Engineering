# Resident mask-ROM API reference

Detailed analysis of the shared TK-840/TK-940/TK-941 resident API trampoline table, covering calling conventions, registers, and hardware side effects.

For the maintained overview, see [TK-x4x Hardware Communication](../README.md), particularly the [resident API quick reference](../Resident_ROM_and_Interrupts.md#18-resident-mask-rom-service-quick-reference). The notes below provide additional detail and may document observations or limitations specific to the analyzed firmware.

- [1F0x: monitor, BCD helpers, initialization](tk840_maskrom_api_1f0x.txt)
- [1F1x: interrupts, model identification, EEPROM writes](tk840_maskrom_api_1f1x.txt)
- [1F2x: EEPROM, DAC, output expander, display](tk840_maskrom_api_1f2x.txt)
- [1F3x: interrupt handlers, CLONE, Flash](tk840_maskrom_api_1f3x.txt)
- [1F4x: cooperative EEPROM, page writer, model ID](tk840_maskrom_api_1f4x.txt)

These documents describe independently recorded analysis, not redistributed firmware. Source-image hashes identify the analyzed revisions.
