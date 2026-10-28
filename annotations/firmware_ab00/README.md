# TK-940/TK-941 AB00 application-firmware TOT annotations

These are original, Unraveled-style comments and symbols for **the transmit
time-out timer in shared TK-940/TK-941 AB00 application firmware**, not the
common 8 KB **mask ROM**. The machine code and original disassembly are not
distributed. Firmware patches described here have **unknown original
authorship**; the preserved files are evidence, not attribution.

## Findings from the original AB00 and the historical TOT-only image

The TK-940/941 firmware uses two separate 8-bit codeplug settings, one for
normal **Dispatch** TX TOT at CPU `E662h` (codeplug offset `0062h`) and
one for **telephone/interconnect** TOT at `E661h` (offset `0061h`).
Their encoding is `(raw + 1) * 15 seconds` for `00h..27h`, yielding
15 through 600 seconds. The comparison at `94F1h` or `94FDh` rejects
raw values `28h..FFh`, substituting `03h` (Dispatch: 60 seconds) or
`0Bh` (telephone: 180 seconds). **In unpatched AB00, `FFh` does not
turn TOT off.** The setup code multiplies `raw+1` by `01F4h` (500) and
loads the resulting 16-bit countdown at `FE40h`; `FE42h` is a prescaler.

The **historical TOT-only patch** is exactly one two-byte edit to AB00:
`81 01` -> `00 00` at CPU addresses `B1AEh-B1AFh` (firmware offsets
`31AEh-31AFh`). At this site the original `BE` branches to `B1B1h`
when the countdown reaches zero. That destination sets `FE7D.0` and
calls `C2DCh`. The replacement `NOP; NOP` falls through to the
immediately following `RET` at `B1B0h`, even on zero. **It skips this
expiration action but does not disable the countdown itself**; the
counter can continue through zero and wrap. The rest of the original
timer setup and expiry processing is unchanged.

A *second* countdown path at `D702h`, gated by `FE78.7`, **also
decrements `FE40h` and can set `FE7D.0`**. It is untouched by that
historical patch. The code alone does not yet establish which operating
modes reach that second path or whether a repeater can still time out
with the historical modification. The full side effects of `C2DCh`
also remain to be traced.

## Producing an annotated listing

The annotation database is
[`tk94x_ab00_tot.inc`](tk94x_ab00_tot.inc). Obtain the original,
unmodified firmware from hardware you own, and prepare a **raw 26,112-byte
(`0x6600`) image** spanning CPU addresses `8000h-E5FFh`. If starting
with Intel HEX, extract only its first `0x6600` firmware bytes and **omit
the separate three-byte checksum/trailer** at HEX offsets `6600h-6602h`.

The expected original firmware is AB00 (16-bit additive sum of the raw
26,112 bytes), with SHA-256
`45e9e8cfb4070fd14a9c6339f97cd19140faf4acf145dc043a9e78c3e8757bcf`
and CRC-32 `6C6A917F`. The database is deliberately CRC-locked to that
original image. It is **not** for the TOT-patched image, other revisions,
or a combined frequency/TOT patch.

With [NEC 78K/III Disassembler](https://github.com/KY4WI/NEC_78KIII_Disassembler):

```sh
78k_disasm -b 0x8000 tk94x_ab00.bin -d tk94x_ab00_tot.inc --listing tot.lst
```

This is a **sparse research database**, not a complete analysis of
application flash: only the identified setup and countdown ranges are
marked as code. Other ranges remain unannotated. The database contains
no Kenwood instruction bytes, just independent labels and comments.

## Enhanced TOT Patch and historical research

The original research proposal to interpret `FFh` as **Off** has since
been implemented in **TKx4xToolkit** as the **Enhanced TOT Patch**.
It adds an Off choice to **Dispatch TOT**, preserving normal 15-600 second
choices; Telephone/Interconnect TOT is unchanged. Enhanced TOT has been
hardware-tested on TK-840 8355 and 9306 and TK-940/941 AB00.

The historical two-byte patch documented above is **not** Enhanced TOT:
it bypasses one timeout-expiry action without adding an Off setting
and leaves a second decrement path intact. Do not confuse the two.

The CRC-locked database in this folder annotates **stock AB00** and the
historical research; it does not match a patched image. See the Toolkit
for the current Enhanced TOT implementation. The original 16-bit timer
limits still apply to hypothetical future changes to timed durations.
