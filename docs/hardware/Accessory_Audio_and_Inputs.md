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
00009e  mov  sfr[0x52], #0x04     ; SCC
0000a1  mov  sfr[0x53], #0x4e     ; BRG = 78 decimal
0000a4  sel  rb0
0000a6  mov  stbc, #0x00          ; the same single STBC write documented in 1.5
0000aa  mov  sfr[0x40], #0x07     ; MM (memory expansion mode)
        ... (RFM, UDCC0/1, CRC, CUIM, CR11, PWM0/1, CPTM, FRCC, RTPC, MD1,
             TMC0/1, ADM, TBM -- unrelated peripheral baseline in between)
0000e9  mov  sfr[0x50], #0xcd     ; SCM, still inside the same unbroken
                                   ; reset routine (ends in a bare `ret` at
                                   ; 0000ecH)
```

The mask ROM also contains its own low-level byte-primitive instructions
for this UART (`mov sfr[0x57], a` at mask-ROM address `0x558`, `mov a,
sfr[0x56]` at `0x579`). `[MASK]`

**The external stock application independently confirms the same UART is a
live, working serial port with a real protocol on it**, not merely
initialized and left idle. Two clean polled byte-primitive routines exist at
fixed addresses, both correctly servicing the watchdog while they wait
(compare against the kick idiom in section 19.3): `[FW]`

```asm
; 0x8178: send_byte(A) -- wait for STIC.IF (transmit done), then send
008178  clr1  sfr[0xde].7      ; clear STIC.IF
00817b  mov   sfr[0x57], a     ; TXB = A
00817d  mov   wdm, #0x00
008181  mov   wdm, #0x86       ; service watchdog while polling
008185  bf    sfr[0xde].7, 0x817d
008189  ret

; 0x818a: recv_byte() -> A -- wait for SRIC.IF (receive full), then read
00818a  mov   wdm, #0x00
00818e  mov   wdm, #0x86       ; service watchdog while polling
008192  bf    sfr[0xdc].7, 0x818a
008196  clr1  sfr[0xdc].7      ; clear SRIC.IF
008199  mov   a, sfr[0x56]     ; A = RXB
00819b  ret
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
00b1d3  bt   sfr[0xdc].7, 0xb1d9
00b1d9  clr1 sfr[0xdc].7
00b1dc  mov  a, sfr[0x56]        ; A = received command byte
00b1e1  cmp  a, #0x43            ; 'C'
00b1e3  bne  0xb202
00b1e5  call 0xb333              ; unresolved validation/checksum helper
00b1e8  bne  0xb200
00b1ea  set1 0xfedc.2
00b1ec  mov  a, #0x06            ; ACK
00b1ee  callt [0x0017]           ; send_byte(0x06)
        ... (RAM/state setup, then:)
00b1fd  mov  sfr[0x52], a        ; SCC rewritten -- mode/baud changes here
00b1ff  ret

00b202  cmp  a, #0x4f            ; 'O'
        ... (structurally identical: validate, ACK, then rewrite SCC) ...

00b22c  clr1 0xfedc.2
00b22e  mov  a, #0x15            ; NAK
00b230  callt [0x0017]           ; send_byte(0x15) for any unrecognized byte
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
protocol. The exact post-ACK `SCC` value, the `0xb333` validation helper, and
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

**Open: the tick rate.** The periodic interrupt driving this is chained
off the A/D converter's conversion-complete interrupt rather than a named
timer, and once tone generation is active nothing in that code path
rewrites the A/D mode register again -- consistent with the A/D being
left to free-run continuously, purely as a convenient periodic-interrupt
source, at whatever rate its hardware conversion time gives at this
radio's 6 MHz CPU clock. That rate is the deciding number for whether
1200 bps Bell 202 AFSK (1200/2200 Hz tones) is reachable: it needs to be
comfortably above a few kHz for clean tone synthesis at those
frequencies. Not yet established -- needs either the µPD78312A's A/D
conversion timing (datasheet) or a bench measurement of the actual tick
rate (e.g. scope `P34`/the `QT`/`DQT` net while a CTCSS tone is active).
`[OPEN]`

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

