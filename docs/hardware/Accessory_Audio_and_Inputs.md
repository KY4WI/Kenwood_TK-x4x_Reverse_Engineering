<!-- Accessory connector, PTT and audio, squelch, model inputs -->
Part of the [TK-x4x hardware reference](README.md) · previous: [Front panel CPU, display, buttons and beeps](Front_Panel.md) · next: [Resident mask ROM services and interrupts](Resident_ROM_and_Interrupts.md)

# 12. Accessory connector, horn output, and reusable commercial I/O

The commercial accessory hardware is worth preserving. Replacement firmware
should model physical capability instead of treating these functions as
commercial-radio baggage.

## 12.1 Horn / programmable sink output

The `HOR` accessory output is driven through the board's control/output logic
and transistor path (including Q13/Q14 in the service information). `[SM][SCH]`

Electrical behavior documented for the factory function:

- low-side/open-collector style sink;
- stated sink capability approximately 800 mA;
- ignition-related circuitry can inhibit factory horn operation.

Useful replacement-firmware roles include an external alert, relay driver,
station-control output, fan relay, call annunciator, or general programmable
open-collector output.

Do not interpret the 800 mA figure as permission to connect arbitrary inductive
loads without appropriate flyback protection and wiring.

## 12.2 Ignition, hook, and configurable accessory behavior

The radio already provides:

- ignition sense;
- external hook input;
- external PTT;
- detector/baseband audio;
- data modulation input;
- external speaker audio;
- serial/data-control pins;
- squelch/busy signaling;
- link-complete/output-expander signaling.

These can be reused for ham-radio control policies such as scan inhibit,
monitor, profile selection, repeater reverse, controller I/O, COR, and external
automation.

Factory AUX assignments include horn-alert/manual-relay style behavior. That is
additional evidence that the external control path was intended to switch real
loads/events and can sensibly be exposed as a generic programmable output in
replacement firmware.

## 12.3 KCT-19 / 15-pin accessory interface

| Pin | Name / alternate | Direction | Factory meaning | Replacement-firmware use |
|---:|---|---|---|---|
| 1 | HK / BUSY | In or Out | Hook / system-busy | digital input or busy/COR output |
| 2 | ME / SPM | Ground or In | Mic ground / speaker mute | audio ground or external mute |
| 3 | IGN | In | Ignition sense | general switched input/interlock |
| 4 | DEO | Out | Receiver detector output | discriminator/baseband audio |
| 5 | MI / TXS | In or Out | External mic / TX sense | external TX audio or TX-state output |
| 6 | E | Ground | Ground | ground |
| 7 | SB | Power out | Switched B+, about 13.4 V, max 1 A | accessory power |
| 8 | PTT | In | External PTT, active low | external PTT/controller |
| 9 | DI | In | Data modulation input | packet/data/direct modulation |
| 10 | HOR | Out | Horn alert | programmable sink output |
| 11 | SQ | Out | Logic squelch, active low | COR/squelch output |
| 12 | SP | Out | Speaker audio | external speaker/audio |
| 13 | LOK | Out | Link-complete pulse, active low | event/pulse output |
| 14 | RXD / MM | In | Serial RX / mic mute | serial input or mute input |
| 15 | TXD / DTC | Out/In | Serial TX / data-TX control | serial output or data control |

`[SM][SCH][RE]`

Pins 14/15 are the real on-chip hardware UART (`P24/TXD`/`P25/RXD`) -- see
12.4 for the register-level detail and, importantly, for the bench-confirmed
caveat that this path is **not** casually reachable on a stock, unmodified
radio the way the rest of this connector's pins are.

Some roles depend on jumpers/options and cannot be used simultaneously.

A replacement application should expose capability-oriented configuration, for
example:

```text
pin 1: DIGITAL_INPUT_HOOK or DIGITAL_OUTPUT_BUSY
pin 5: EXTERNAL_MIC_AUDIO or TX_ACTIVE_OUTPUT
pin 10: OPEN_COLLECTOR_USER_OUTPUT or HORN_ALERT
pins 14/15: ASYNC_SERIAL or MUTE/DATA_CONTROL
```

## 12.4 The on-chip hardware UART is real and wired to the accessory interface

`P24/TXD`/`P25/RXD` (section 4) are not just schematic labels -- they are the
dedicated pins of a genuine on-chip async serial peripheral, completely
separate from Timer1 and from the front-panel link. On this radio they route
to the internal accessory connection used by the KCT-19 path rather than to
the normal microphone/programming connector. A stock radio therefore does
**not** provide convenient external access to this UART unless the appropriate
accessory hardware/harness is installed or the radio is modified. `[SM][SCH][DS][MASK][FW][BENCH]`

