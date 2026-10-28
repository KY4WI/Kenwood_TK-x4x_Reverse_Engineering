# TK-x4x bench-test channel files

**These are synthetic TEST codeplugs, not operational channel plans.**
Back up the radio first. Use a properly rated dummy load and an RF
counter/spectrum analyzer/service monitor. Do not radiate test transmissions.
Only transmit on channels intended for the connected model.

## Files

- `TK-x4x_UNIVERSAL_TEST_CH.bin`: one 6144-byte raw codeplug with all
  three sets. System 1 is named TK-840; System 2 is TK-940;
  System 3 is TK-941. Use the Toolkit to load/write this raw file.
  When placed in the wrong model, frequencies will decode differently.
  SYS/group numbers in the table below refer to the UNIVERSAL file;
  each standalone model file puts its channels in System 1.
- `TK-840_TEST_CH.bin`, `TK-940_TEST_CH.bin`, `TK-941_TEST_CH.bin`:
  raw Toolkit channel files with only the appropriate model's System,
  numbered System 1 for convenience.
- `TK-840_CHIRP_UPLOAD_TEST.img`, `TK-940_CHIRP_UPLOAD_TEST.img`,
  `TK-941_CHIRP_UPLOAD_TEST.img`: CHIRP-only model-tagged files. These
  DELIBERATELY have TX Inhibit CLEARED on the two Duplex-off channels,
  to test that the CHIRP driver forces inhibit BEFORE writing.
  The Toolkit rejects these larger CHIRP metadata-bearing files.
  CHIRP may change channel data during upload, notably enforcing
  TX Inhibit on absent-TX channels. Raw Toolkit writes leave data untouched.

System names identify the radio model. Channel names are eight
uppercase ASCII characters describing the test. OFF means absent TX.
INH means TX Inhibit set; TA means Talk Around enabled.

## Expected channels and transmit frequencies

Frequency columns in MHz. "Inhibit" means the TX Inhibit bit is set.
An expected frequency is a PLL/programming expectation, **not** a
guarantee of PLL lock, RF output, or operation within hardware limits.
On the TK-840 the Enhanced TX Patch does not apply.

| Model | SYS/group | Name | RX | Stored TX | Inhibit | TA | Stock TX | Patched TX |
|:--|--:|:--|--:|:--|:--:|:--:|:--|:--|
| 840 | 1/1 | `SIMPLEX0` | 446.0000 | 446.0000 | N | N | 446.0000 | 446.0000 |
| 840 | 1/2 | `TXINHBIT` | 446.0125 | 446.0125 | Y | N | INHIBITED | INHIBITED |
| 840 | 1/3 | `OFF7FFF0` | 446.0250 | 7FFF | Y | N | INHIBITED | INHIBITED |
| 840 | 1/4 | `OFFFFFF0` | 446.0375 | FFFF | Y | N | INHIBITED | INHIBITED |
| 840 | 1/5 | `SPLITTX0` | 446.0500 | 446.0750 | N | N | 446.0750 | 446.0750 |
| 840 | 1/6 | `TALKARND` | 446.0875 | 446.1125 | N | Y | 446.0875 | 446.0875 |
| 840 | 1/7 | `OFF2SIMP` | 446.1000 | 446.1000 | Y | N | INHIBITED | INHIBITED |
| 840 | 1/8 | `LOWEDGE0` | 420.0000 | 420.0000 | Y | N | INHIBITED | INHIBITED |
| 840 | 1/9 | `HIGHEDGE` | 450.0000 | 450.0000 | Y | N | INHIBITED | INHIBITED |
| 940 | 2/1 | `STKOFFST` | 860.0000 | 861.0000 | N | N | 815.0000 | 861.0000 |
| 940 | 2/2 | `TXINHBIT` | 860.0125 | 861.0125 | Y | N | INHIBITED | INHIBITED |
| 940 | 2/3 | `OFF7FFF0` | 860.0250 | 7FFF | Y | N | INHIBITED | INHIBITED |
| 940 | 2/4 | `OFFFFFF0` | 860.0375 | FFFF | Y | N | INHIBITED | INHIBITED |
| 940 | 2/5 | `SPLITTX0` | 859.0000 | 862.0000 | N | N | 814.0000 | 862.0000 |
| 940 | 2/6 | `TALKARND` | 860.0500 | 861.0500 | N | Y | 860.0500 | 861.0500 |
| 940 | 2/7 | `OFF2SIMP` | 860.0625 | 860.0625 | Y | N | INHIBITED | INHIBITED |
| 940 | 2/8 | `LOWEDGE0` | 851.0000 | 856.0000 | Y | N | INHIBITED | INHIBITED |
| 940 | 2/9 | `HIGHEDGE` | 870.0000 | 865.0000 | Y | N | INHIBITED | INHIBITED |
| 941 | 3/1 | `STKOFFST` | 940.0000 | 900.0000 | N | N | 901.0000 | 900.0000 |
| 941 | 3/2 | `TXINHBIT` | 940.0125 | 900.0125 | Y | N | INHIBITED | INHIBITED |
| 941 | 3/3 | `OFF7FFF0` | 940.0250 | 7FFF | Y | N | INHIBITED | INHIBITED |
| 941 | 3/4 | `OFFFFFF0` | 940.0375 | FFFF | Y | N | INHIBITED | INHIBITED |
| 941 | 3/5 | `SPLITTX0` | 939.0000 | 899.5000 | N | N | 900.0000 | 899.5000 |
| 941 | 3/6 | `TALKARND` | 940.0500 | 900.0500 | N | Y | 940.0500 | 900.0500 |
| 941 | 3/7 | `OFF2SIMP` | 940.0625 | 940.0625 | Y | N | INHIBITED | INHIBITED |
| 941 | 3/8 | `HAM927RX` | 927.0000 | 902.0000 | Y | N | INHIBITED | INHIBITED |
| 941 | 3/9 | `LOWEDGE0` | 935.0000 | 900.0000 | Y | N | INHIBITED | INHIBITED |
| 941 | 3/10 | `HIGHEDGE` | 941.0000 | 900.0000 | Y | N | INHIBITED | INHIBITED |