## 13.4 Low-speed receive signaling

```text
AN0 = LSDIN
```

AN0 is the receive low-speed signaling input used for CTCSS/DCS-type decode.
`[SM]`

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

So the low-level firmware abstraction should be named something like:

```text
squelch_noise_pulses
```

rather than simply `carrier_detect`.

The CPU's `CI0` peripheral is well suited to counting/timing those transitions.
The exact stock threshold/window algorithm is still `[OPEN]` and should be
recovered from the application firmware or measured before copying Kenwood's
behavior.

The original `HARDWARE_COMMUNICATION.md` contained a deliberately marked
placeholder that read an assumed `SFR8(0xFF04)` bit as squelch. That sketch is
obsolete and should not be reused. The important correction is that `P30/CI0`
is a **noise-pulse source**, not a proven binary carrier GPIO.

**Searched and not found (recorded so this isn't re-searched from scratch):**
a direct search of both the external firmware and mask ROM for any bit-test
of `0xFF03.0` (P30), any whole-byte read of `0xFF03`, and any reference at
all to the capture registers `CPT0`/`CPT1` (`0xFF10-13`) or the free-running
counter `FRCC` (`0xFF60`) turned up **zero hits** in either ROM. `1F12`'s
baseline (section 19.2) does configure a different-looking timer/capture
block (`0xFF0E`, `0xFF14`, `0xFF16`, plus byte SFRs `sfr[0x60]`/`sfr[0x64]`/
`sfr[0x66]`/`sfr[0x68]`) at startup, but nothing was found reading those
registers back and comparing against a threshold, so no connection to
squelch could be established. `[FW][MASK]`

**The mask ROM's vectors were traced, and none of them is squelch.** `[MASK]`

- `CRF00` and `CRF01`, the interrupts of the counter on `CI0`/`P30`, have no
  handler: their vectors (`001AH`, `001CH`) point at `08A4h`, a bare `RETI`
  shared with every unused source (`default_int_handler`). So nothing in
  the mask ROM counts the noise pulses.
- The targets reached through the other vectors belong to the serial ports:
  `0FA4h` is the mic-port transmit bit handler (timer 1), `0FDAh` the
  mic-port receive start bit (`EXIF1`), `17E5h` the firmware loader's
  receive start bit ([Mic Port Serial](../programming/Mic_Port_Serial.md),
  section 3). `EXIF2`'s handler (`1513h`) picks a timer-0 period by
  `FED7.5`, unmasks `TMIC0`, masks `EXIC2`, clears a history byte (`FEC7h`)
  and sets a count of 9 (`FEC9h`). `TMF0`'s handler (`08A5h`) branches by
  `FEDE.1`/`.2` to `1006h`, `153Fh` or `17C2h`; `153Fh` samples `P23` (the
  clone/programming data input) into that history byte. `1006h` and `17C2h`
  were not followed further, but they are reached through `TMF0`, not
  `CI0`.

So the squelch algorithm is not in the mask ROM. It has to be in the
application image, which may route `CI0`'s interrupts to its own handlers
(`TPF` set; section 2.3). Tracing the stock application (AB00h) for
`CRF00`/`CRF01` vectors and noise-pulse counting is the next step. `[MASK][RE]`

## 14.2 Accessory SQ is different

The `SQ` output presented externally is generated through the 4094 `Q7` path.
It is the radio's computed logic-squelch indication, not the raw P30 pulse
source. Buffer/transistor circuitry can invert the level between FE28 bit 7 and
the accessory connector. `[SM][SCH][RE]`

---

# 15. Model/RF-split analog inputs

## 15.1 Analog inputs

| ADC input | Current role | Evidence |
|---|---|---|
| `AN0` | low-speed signaling input (`LSDIN`) | `[SM]` |
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
