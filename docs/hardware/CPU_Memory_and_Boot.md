<!-- CPU, memory map and boot -->
Part of the [TK-x4x hardware reference](README.md) · next: [I/O map and the shared serial bus](IO_Map_and_Serial_Bus.md)

# 0. Foundations: how C code talks to the hardware

## 0.1 SFR access from C `[RE, compiled+verified]`

The following pattern has been compiled with the project's vbcc78k3 toolchain,
linked, disassembled, and checked against the intended 78K/III operations:

```c
typedef unsigned char  u8;
typedef unsigned short u16;

#define SFR8(addr)  (*(volatile u8  *)(addr))
#define SFR16(addr) (*(volatile u16 *)(addr))
```

Verified examples:

```c
/* 16-bit SFR write */
SFR16(0xFF8E) = 0x0067;

/* byte read-modify-write */
SFR8(0xFF01) |= 0x20;
SFR8(0xFF01) &= ~0x20;

/* byte read/test */
u8 test_tmic1_flag(void)
{
    return SFR8(0xFFD0) & 0x80;
}
```

Current vbcc78k3 code generation uses generic pointer/register-indirect accesses
for these expressions rather than the compact native `sfr[]`/`saddr`
encodings. That is a size/speed inefficiency, not a correctness problem. For
strict-timing loops such as PLL or software-serial bit periods, use a small
assembly routine rather than depending on the compiler's generic lowering.

A future backend improvement could recognize constant-address SFR dereferences
and emit native `sfr[]`/`saddr` forms directly. That is an optimization
opportunity, not a prerequisite for replacement firmware.

## 0.2 Calling fixed resident routines `[RE, compiled+verified]`

No-argument resident entry points can be called from C through a fixed-address
function pointer:

```c
typedef void (*fnptr)(void);

void call_gpio_init(void)
{
    fnptr f = (fnptr)0x1F0F;
    f();
}
```

That call pattern has been compiled and verified.

Do not assume the vbcc C ABI matches the resident ROM ABI for routines that take
arguments. The mask ROM frequently uses registers and fixed resident RAM, for
example:

```text
1F24 DAC write:
    A = channel
    X = value

1F2D panel transmit:
    A = outgoing byte
```

Use a small hand-written assembly shim for each such routine.

## 0.3 `resident_api.h` / `resident_api.s` `[RE, compiled+verified]`

The project already has a growing resident-ROM wrapper layer that converts
vbcc's stack-based argument convention into the mask ROM's per-function
register/RAM convention.

Wrappers already verified against both stock callers and the resident routine
implementation include:

```c
void eeprom_seq_read(u8 addr, void *dest, u8 count);
void eeprom_page_write(u8 addr, const void *src, u8 count);
void bu4094_commit(void);
void bu4094_write(u8 value);
```

The original guide correctly withheld wrappers that had not been proven. That
rule remains. Since then, however, the panel path has become substantially
clearer:

- `1F2D` is definitively the resident one-byte panel transmit service.
- the stock application independently implements the same `FE25/FEDA.1/P04`
  transmit sequence at `0x8F22`;
- the external CALLT slot at table address `0x0040` targets `0x8F22`;
- normal application code consumes panel receive bytes from `FE2C` under
  `FEDA.2`.

A C-facing `panel_send_byte(u8)` shim is therefore now justified. The complete
high-level panel command dictionary is still not complete, so a wrapper should
expose raw panel bytes rather than invent symbolic commands that are not yet
proven.

The DAC wrapper (`1F24`) is also sufficiently decoded at the resident level
(`A=channel 1..8`, `X=value`), but any public C shim should still be verified
against the actual assembler/C calling convention before being marked
`[compiled+verified]`.

The cooperative EEPROM entries `1F42`/`1F45` remain more complicated because
they depend on an application callback at `0x8235`; use the simpler blocking
EEPROM API first.

# 1. System architecture and memory map

## 1.1 Main CPU

Main controller:

```text
NEC µPD78312AGF3563BE
78K/III family
12 MHz main-MCU oscillator (per schematic)
8 KiB internal mask ROM
```

The front-panel MCU uses a separate approximately 4.194 MHz oscillator, as
shown in the schematics. These values describe oscillator inputs, not
necessarily the CPUs' internal clock rates. Section 1.5 details the
main-CPU clock-divider evidence; a direct operating-clock measurement has
not been documented here.