This makes the hardware UART very useful on an equipped development radio, but
it is **not** the project's universal or no-modification debug/service port.
For testing on an otherwise-stock radio, the existing programming/microphone
interface involving `P15`/`P22`/`P23` remains the externally accessible path.
Keep the two interfaces conceptually separate.

**Register map** (datasheet Table 1, confirmed distinct from the timers --
this peripheral has its own dedicated baud-rate generator, so it does not
depend on `TM1`/`MD1` at all): `[DS]`

| Symbol | Address | Function |
|---|---|---|
| `SCM` | `FF50H` | Serial communication mode register |
| `SCC` | `FF52H` | Serial communication control register |
| `BRG` | `FF53H` | Baud rate generator |
| `RXB` | `FF56H` | Serial communication receive buffer (read-only) |
| `TXB` | `FF57H` | Serial communication transmit buffer (write-only) |

Interrupt flags: `SEF`/`SRF`/`STF` (serial error/receive/transmit, section
19.1) via `SEIC`/`SRIC`/`STIC` at `FFDAH`/`FFDCH`/`FFDEH`. `P27/CTS` is tied
to ground on this board (section 4), which is consistent with the UART
running in plain asynchronous mode with no flow control; `P26/SCK` is
repurposed as the microphone-mute line rather than used as the serial clock.

**The mask ROM initializes this UART unconditionally, every boot,** in the
same instruction block that contains the project's single confirmed `STBC`
write (section 1.5): `[MASK]`

```asm
009E  mov  sfr[SCC], #0x04
00A1  mov  sfr[BRG], #78          ; 78 = 0x4E
00A4  sel  rb0
00A6  mov  stbc, #0x00            ; the same single STBC write documented in 1.5
00AA  mov  sfr[MM], #0x07         ; memory expansion mode
      ... (RFM, UDCC0/1, CRC, CUIM, CR11, PWM0/1, CPTM, FRCC, RTPC, MD1,
           TMC0/1, ADM, TBM -- unrelated peripheral baseline in between)
00E9  mov  sfr[SCM], #0xCD        ; still inside the same unbroken reset
                                  ; routine (ends in a bare `ret` at 00ECH)
```

The mask ROM also contains its own low-level byte-primitive instructions
for this UART (`mov sfr[TXB], a` at mask-ROM address `0558H`, `mov a,
sfr[RXB]` at `0579H`). `[MASK]`

**The external stock application independently confirms the same UART is a
live, working serial port with a real protocol on it**, not merely
initialized and left idle. Two clean polled byte-primitive routines exist at
fixed addresses, both correctly servicing the watchdog while they wait
(compare against the kick idiom in section 19.3): `[FW]`

```asm
; sub_8178: send_byte(A) -- wait for STIC.IF (transmit done), then send
8178  clr1  sfr[STIC].7        ; clear the transmit-done flag
817B  mov   sfr[TXB], a
817D  KICK_WDT                 ; service the watchdog while polling
8185  bf    sfr[STIC].7, 817D
8189  ret

; sub_818A: recv_byte() -> A -- wait for SRIC.IF (receive full), then read
818A  KICK_WDT                 ; service the watchdog while polling
8192  bf    sfr[SRIC].7, 818A
8196  clr1  sfr[SRIC].7        ; clear the receive-full flag
8199  mov   a, sfr[RXB]
819B  ret
```

These two addresses are not incidental -- they are exactly the resolved
targets of `callt [0x0017]` and `callt [0x0019]` in the project's own
already-validated external CALLT table (section 17.1: table address `006E`
`->` `8178`, table address `0072` `->` `818A`). That table was independently
cross-checked against 21 real DAC call sites in the prior session, so this
resolution is trustworthy, not a coincidence of address arithmetic. In other
words, `callt [0x0017]` is the application's shared "send one byte over the
hardware UART" primitive, and `callt [0x0019]` is "receive one byte." `[FW][RE]`

**A real command dispatcher for this UART exists in the external firmware**,
at `0xb1d3`-`0xb22c`. It polls `SRIC.IF`, reads a command byte from `RXB`,
and switches on it: `[FW]`

```asm
B1D3  bt   sfr[SRIC].7, B1D9
B1D9  clr1 sfr[SRIC].7
B1DC  mov  a, sfr[RXB]           ; A = received command byte
B1E1  cmp  a, #'C'
B1E3  bne  B202
B1E5  call sub_B333              ; unresolved validation/checksum helper
B1E8  bne  B200
B1EA  set1 ram_FEDC.2
B1EC  mov  a, #0x06              ; ACK
B1EE  callt [callt_sub_8178]           ; send_byte(0x06)
      ... (RAM/state setup, then:)
B1FD  mov  sfr[SCC], a           ; SCC rewritten -- mode/baud changes here
B1FF  ret

B202  cmp  a, #'O'
      ... (structurally identical: validate, ACK, then rewrite SCC) ...

B22C  clr1 ram_FEDC.2
B22E  mov  a, #0x15              ; NAK
B230  callt [callt_sub_8178]           ; send_byte(0x15) for any unrecognized byte
```