## How to use the tests

1. Save a verified original full backup and channel backup.
2. Write the raw file using the Toolkit (or open the corresponding
   `.img` file in CHIRP when specifically testing the CHIRP upload policy).
3. Read channels back; compare eight-character labels and the model
   decoded RX/TX values. Model-mismatched Systems in the universal file
   will display nonsense frequencies. This is EXPECTED.
4. For the `TXINHBIT`, `OFF7FFF0`, `OFFFFFF0`, and `OFF2SIMP` cases,
   verify PTT does **not** produce RF or key the transmitter.
5. For `SIMPLEX0`, `STKOFFST`, `SPLITTX0`, and `TALKARND`, compare RF
   frequency with the stock or patched column for your model.
6. With `OFF2SIMP` in CHIRP, change Duplex to off and then back to
   simplex. Verify Extra / TX Inhibit stays enabled throughout.
   Only manual clearance should
   permit TX. After upload, read the radio back and verify.
7. For the CHIRP `.img` probe files, first confirm each OFF channel
   appears as Duplex off with **TX Inhibit unchecked** in Extra.
   Upload using CHIRP, then read back. Both absent-word channels
   should now have TX Inhibit enabled; their TX words should be 7FFF.
   This specifically tests the CHIRP upload sanitizer, not the Toolkit.
8. Read back a CHIRP-written raw image and verify an intentionally
   inhibited VALID-TX channel stays inhibited. Valid TX should never
   cause automatic uninhibiting.

## Model/hardware notes

These are expected outcomes for the test channels, not a complete band
or firmware-encoding reference. For the **full** explanation of the
851 MHz conventional base, TK-940/941 stock offsets, dealer/test
812 MHz base, and hardware versus CHIRP limits, see
[Frequency Encoding and Limits](../docs/codeplug/Frequency_Encoding_and_Limits.md).

- TK-840: the normal firmware honors the stored TX word. No Enhanced TX
  firmware patch is applicable. Talk Around should use the RX word.
- TK-940 stock: normal TX = RX minus 45 MHz, ignoring the TX word.
  TK-940 patched: normal TX follows the stored TX word, which must
  encode 851 MHz or above. The stock TK-940 TX split is 806–825 MHz;
  patch targets above 851 MHz may not produce RF on unmodified hardware.
- TK-941 stock: normal TX = RX minus 39 MHz, ignoring the TX word.
  TK-941 patched: normal TX follows the stored TX word. 940/900 MHz
  allows a 1 MHz stock/patched difference on original RF hardware.
- `TALKARND`: stock uses RX for TX, while the 940/941 Enhanced TX
  patch bypasses Talk Around and uses the programmed TX word.
- `HAM927RX` is inhibited intentionally. Its 927 RX / 902 TX
  represents amateur operation only after suitable hardware work and
  with appropriate modified firmware. Stock firmware would derive
  TX as 888 MHz from 927 RX; leave TX Inhibit ON until verified.
- In all three models, valid program words do NOT guarantee PLL lock.
- CHIRP can normalize the `FFFF` absent TX word to `7FFF` on upload.

## Source and integrity

Generated using the channel block format documented in
`KY4WI/Kenwood_TK-x4x_Reverse_Engineering` and the existing CHIRP
driver fixture global-settings block. These are newly constructed
synthetic images, not radio-extracted codeplugs. All `_CH.bin` files
are exactly 6144 bytes. They are intended for controlled bench testing.

An actual radio test has NOT yet been performed on these particular
generated files. Check every frequency and result on your bench.