The same mask-ROM image is used across the TK-840/TK-940/TK-941 hardware
family. Model/RF-split detection is performed from analog hardware straps.
`[MASK][RE]`

### 1.1.1 Analyzed images used by this project

The principal TK-840 application image used for the detailed firmware work is
the `0x6600`-byte executable region corresponding to CPU `0x8000-0xE5FF`.
Its 16-bit byte sum is `0x8355`, which matches the historical firmware naming
used in this project. One retained copy is identified as
`TK-840_firmware_8355(1).bin`.

The common internal mask-ROM dump used for resident-API analysis covers
`0x0000-0x1FFF`.

When publishing patches or new reverse-engineering conclusions, record the
exact source image/hash used. The family shares a common architecture, but an
observation from one external firmware revision should not automatically be
promoted to every x4x application revision.

## 1.2 CPU address space

```text
0000-1FFF   internal mask ROM
2000-7FFF   normally unused/open external address space
8000-E5FF   external AT29C256 application firmware
E600-FDFF   external AT29C256 codeplug/configuration
FE00-FEFF   internal RAM
FF00-FFFF   SFR space
```

The external flash is a 32 KiB AT29C256. CPU A15 selects the upper 32 KiB
address region; **A15 is not an address input on the AT29C256**, which only has
A0-A14. `[SCH][DS][RE]`

## 1.3 External flash bus

```text
P40-P47       multiplexed AD0-AD7
ALE           latches A0-A7 into the external address latch
P50-P56       A8-A14 -> AT29C256 A8-A14
P57 / A15     external-address-space/chip-select logic
/RD           external read / flash output-enable path
/WR           external write path
P37           board-level flash write-enable/protect control
```

The low address is latched from AD0-AD7 using the board's 74HC373-style
address latch. During the data phase P40-P47 carry D0-D7. `[SCH][RE]`

**Replacement-firmware rule:** do not treat P57/A15 as a flash address bit and
do not attempt to use the hidden physical tail of the AT29C256 through normal
CPU addressing. Internal RAM/SFRs overlay FE00-FFFF (see 1.4 for the internal
breakdown of that range).

## 1.4 Internal RAM layout: stack, working RAM, and register-bank storage

Confirmed from the NEC μPD7831xA/78P31xA datasheet (Figures 1, 2, 7, 8) plus
mask-ROM disassembly of `1F0C` (`clear_resident_ram`, real body at `0x010C`):

```text
FE00-FE1F   stack (32 bytes) -- SP is initialized to 0xFE20 by the mask ROM;
            NOT touched by 1F0C (would corrupt the stack while it's running)
FE20-FE7F   "Available Storage" per the datasheet's own memory map (Fig. 1) --
            genuine free working RAM. 96 bytes.
FE80-FEEF   architecturally register-bank storage (Banks 1-7, 16 bytes each,
            Bank7=FE80-8F ... Bank1=FEE0-EF) but NOT used as live register
            banks by resident or stock code (only Bank 0 is ever selected) --
            so the mask ROM/stock app repurpose it as bonus scratch RAM.
            112 bytes. Cleared by 1F0C along with FE20-FE7F.
FEF0-FEFF   Bank 0 -- the ACTIVE register bank (AX/BC/DE/HL/etc.) at any
            point code is running. NEVER cleared by 1F0C (clearing your own
            live registers mid-subroutine would be self-destructive) -- this
            was an open question in the previous revision of this document
            and is now resolved.
```

`1F0C`'s real body (`mov a,#0x00` / `movw de,#0xfe20` / `mov c,#0xd0`) clears
exactly `0xFE20`-`0xFEEF` (208 = `0xD0` bytes), confirming the `FE20`/`FEF0`
boundaries above byte-for-byte. `[MASK][DS]`

Practical consequences for replacement firmware:

- The previously-catalogued "resident scratch RAM" addresses `FEDA`, `FED9`
  (fall inside Bank 2, `FED0-DF`) and `FEBC-FEBE` (fall inside Bank 4,
  `FEB0-BF`) are borrowed unused-bank storage, not generic free RAM in the
  ordinary sense. They are safe to keep using exactly as the stock firmware
  does (nothing else claims them, since only Bank 0 is ever selected), but
  **do not `SEL RB2` or `SEL RB4`** in replacement code without first
  relocating whatever currently lives in that bank's storage -- switching
  the active register bank there would alias live register contents onto
  the panel-link scratch state (and vice versa).