`0x43`/`0x4f` (`'C'`/`'O'`) and the `0x06`/`0x15` ACK/NAK-style replies are
read directly from the disassembly, as is the control flow. **Do not identify
this as the normal PC clone/programming-cable protocol.** That path uses the
separate conditioned `P14`/`P15`/`P22`/`P23` interface documented elsewhere.
Because this UART is routed to the accessory/KCT-19 side, the safer current
interpretation is an **accessory/data-terminal control protocol** -- consistent
with the radio's MDT/data-accessory hardware -- but the exact purpose of
`'C'`/`'O'` is still unproven. After a recognized command the UART's `SCC`
register is rewritten, suggesting a mode or baud change within that accessory
protocol. The exact post-ACK `SCC` value, the `sub_B333` validation helper, and
any additional commands remain open and are recorded as open item 12 below.
`[FW][RE][OPEN]`

**Practical conclusion for replacement firmware:** a real, watchdog-safe,
byte-level UART driver already exists as a template in the stock image
(the `0x8178`/`0x818a` pair above). On a development radio that already has
the accessory UART brought out, this is an excellent debug/bring-up console:
it avoids `P15` double duty and cannot suffer the polled bit-bang-loop jitter
identified in section 1.5. It should **not**, however, replace the programming-
port debug path as the general project interface, because using it on a normal
radio requires optional accessory hardware or a hardware modification.

**Baud rate (2026-10-14, from the NEC user's manual, section 4.4):** the baud
rate is `fCLK / (2^(n+2) x G)`, where `n` is `SCC` bits 2-0 and `G` is `BRG`
(1-255). With `fCLK = 6 MHz`, the mask ROM's `SCC=0x04`, `BRG=78` give
**1202 baud, i.e. 1200**, and `SCM=0xCD` is asynchronous, 8 data bits, no
parity, **2 stop bits**, receive and transmit enabled. Keeping `BRG=78`,
`SCC` = 4, 3, 2, 1, 0 gives 1200, 2400, 4800, 9600, 19200 baud (the manual's
own table). `[DS][MASK]`

**Bench-confirmed (2026-10-16):** our own code sends clean text at every
speed tried, all 0.16% fast by the formula: `[BENCH]`

| bit/s | `SCC` | `BRG` |
|---:|---:|---:|
| 1200 | `04h` | `4Eh` (78) |
| 9600 | `00h` | `9Ch` (156) |
| 19200 | `00h` | `4Eh` (78) |
| 38400 | `00h` | `27h` (39) |
| 57600 | `00h` | `1Ah` (26) |
| 115200 | `00h` | `0Dh` (13) |

(`baudsweep` for all six, `uartprobe` for 1200; framing 8N2, `SCM = CDh`,
set once, then only `SCC`/`BRG` changed between speeds.) On Dave's bench
radio the UART is taken from CN3 pin 2 (RXD) and CN3 pin 3 (TXD). CN3 pin
1 is LOK (the link-complete pulse of section 12.3), also duplicated at
CN2 pin 1, so its trace is cut there and that pin tied to ground instead
-- putting GND, RXD and TXD on three adjacent pins of one connector (CN3)
for a single 3-pin tap, rather than a separate wire out to CN4 pin 2
(ground) elsewhere on the board. This leaves a second serial port
connected alongside the mic-jack one. `[BENCH]`

Earlier results here were wrong because of a compiler bug, not the
hardware: before vbcc backend revision `2945d2e`, C code read a string's or table's
contents where its address was meant. `uartprobe`'s first run, recorded
as "Hello world! plus the alphabet", was really the open bus counting up
from the address `"He"` (`6548h`): `H I J K ...`. `baudsweep`'s "garbage
at every rate", blamed on rewriting `SCM`'s `TXRDY` bit, came from reading
its speed table from the wrong place.

**The `'C'`/`'O'` dispatcher, decoded (TK-941 `AB00h` image, `B28Fh`; the
`0xb1d3` code above is the TK-840 image's copy):** `[FW]`

- The UART runs at 1200 baud, 8N2, from reset. A received `'C'` (43h) or
  `'O'` (4Fh) must be followed by one more byte `r` = 1-5 (the `0xb333`
  helper's job, `B41Ch` in `AB00h`); anything else gets `0x15` (NAK).
- On a good pair the radio sets `FEDC.2`, sends `0x06` at the old rate,
  waits for a timer tick, then writes `SCC = 5 - r`: the PC picks 1200
  (`r`=1) up to 19200 (`r`=5) baud; `r`=4 is 9600.
- `'O'` additionally sends `0x8E` to the front panel, shows `"PROGRAM "`
  and sets `FEDC.3`.
- While `FEDC.2` is set, every byte sent to the front panel is also copied
  to the UART (in `panel_send_byte`), and the radio reports state changes
  with single bytes `0x76`/`0x77` (expander `RLO` bit) and `0x78`/`0x79`
  (`P0.5`).
- After the connect, `0x70`/`0x71` drive `P1.5` low/high (as an output or
  input), `'E'` sends `0x06` and jumps to the reset address, and `'S'`
  enters the application's own memory session at `80AAh`. That session
  disables interrupts, re-runs the start-up (keeping the UART rate), shows
  `"PROGRAM "`, sends `0x06`, then only polls the UART for `'R'`, `'W'` and
  `'E'` (`'E'`: ACK and restart). Nothing else of the radio runs meanwhile.
  Other bytes are passed to the same handlers as front-panel key codes
  (`0x98`, `0x99`, ...).

The UART still goes to the accessory/KCT-19 side, so this is not the
microphone-port programming path. That path is the mask ROM's KPG monitor,
entered when the front panel sends `0x55 0xA0`. The application's own
tuning session (`0x55 0xA2`, or `0xA1` after erasing the codeplug) uses
the same microphone-port lines; see
[Programming Protocol](../programming/Programming_Protocol.md).

---

# 13. PTT, RF keying, audio, and signaling

## 13.1 Distinguish PTT requests from RF KEY

A PTT input is only a **request to transmit**. RF is actually enabled through:

```text
P17 = KEY, active high
```

`[SM][FW]`

Replacement firmware should normalize multiple request sources:

```c
enum ptt_source {
    PTT_NONE = 0,
    PTT_ACCESSORY,          /* P10 */
    PTT_OPTION_TRANSPOND,   /* P06 */
    PTT_PROGRAM_OR_MIC_PATH /* P15/P23 conditioned path; mode details OPEN */
};
```

Do not assert KEY until PLL, VCO state, calibration, frequency limits, and any
thermal/power/interlock requirements are satisfied.

C-facing notes from the earlier communication guide remain important:

- `P10` is the external/accessory PTT input. The accessory connector presents
  this as active low.
- `P06` is an option/transpond PTT request and should be treated separately
  from a human PTT.
- `P15` participates in a mode-dependent programming/PTT path. Do not freeze it
  as a simple input or output until the normal-operation direction/path is
  completely traced.
- `P14/P15` and `P22/P23` are conditioned/cross-connected PC/clone/PTT paths.
  Model the logical function, not merely four independent GPIO bits.

A good application abstraction is `enum ptt_source`, with policy deciding
whether each request is allowed to key RF.

## 13.2 Microphone mute

```text
P26 = microphone mute control
```

`[SM]`

Exact active polarity and interaction with accessory/data modes should be
bench-verified before assigning a public firmware API. `[OPEN]`

## 13.3 Low-speed TX signaling

```text
P34/PWM0 = QT/DQT / low-speed signaling waveform output
```

This is not the M62363 DAC output. P34 generates the signaling waveform; the
analog circuitry filters/couples/sums it into the transmit modulation path,
while DAC-controlled calibration sets levels elsewhere in the chain.
`[SM][FW][RE]`

Physically, on the radio's 64-pin plastic QFP (14 x 20 mm, the `GF` package
of the µPD78312AGF-xxx-3BE; manual sec. 1.3.1, page 1-7), `P34`/`PWM0` is
**pin 33**, right after `AVss` (pin 32) and before `P35`/`PWM1` (pin 34).
`[DS]`

*Correction (2026-11-01):* an earlier version of this section gave **pin 39**.
That is the number on the 64-pin shrink DIP/QUIP drawing (page 1-6), a package
this radio does not use. The `GF` part is the QFP, so pin 33 is the pin to
probe.

This path is appropriate for:

- CTCSS/PL encode,
- DCS encode,
- other low-rate signaling experiments.

### 13.3.1 The coupling filter, traced on the bench radio

From P34 into the modulation summing op-amp: `[SM][BENCH]`

```text
P34 --[22k]-- A --[10k]-- B --[4.7uF]--[100k]-- op-amp summing input
             |                       |
          4700pF to GND      0.01uF and 22k, both to GND
```

Two poles (the two caps; the 22k-to-ground at B and the 100k into the
op-amp are loading, not additional poles). Solved as a loaded two-stage
RC ladder (op-amp input treated as AC virtual ground; the 4.7uF series
cap is low-impedance at these frequencies regardless): `[RE, math-verified]`

| Frequency | Attenuation (relative to the P34 drive) |
|---:|---:|
| 150 Hz (mid-CTCSS) | approx. -9 dB |
| 1200 Hz | approx. -12.5 dB |
| 2200 Hz | approx. -16.5 dB |

This is a gentle low-pass with its effective corner around 1 kHz, not a
tight sub-audio-only filter -- both Bell 202 AFSK tones (1200/2200 Hz)
pass through it, about 4 dB apart. That imbalance is small enough to
correct by scaling one tone's amplitude in software (see 13.3.2's DDS
table, which already gives each tone its own entry). Nothing here rules
out a software AFSK packet transmitter on this path; the open question is
the DDS tick rate (13.3.2), not this filter.

### 13.3.2 P34 is already a software tone synthesizer, not a fixed tone chip

Stock firmware generates CTCSS/DCS on P34 with a phase-accumulator DDS,
not a one-shot square wave: a per-tone frequency word is added into a
running phase value every tick (an interrupt-driven add-with-carry), and
the resulting phase indexes a smoothly-varying lookup table whose value
is written straight to `PWM0` every tick. This means the hardware path
is already a general-purpose, per-tone-programmable waveform generator,
not fixed-function CTCSS/DCS logic -- structurally the same idea as the
TK-x80's DAC-based tone synthesizer, just through PWM instead of a
parallel DAC. `[FW][RE]`

**The tick rate: resolved -- correcting an earlier wrong guess in this
section.** An earlier pass here read `sub_8B36`'s `ADM`/`ADIC` writes as
proof the tick was chained off the A/D converter, and estimated ~30
us/33 kHz from the A/D's conversion-time spec on that basis. That was
wrong: the interrupt *vector* `sub_8B36` is actually installed on (0x800E,
confirmed against the real hardware vector layout) is **TMF0 -- on-chip
8-bit Timer 0 -- not ADF**. The real ADF vector (0x8028) is a different
routine entirely, `sub_8B15`, which is the *receive*-side `AN0`/`LSDIN`
CTCSS/DCS discriminator (13.4) -- it reads `ADCR` and compares it to
mid-scale, nothing to do with TX tone generation. The `ADM`/`ADIC` writes
in `sub_8B36` are the receive side of the same handler: on every `TMF0`
while **not** transmitting (`FE78.7` clear) it starts one A/D conversion
on `AN0`, so timer 0 is also the receive sample clock (13.4). While
transmitting it runs the DDS instead. *Correction (2026-11-01):* this
paragraph used to say the `ADM` write happened only once, on the
handler's first entry. `[FW][RE]`

The actual driver, traced through `TMC0`/`MD0` (sec. 4.3.4 of the
manual): `TMC0 = 0x80` selects interval-timer mode, free-running (no
external trigger), `TM0` clocked at `f_CLK/6` = **1 MHz** at this radio's
6 MHz CPU clock, counting up to the 16-bit reload value in `MD0` before
`TMF0` fires and `TM0` resets. The arming call is `sub_B05E`, the single
shared entry point both the QT/DQT frequency-word path and the LTR
tone-table path converge on before falling into `loc_B0CC`: `MD0 =
0x005C` (92 decimal), `TM0 = 0`. **93 us per tick (`MD0` + 1, see 13.4)
= about 10.75 kHz, for all three modes alike while transmitting.**
Receiving uses other `MD0` values per mode (13.4). `[FW][RE]`

That's about 4.9x oversampling on the higher (2200 Hz) Bell 202 tone --
noticeably less generous than the earlier wrong estimate, but still a
real, comfortable margin, and it lands right in the range actual
hardware TNCs use for AFSK generation (9600-19200 Hz is typical), which
is a better sanity check than the suspiciously round 33 kHz this section
used to claim. Combined with 13.3.1 (the coupling filter passes both
tones, ~4 dB apart) and 13.3.3 (the output level is a software-read
constant, not a hardware ceiling), the conclusion is unchanged: no
remaining hardware or firmware obstacle this trace has found to
synthesizing 1200 bps Bell 202 AFSK for transmit through the
`P34`/`QT`/`DQT` path. `[RE]`

### 13.3.3 Output level is a per-mode EEPROM calibration byte, multiplied in every tick

The DDS output amplitude is not fixed. Which calibration byte applies
depends on a mode flag (`FE8F.5`): `[FW][RE]`

```text
amplitude_byte = FE8F.5 ? FE66 : FE64
PWM0           = amplitude_byte * phase_table[...] + table[amplitude_byte]
```

every tick. `FE64` and `FE66` are loaded straight from EEPROM bytes `F9`
and `FA` -- exactly the bytes the KPG calibration readback (9.4.3) labels
**"QT fine deviation"** and **"DQT fine deviation."** A third, sibling
routine for the 300 bps LTR data path does the identical multiply using
`FE68`, loaded from EEPROM byte `FB` ("LTR fine deviation").

So each of the three low-speed signaling modes already has its own
independent, per-radio, software-read gain byte multiplied straight into
the PWM/DDS output -- this is Kenwood's own deviation-alignment knob for
these modes, not a fixed level baked into the hardware. Stock sets it low
because CTCSS/DCS/LTR only need a few hundred Hz of deviation, nowhere
near full-scale. Replacement firmware reusing this path for something
that wants more deviation (e.g. Bell 202 AFSK) does not need to touch the
radio's actual EEPROM calibration -- it only needs to not read `F9`/`FA`
and substitute its own constant. The multiply is 8x8->16-bit, so there is
substantial headroom between the stock CTCSS-sized value and whatever
the coupling filter (13.3.1) and the deviation limiter can take before
clipping; the actual stock `F9` value (and therefore how much headroom is
left) hasn't been read back from a real radio yet. `[OPEN]`

## 13.4 Low-speed receive signaling (CTCSS, DCS, LTR decode)

```text
AN0 = LSDIN (pin 27)
```

"LSD" is Kenwood's **low-speed data**: everything sub-audible, i.e.
CTCSS (Kenwood "QT"), DCS ("DQT") and LTR's 300 bps data. The radio has no
tone decoder chip: the main CPU samples the sub-audible band on `AN0` and
decodes all three in software. `[SM][FW]`

### 13.4.1 Hardware path (from the schematic, Dave, 2026-11-01)

```text
IF/FM detector MC2272SV -> MC22173 "DET AMP" -+-> NJM4558 1/2, low-pass  -> LSD IN -> pin 27 (AN0)
                                              +-> NJM4558 2/2, high-pass -> Q202 "clip" -> SQ -> pin 30 (P30/CI0)
```

The same detector audio feeds both: the low-pass keeps only the
sub-audible band for `AN0`, and the high-pass keeps only the noise above
the voice band, which Q202 squares up into pulses for the squelch counter
(14.1). `[SCH]`

### 13.4.2 Sampling

Timer 0 is the sample clock. Its interrupt (`TMF0`, `sub_8B36`) starts one
A/D conversion on `AN0` (`ADM = 81h`) on every tick while receiving. The
A/D-done interrupt (`ADF`, `sub_8B15`) reads `ADCR`, takes 7Fh off as the
mid-scale bias, and leaves the sample as a magnitude (`FE36h`) and a sign
(`FE2Fh`: FFh at or above mid-scale, 00h below). The filter runs from the
main loop (`sub_8381`), once per new sample (`FE74.5`). `AN0` is read
nowhere else. `[FW]`

Timer 0 counts 1 µs; `MD0` sets the period, which is `MD0` + 1 µs (that is
what makes the DCS rate come out at exactly 8 samples per bit). The decoder
set-up (`sub_897A`) picks the rate by mode: `[FW][RE]`

| Mode | `MD0` | Period | Sample rate | Note |
|---|---|---:|---:|---|
| CTCSS | `0257h` | 600 µs | 1667 Hz | Nyquist 833 Hz, well above 254.1 Hz |
| DCS | `03A1h` | 930 µs | 1075.3 Hz | 8 samples per 134.4 bps bit |
| LTR | `01A3h` | 420 µs | 2381 Hz | about 8 samples per 300 bps bit |
| Transmit (DDS) | `005Ch` | 93 µs | 10.75 kHz | 13.3.2 |

The RX tone word is the channel's CTCSS/DCS word from the codeplug, held
in `FE8Ch`-`FE8Dh` (`0xC000` + tone x 10 for CTCSS, `0xE000` + code for DCS;
see the codeplug format). `[FW]`

### 13.4.3 The CTCSS decoder

A textbook narrow-band tone detector, in 16-bit sign-magnitude
fixed point: `[FW][RE]`

1. **Band-pass:** a second-order resonator tuned to the programmed tone,
   `y[n] = b0·x[n] + a1·y[n-1] − a2·y[n-2]` (`FE38h` b0, `FE3Eh` a1, `FE40h`
   a2; state `FE3Ah`-`FE46h`). The input is scaled down by 8 first.
2. **Coefficients:** computed per tone (`sub_88CE`) from the tone frequency
   (word & 0FFFh = tone x 10) by linear interpolation in two 64-entry
   tables in the mask ROM (`waveform_tables`), so any tone in the range
   works, not just a fixed list.
3. **Envelope:** a second stage (`FE48h`-`FE52h`) turns the resonator output
   into a level, clamped at 014Ah and halved.
4. **Smoothing:** another second-order low-pass (`FE86h`, `FE60h`, `FE90h`)
   gives a slow level in `FE8Ah`.
5. **Decision with hysteresis:** tone detected (`FE75.1`) when the level
   reaches 35BCh, released below 23D3h; a second, lower threshold (11E9h)
   sets `FE75.0`. Detection is only accepted while `FE74.3` is set
   (meaning not yet traced; probably carrier/squelch open).

Note: `FE40h` is the resonator's a2 while receiving and the TOT count while
transmitting; the two never run at the same time (`sub_8381` returns at
once while `FE78.7`, transmitting, is set). See the TOT Off patch in the
firmware repo. `[FW]`

### 13.4.4 DCS and LTR

- **DCS:** the same resonator front end, retuned (fixed coefficients),
  slices one bit per 8 samples into a shift register (`FE60h` on) and
  compares it with the expected code word, its rotations and inverse
  (`FE94h`, `FEA4h`, `FEB4h`, with a bit-error count, `sub_88A6`). The
  expected word is built from the 9-bit code with the standard DCS
  Golay (23,12) parity, generator `C75h` (`sub_8AC6`). `[FW][RE]`
- **LTR:** again the same front end with its own coefficients; bits go into
  a 56-bit shift register (`FE88h`-`FE8Eh`) that is matched against the sync
  pattern by XOR and a bit-count table: 6 or fewer errors arms it, 10 or more
  rejects. `[FW][RE]`

### 13.4.5 For replacement firmware

Nothing here needs special hardware handling: a timer interrupt at the
sample rate, one `AN0` conversion per tick, and a tone detector in the
main loop. Our own code does not have to copy Kenwood's filter; a
Goertzel detector per tone at 1667 Hz, or the same resonator-envelope
design, both fit the CPU easily. Open items: `[OPEN]`

- the A/D conversion time and whether one conversion per tick at 1667 Hz
  leaves the 3 ms tick and the panel link undisturbed (stock shows it does);
- the meaning of `FE74.3` (the gate on detection);
- the `AN0` signal level for a standard-deviation tone (scope or probe), to
  set thresholds.

---

# 14. Receiver squelch/noise detector

## 14.1 P30 is a pulse input, not a binary carrier line

```text
P30 / CI0 = SQ/noise pulse input
```

The receiver noise detector converts discriminator/high-frequency noise into a
pulse train. With no usable signal, noise produces pulses. A good carrier
quiets the receiver noise and the pulse train collapses; the schematic/service
notes describe "signal off" as pulses and "signal on" as a steady low state.
`[SM][SCH][RE]`

`P30/CI0` is a **noise-pulse source**, not a binary carrier line. Do not
model squelch as a bit read from `P3`: neither ROM ever reads `P3.0`. The
pulses are counted in hardware and the count is read periodically.

`P30/CI0` is **pin 30** on the 64-pin QFP, fed from the high-pass and Q202
clipper (13.4.1). `[SCH]` (Dave, 2026-11-01)

## 14.2 How stock squelch works

Traced in both stock images; the code is byte for byte the same in TK-941
AB00 and TK-840 8355, at the same addresses. `[FW][MASK]`

**The counter.** The mask ROM's reset code sets `UDCC0 = 88h` and
`CUIM = 01h` (`00B5h`, `00BEh`), after which the up/down counter `UDC0`
(`FF1Ch`) counts the noise pulses on `CI0` with no software help. Its
interrupts (`CRF00`/`CRF01`, vectors `001Ah`/`001Ch`, and `801Ah`/`801Ch`
in the application) go to a bare `RETI` in both ROMs; nothing is
interrupt driven. The application just reads `UDC0` and writes it back to 0.
`[MASK][FW]`; the exact meaning of the two control values is `[DS]`
unchecked.

**The sample tick.** Timer 1's interrupt (`isr_tmf1`, `8AFAh`) sets
`FED5h.5` every **3 ms** (`MD1 = 0BB8h`), but only when timer 1 isn't busy
with a mic-port serial bit: the stock tuning session's soft serial uses the
same timer, so **no squelch samples are taken while a mic-port byte is
being sent or received**. `[FW]`

**`squelch_update`** (`8238h`, CALLT slot `08h`). The main loops call it
often (five times per pass of the loop at `9E01h`, and from `A9A5h` on);
it does nothing unless the 3 ms flag is set, so it runs at most once per
tick, and not at all while transmitting (`FE78h.7`, set on the transmit
paths, cleared before `tune_rx_word`). Each tick: `[FW]`

1. `pulses = UDC0`, then `UDC0 = 0`.
2. `quiet = 47 - min(pulses, 47)` (`SQ_PULSE_MAX = 2Fh`): 0 = all noise,
   47 = no noise pulses at all in 3 ms.
3. One-pole low-pass filter in `FE30h`:
   `filter = filter × DF5Ah / 10000h + quiet` (×0.873 per tick, a time
   constant of about 22 ms). `[RE]` this reads `MULUW` as leaving the high
   word in `AX`, which is the only reading that gives a working filter.
4. `level = filter / 2`. With a steady input `level` settles at about
   `3.94 × quiet`, so 0 to about 185.
5. Compare with the threshold `sq_threshold` (`FE5Eh`, copied to `FE34h`):
   - **open** when `level ≥ sq_threshold`;
   - **close** when `level + 45 < sq_threshold` (`SQ_HYSTERESIS = 2Dh`);
   - anything between leaves the state alone.

   The state is `FE74h.3` (set = open, "busy"). The stock code reads that
   bit in about ten places to mute audio, light BUSY and so on.

**The threshold.** `sq_threshold` comes from EEPROM `F8h` (the squelch
alignment byte, section 9.4.1; the read goes through `FEF0h`, which is
register `X` of bank 0, into the low byte of `FE5Eh`). Setting it to 0 makes `level ≥ 0` always
true, which is how the monitor / squelch-off function opens it: the code
at `9F89h`/`9FAFh` toggles between the EEPROM value and 0, with panel
bytes `9Dh`/`9Eh` for the indicator. `B739h`/`B748h` step it down and up by
one (tuning mode). `[FW]`

**After a channel change.** `pll_lock_wait` and the receive tune
(`9BABh` in AB00, `9C0Dh` in 8355, and `DA1Fh`) wait 10 ms for lock, one
more 3 ms tick, then zero `UDC0`, so the first count after a tune is clean.
`squelch_reset` (`82C7h`) is called on channel changes: it preloads
`filter` from the threshold so a decision comes sooner, forces the state
closed, and zeroes `UDC0`. `FE74h.2`, set at `DA3Eh`, makes opening
easier until the first open: while it is set, the open test is
`level ≥ 51` (for thresholds below `49h`) or `level ≥ sq_threshold - 22`.
`[FW]`; the preload arithmetic is not worked through. `[OPEN]`

**On open and close** `squelch_update` also: `[FW]`

- drives the accessory `SQ` line: `outexp_shadow.7` (4094 `Q7`) cleared on
  open, set on close (skipped while `FE79h.1` is set);
- if `FEDCh.2` is set (the same bit that copies panel bytes to the
  hardware UART, section 12.4), sends `70h` (open) or `71h` (close) out of
  the hardware UART;
- on close, if `FEC3h.7` is set and `FE7Fh.2` clear, calls `C7ADh` first.
  `[OPEN]` what that does (squelch tail or a signalling timer are
  candidates).

**For replacement firmware** this is easy to copy: read and clear `UDC0`
every 3 ms, filter, compare with hysteresis. A scan step costs about 13 ms
of lock and settle plus the filter's time to decide; how long a real
signal takes to open (and noise to stay shut) should be measured on the
bench before designing around it. `[RE]`

## 14.3 Accessory SQ is different

The `SQ` output presented externally is generated through the 4094 `Q7` path
(`outexp_shadow.7`, driven by `squelch_update`, section 14.2).
It is the radio's computed logic-squelch indication, not the raw P30 pulse
source. Buffer/transistor circuitry can invert the level between FE28 bit 7 and
the accessory connector. `[SM][SCH][FW]`

---

# 15. Model/RF-split analog inputs

## 15.1 Analog inputs

| ADC input | Current role | Evidence |
|---|---|---|
| `AN0` (pin 27) | low-speed signaling input (`LSDIN`, 13.4) | `[SM][SCH]` |
| `AN1` | electrically part of/near the AN2 strap network; not used by decoded model routine | `[SCH][MASK][OPEN]` |
| `AN2` | model/RF-split strap | `[SM][MASK]` |
| `AN3` | family strap; GND on TK-840 | `[SM][MASK]` |

## 15.2 Resident model detection

Mask entry:

```text
1F15 = hardware/model detect
```
It converts AN3/AN2 into resident flag bits:

```text
FED3.4
FEDA.0
FEDF.4
```

`[MASK][RE]`

### TK-840 path: AN3 low

```text
AN2 < 0x55          -> M890B3
0x55 <= AN2 < 0xAA -> M890B2
AN2 >= 0xAA         -> M890B1
```

### TK-940/TK-941 path: AN3 high

```text
AN2 < 0x7F  -> M941B1
AN2 >= 0x7F -> M940B1
```

The unused TK-840-family flag combination is never produced by the decoded
threshold tree and should be treated as unsupported hardware rather than a
sixth known radio.

Mask entry `1F4B` converts the flags into the six-character PROGRAM/CLONE model
field.

**Replacement-firmware rule:** use `1F15` early in startup instead of duplicating
the ADC thresholds unless there is a compelling reason not to.

---

Part of the [TK-x4x hardware reference](README.md) · previous: [Front panel CPU, display, buttons and beeps](Front_Panel.md) · next: [Resident mask ROM services and interrupts](Resident_ROM_and_Interrupts.md)