- The chip's 8 hardware "Macroservice" (DMA-like) channels share their
  pointer/counter storage with Register Banks 0 and 1 (`FEE0H-FEFFH`,
  datasheet Fig. 8, explicit note). Any future use of Macroservice channels
  would collide with Bank 0/1 storage -- and Bank 0 is the live register
  bank per the point above, so Macroservice use is only safe if it is
  planned around that overlap deliberately, not assumed independent.
- Total internal RAM is 256 bytes (`0xFE00`-`0xFEFF`), confirmed directly
  from datasheet text, not inferred.
- **Stack room for replacement firmware (2026-10-16).** The mask ROM and the
  stock firmware both start the stack at `FE20h`, which leaves 32 bytes
  (`FE00h`-`FE1Fh`; below that is the codeplug flash). Hand-written stock
  code fits; our compiled C did not (phase1 about 51 bytes, selftest about
  61, panelprobe about 26), and overflowing made phase1 restart or stop.
  While the application runs, the mask ROM names nothing in `FE43h`-`FEBBh`:
  only its firmware loader and KPG monitor write there (through pointers),
  and both reset SP themselves; `1F36h` copies its codeplug page from
  `FE40h`-`FE7Fh`. So the probes now make the start-up calls with SP at
  `FE20h` (`1F0Ch` clears `FE20h`-`FEEFh`, return address included) and then
  move SP to `FEBCh`, giving 92 bytes down to `FE60h`, with variables at
  `FE40h`-`FE5Fh`. `[MASK][RE]` Proven on the bench: phase1 2026-10-16-2
  ran past the point where it had crashed with the stack at `FE20h`.
  `[BENCH]`

## 1.5 System clock: 6 MHz inferred from 12 MHz oscillator and STBC configuration

The schematics show a **12 MHz oscillator feeding the main MCU**; this is not
a measurement of its internal instruction/system clock. The front-panel MCU
has its own approximately **4.194 MHz oscillator**, with its operating clock
also dependent on that MCU's internal clock configuration.

The following evidence from the NEC documentation and traced stock code
supports a nominal **6 MHz main-MCU system clock** for the analyzed firmware:

1. The datasheet's pin description for `X1`/`X2` states plainly: **"The
   system clock frequency is half the input frequency."** `[DS]`
2. The AC Characteristics table's Note (1) is more precise: **`fCLK` equals
   the oscillation clock `fXX` divided by 2 or 8, as determined by bit 5 of
   `STBC`** (the standby control register, `0xFF44`). The same note states
   the table's own reference condition uses `fXX = 12 MHz`, `fCLK = fXX/2`.
   `[DS]`
3. `STBC` is a protected register, written only through a dedicated
   `MOV STBC,#byte` instruction (confirmed in the NEC 78K/III Instructions
   User's Manual -- an invalid form there triggers an op-code-trap
   interrupt). Firmware tracing found **exactly one** write to `STBC` in
   either ROM: the mask-ROM reset handler, `mov stbc,#0x00` -- and it is
   never written again, by the mask ROM or by the external application.
   `[MASK][FW][RE]`

Bit 5 of `0x00` is `0`, consistent with the divide-by-2 mode in the
datasheet. With a 12 MHz oscillator, the **inferred system clock is 6 MHz**
for the analyzed stock mask ROM and application, assuming the traced `STBC`
configuration is the one in effect. This is supported by code analysis, **not
by a direct frequency measurement**; other firmware or operating modes may
configure the clock differently. `[DS][MASK][FW][RE]`

This lets Timer1's actual bit-clock rate be computed rather than guessed.
Per the timer block diagram (Figure 4), `TM1` is clocked by `fCLK/6` or
`fCLK/128` (mode-selected). Stage-1's own serial code (`stage1.s`) programs
`TMC1 = 0x80` (default clock-source selection, i.e. the first-listed option)
and `MD1 = 0x0067` (103 decimal) -- copied from the mask ROM's own 9600-baud
routine at `0x0F76`. With `fCLK = 6 MHz`:

```text
fCLK/6           = 1,000,000 Hz
period (MD1+1)   = 104 ticks
resulting rate   = 1,000,000 / 104 ~= 9615 Hz
```

9615 Hz against a 9600 baud target is a 0.16% error, consistent with
ordinary UART quantization. The alternative `fCLK/128` would produce roughly
450 Hz, making it inconsistent with the intended 9600-baud setup. Thus the
prescaler and 6 MHz inference reinforce one another, though this calculation
is not an independent bench measurement. `[RE, math-verified]`

**The earlier "persistent serial glitch"** in the debug output (first
characters of a message right, later ones wrong) was not timing: it was
the vbcc string bug (fixed in vbcc backend revision `2945d2e`), which passed a
string's first bytes instead of its address. The debug output was slowed
from 9600 to 2400 bit/s while chasing it; with the bug fixed, 9600 is
worth trying again. `[RE]`

**Also confirmed, and already correctly handled:** `vasm`'s 78K/III backend
already encodes `MOV WDM,#byte` / `MOV STBC,#byte` as the required
"byte + one's complement" protected form (`cpus/78k3/cpu.c`, cross-checked
against all 82 real instances of these instructions across both ROMs, zero
exceptions). Nothing needs to change in the toolchain for replacement
firmware to write either register correctly.

---

# 2. Boot contract with the mask ROM

The mask ROM is not merely a boot stub. It is a resident hardware-support layer
and should be deliberately used by early replacement firmware.

## 2.1 External firmware validity signature

The resident boot code checks:

```text
E580-E586 = "KENWOOD"
```

If the comparison fails, the CPU stays in the resident firmware programming /
recovery path rather than entering the external application. `[MASK][RE]`

A replacement image therefore needs a valid `KENWOOD` signature at E580 if it
is intended to boot normally.

## 2.2 External application entry points

The mask ROM dispatches external application modes through fixed addresses:

```text
8080   normal application
8083   Dealer Mode entry
8086   Panel Tuning / Alignment entry
8089   Clone Mode path
```

Firmware PROGRAM/recovery is resident in mask ROM and does not require an
external application entry point. `[MASK][RE]`

For a first replacement image, preserve these entry addresses even if some
entries are only short branches to common code.

## 2.3 Vector/CALLT area

The first 0x80 bytes of external program space are special infrastructure, not
ordinary free code. The stock application contains CALLT targets, interrupt
stubs, and resident-ROM cooperation there. `[FW][RE]`

**Why (2026-10-14):** `TPF`, bit 1 of the CPU control word `CCW` (SFR
`FF4Eh`), selects where the CPU takes its interrupt vectors and CALLT table
from: the mask ROM's 0000h-007Fh while it is clear (as from reset), the
application's 8000h-807Fh while it is set. The stock application sets it in
its start-up routine, just before it unmasks the panel interrupt; every mask
ROM mode clears it first. Until it is set, CALLT must not be used. Details:
[Front Panel Link](Front_Panel_Link.md), section 6.1.
`[DS][MASK][FW]`

The first replacement firmware should preserve/reconstruct this area
intentionally rather than letting the linker place arbitrary code into it.

Two panel-related external vector stubs are particularly important:

```text
8092   wrapper leading into resident panel ISR 1F33
8097   panel-start/INTE0 routing involving resident 1F30
```

The stock vector arrangement should be retained while replacement firmware uses
the resident panel transport. There is currently no technical need to replace
that transport merely to obtain custom button/display behavior. `[FW][RE]`

## 2.4 Resident application callback at 8235

Mask-ROM cooperative EEPROM services `1F42` and `1F45` repeatedly call external
application address:

```text
8235
```

Stock TK-840 firmware branches from 8235 into application background-service
code. If replacement firmware uses `1F42`/`1F45`, it **must** provide a callable
routine at 8235. A RET-compatible stub is sufficient if no cooperative work is
needed. `[MASK][FW][RE]`

The simpler blocking EEPROM routines `1F18/1F1B/1F1E/1F21` have no such
external callback dependency.

---

# 3. Baseline GPIO state provided by mask ROM

Resident entry `1F0F` establishes the common GPIO baseline. `[MASK]`

```text
Port output latches:
P0   = F1
P1   = 7F
P2   = 7F
P3   = BF

Port mode registers:
PM0  = 60
PM1  = 7B
PM2  = AF
PM3  = 4F

Port mode/function registers:
PMC2 = BF
PMC3 = 3F
```

These values are a much safer starting point than inventing a fresh GPIO reset
state. Some pins use alternate peripheral functions, so do not infer every
physical direction solely from the PM byte without also considering the PMC
configuration and mode in use.

**Boot rule:** during initial replacement-firmware bring-up, call `1F0F` before
directly manipulating shared hardware lines unless there is a specific reason
not to.

---

Part of the [TK-x4x hardware reference](README.md) · next: [I/O map and the shared serial bus](IO_Map_and_Serial_Bus.md)
