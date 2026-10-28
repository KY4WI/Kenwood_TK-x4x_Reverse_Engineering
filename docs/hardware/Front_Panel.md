<!-- Front panel CPU, display, buttons and beeps -->
Part of the [TK-x4x hardware reference](README.md) · previous: [External flash programming](Flash_Programming.md) · next: [Accessory connector, PTT and audio, squelch, model inputs](Accessory_Audio_and_Inputs.md)

# 11. Front panel CPU, display, buttons, and beeps

The link's protocol, timing, interrupt handlers, the stock application's
use of them, and a checklist for replacement firmware are now in
[Front Panel Link](Front_Panel_Link.md), which supersedes the transport
details below where they differ.

## 11.1 Architecture

The main CPU does **not** scan the front-panel switches and does **not** drive
the LCD glass directly. The front panel contains a separate NEC
µPD75308BGK-740 MCU. `[SM]`

The panel MCU:

- scans the eight front-panel function switches;
- handles volume up/down;
- directly drives the LCD segments and annunciators;
- generates local alert/beep tones;
- exchanges serial bytes with the main µPD78312.

This is favorable for replacement firmware: the new application can treat the
front panel as a serial peripheral rather than reproducing LCD multiplexing or
physical key scanning.

## 11.2 Physical panel key inputs

The service information maps the eight front-panel switches directly to panel
CPU inputs:

| Panel CPU input | Physical front-panel key |
|---|---|
| `P60` | AUX |
| `P61` | SCAN |
| `P62` | Volume Up |
| `P63` | Volume Down |
| `P70` | Group Up |
| `P71` | Group Down |
| `P72` | System Up |
| `P73` | System Down |

`[SM]`

The commercial names "Group" and "System" are software policy. Replacement
firmware should model these as physical keys and may bind them to VFO,
memory/channel, scan, monitor, reverse, power, or other ham-radio functions.

The POWER switch is not one of these eight software-scanned panel inputs.

## 11.3 LCD hardware owned by the panel CPU

The µPD75308 directly drives the LCD through its segment/common outputs:

```text
S0-S31
COM0-COM3
VLC0-VLC2 / LCD-bias circuitry
```

The TK-840 panel has eight alphanumeric **14-segment** character positions. The
physical annunciators observed on the glass are:

```text
TX
BUSY
CALL
S in an oval
TA
OPT
AUX
telephone symbol
```

The command bytes for every one of these icons are in section 11.10. Other
x4x panel variants may differ. `[SM][BENCH]`

Replacement main firmware sends panel commands/character data. It does not need
the glass segment map for ordinary text operation.

The panel MCU necessarily contains the physical segment map and character
generator because it directly drives `S0-S31`/`COM0-COM3`. However, no raw
14-segment bitmap-write interface has been identified in the analyzed main-CPU
firmware or mask-ROM display helpers. The known paths send character codes and
control bytes instead. `[FW][MASK][RE]`

**Practical rule:** treat the µPD75308 as a black-box smart display/key
controller. Dumping its mask ROM is not required for normal replacement
firmware unless a future goal requires arbitrary custom glyphs/raw segment
control or an otherwise unreachable panel function.

## 11.4 Main-to-panel link pins

```text
Main P04       -> panel serial input
Main P21/INTE0 <- panel serial output

Panel P10      <- main CPU
Panel P33      -> main CPU
```

`[SM][MASK]`

Transport characteristics already established:

- LSB-first data shifting;
- `0xFF` is treated as idle/no-data on receive;
- one transfer shifts an outgoing byte while simultaneously sampling the
  return-data line;
- completion is interrupt/timer driven in the resident mask ROM.

## 11.5 Resident panel transport services

```text
1F2A   recover/kick receive-start state
1F2D   queue/start one outgoing byte, A = byte
1F30   panel INTE0/start ISR, RETI-only
1F33   timed panel-link ISR/state machine, RETI-only
```

`[MASK][RE]`

Resident state:

```text
FE25      current TX shift byte
FE26      current RX shift byte
FE27      bit/time counter

FE2C      primary queued received byte
FEBC      backup received byte 1
FEBD      backup received byte 2
FEBE      backup received byte 3

FEDA.1    transmit/busy
FEDA.2    primary receive byte pending
FED9.7    frame/link active

FEDD.0
FEDD.1
FEDD.2    backup receive-queue occupancy/state
```

The resident `1F2D` entry waits for the previous transfer to become idle, stores
the new byte in `FE25`, sets the busy state, initiates the transfer on `P04`,
and returns while the interrupt machinery shifts the byte. It does not return a
synchronous received byte.

### Important failure behavior

`1F2D` has no timeout while waiting for the previous busy state to clear. If
the panel state machine is broken or its required interrupts are not running, a
later panel send can block forever while servicing the watchdog.

For the first replacement image, preserve the original panel interrupt/vector
arrangement before relying on `1F2D`.

## 11.6 Stock application panel-send path

The external application independently contains a byte-send wrapper at
`0x8F22` (in the TK-840 image, byte sum `8355h`, that this section was
written from; the TK-941 `AB00h` image has two, at `8F07h` and `A4E1h`).
Its core behavior is the same resident state contract:

```text
wait while FEDA.1 is set
FE25 = A
set FEDA.1
drive P04 low to start transfer
return
```

`[FW][RE]`

The first external CALLT entry (`table 0x0040`, `CALLT [0]`) targets `0x8F22`.
A second application helper at `0xA43F/0xA458` performs the same basic transmit
operation with additional application background servicing.

This is useful independent confirmation that `FE25`, `FEDA.1`, and `P04` form
the intended application-visible transmit state.

Replacement firmware does not need to copy the stock helper. A small verified
assembly shim around resident `1F2D` is cleaner.

Conceptual interface:

```c
void panel_send_byte(u8 value);   /* assembly shim -> CALL 1F2D */
```

## 11.7 Reading panel events: the receive queue is now understood

The old hardware-communication guide treated `FE2C` as only a rough sketch and
did not know how to consume the pending/backup slots. The stock application
shows the real pattern.

Normal panel-event code tests `FEDA.2`, clears it, and reads `FE2C`. The
application also contains a queue-pop helper around `0xB1B0` that advances all
backup slots:

```text
return_value = FE2C

FEDD.0 -> FEDA.2
FEDD.1 -> FEDD.0
FEDD.2 -> FEDD.1
clear FEDD.2

FEBC -> FE2C
FEBD -> FEBC
FEBE -> FEBD
0xFF -> FEBE
```

`[FW][RE]`

That means a replacement driver can implement a real `panel_get_event()`
instead of simply reading the "last byte."

Conceptual API:

```c
u8 panel_event_available(void);
u8 panel_get_event(void);
```

The exact C/assembly implementation should preserve interrupt atomicity while
moving the queue flags/bytes.

## 11.8 Known key/event values

The front-panel keys are fully mapped (table below): one byte when a key
is pressed, the next byte up when it is released, and nothing in between.

Mask-ROM power-on key/mode decoding identifies:

```text
0x8C   Group Up
0x8D   Group Down
0x84   System Up
0x9E   AUX
```

These power-on mappings have also been exercised in the x4x family work.

The stock normal-operation dispatcher at `0x9E4C` consumes `FE2C` and explicitly
recognizes at least these event values:

```text
80 81 82 83
88 89 8A 8B
90 91 92 93
98 9C
A0 A2
```

`[FW][RE]`

Dave's bench session (2026-10-08) names them. Each key sends one byte when
pressed and the next byte up when released: `[BENCH]`

| Key | Pressed | Released | Name (pressed; released adds `_REL`) |
|---|---|---|---|
| System Up | `0x80` | `0x81` | `PANEL_KEY_SYS_UP` |
| System Down | `0x82` | `0x83` | `PANEL_KEY_SYS_DN` |
| Group Up | `0x88` | `0x89` | `PANEL_KEY_GRP_UP` |
| Group Down | `0x8A` | `0x8B` | `PANEL_KEY_GRP_DN` |
| Volume Up | `0x90` | `0x91` | `PANEL_KEY_VOL_UP` |
| Volume Down | `0x92` | `0x93` | `PANEL_KEY_VOL_DN` |
| Scan | `0x98` | `0x99` | `PANEL_KEY_SCAN` |
| AUX | `0x9C` | `0x9D` | `PANEL_KEY_AUX` |

The names here and in the tables below are defined in
[`include/panel_codes.h`](../../include/panel_codes.h) (C) and
`include/panel_codes.inc` (assembly); code should use them rather than
the hex.

The key bytes decode as `1kkkkkke`: bit 7 always set, bit 0 the edge
(0 pressed, 1 released), and bits 6-1 a key number, `(byte & 7Eh) >> 1`:
System Up 0, System Down 1, Group Up 4, Group Down 5, Volume Up 8, Volume
Down 9, Scan 12, AUX 14. The three rockers each take the first two numbers
of a group of four; Scan and AUX share a fourth group, leaving numbers 13
(`9Ah`/`9Bh`) and 15 (`9Eh`/`9Fh`) unused by these keys. `[RE]` By the
same decode `0xA0` and `0xA2` are numbers 16 and 17, not a press/release
pair, which fits their being session requests rather than keys.

So the dispatcher's list above is every key's press and release, except
that it uses only the press for Scan and AUX. The power-on values
(`0x84`, `0x8C`, `0x8D`, `0x9E`) are different bytes from these;
they are what the mask ROM reads at power-on, not normal key events.
`0xA0` and `0xA2` are not keys: they ask for the programming and tuning
sessions (see `docs/programming/Programming_Protocol.md`). **The panel sends no
repeat codes**: holding any key down, however long, sends only the press
byte and, on release, the release byte. Any auto-repeat has to be timed by
the main CPU. `[BENCH][FW]` (2026-10-16)

`0xA0` is especially notable because the normal dispatcher can branch into the
resident PROGRAM monitor (`1F00`) when that event is accepted.

## 11.9 Writing ordinary display text

This is no longer an unknown transport problem.

The mask ROM contains resident code that sends:

```text
0x02
<eight bytes from a buffer>
```

and another path that sends `0x02` followed by four bytes. The stock external
application independently emits `0x02` followed by character bytes for normal
display construction. `[MASK][FW][RE]`

This strongly identifies `0x02` as a character/display-data command or prefix.
The TK-941 `AB00h` image also sends `0x02` followed by only 4 characters, so
`0x02` starts text rather than meaning exactly eight. Beware that a byte's
meaning depends on the byte before it: after `0xB3`, `0x02` is a beep
number, not text ([Front Panel Link](Front_Panel_Link.md), section 7). `[FW]`
For an eight-character main field, the practical first replacement-firmware
operation is:

```text
panel_send_byte(0x02)
panel_send_byte(char0)
panel_send_byte(char1)
...
panel_send_byte(char7)
```

Space-pad unused positions.

Conceptual helper:

```c
void panel_write_text8(const char *s)
{
    unsigned i;

    panel_send_byte(0x02);

    for (i = 0; i < 8; ++i) {
        u8 ch = (s && s[i]) ? (u8)s[i] : (u8)' ';
        panel_send_byte(ch);
    }
}
```

The exact high-level semantic name of command `0x02` should remain
`[RE]` until bench capture confirms whether it always means "write all eight
main characters" or has context-dependent cursor/field behavior. The byte
sequence itself is well supported enough for bring-up testing.

## 11.10 Observed display/control and annunciator bytes

Resident CLONE mode sends:

```text
0x8E
" CLONE  "
```

and uses resident strings including:

```text
" ERROR  "
" END    "
```

`[MASK][RE]`

Tracing stock application callers now gives a substantially better picture of
the `0x9x`/`0xAx` control-byte range. These are **main-CPU -> panel** bytes.
Do not confuse them with same-valued **panel -> main** key/event bytes; the two
directions are independent and the same numeric value can legitimately mean
different things in each direction.

### Bench-confirmed panel commands (2026-10-08)

Dave's bench sessions with the `panelprobe` byte picker sent single bytes
`0x80`-`0xB3` to a real panel and recorded what each one did. `[BENCH]`

**Icons.** Every icon has an on / off / flash triplet, except TX, which has
no flash byte (`0x98` does nothing).

| Icon | On | Off | Flash | Names |
|---|---|---|---|---|
| TA | `0x90` | `0x91` | `0x92` | `PANEL_TA_ON` / `_OFF` / `_FLASH` |
| TEL (phone) | `0x93` | `0x94` | `0x95` | `PANEL_TEL_...` |
| TX | `0x96` | `0x97` | none | `PANEL_TX_ON` / `_OFF` |
| BUSY | `0x99` | `0x9A` | `0x9B` | `PANEL_BUSY_...` |
| CALL | `0x9C` | `0x9D` | `0x9E` | `PANEL_CALL_...` |
| "(S)" (S in a circle) | `0xA0` | `0xA1` | `0xA2` | `PANEL_SCIRCLE_...` |
| arrow (triangle) | `0xA3` | `0xA4` | `0xA5` | `PANEL_ARROW_...` |
| OPT | `0xA6` | `0xA7` | `0xA8` | `PANEL_OPT_...` |
| AUX | `0xA9` | `0xAA` | `0xAB` | `PANEL_AUX_...` |

**Whole display.**

| Byte | Name | Effect |
|---|---|---|
| `0x80` | `PANEL_ALL_ICONS_ON` | all icons on (lamp test) |
| `0x81` | `PANEL_ALL_ICONS_FLASH` | all icons flashing |
| `0x82` | `PANEL_FLASH_ALL` | all icons flash, and so does the text being shown (for example `"- SCAN -"` flashes too) |
| `0x83` | `PANEL_ALL_SEGMENTS` | as `0x82`, plus every text segment lit; the text still shows underneath |
| `0xB0` | `PANEL_CLEAR`, `PANEL_SYNC` | clears the screen. It is also the reset handshake byte (the panel answers `0xB1`, `PANEL_SYNC_ACK`) |

**Built-in strings.** One byte shows a whole 8-character string stored in
the panel itself, with no `0x02`:

| Byte | Name | Shows |
|---|---|---|
| `0x85` | `PANEL_SHOW_SCAN` | `"- SCAN -"` |
| `0x86` | `PANEL_SHOW_AUTOTEL` | `"AUTONTEL"` (read off the display; not the `"AUTO TEL"` text the stock firmware keeps in flash) |

`0x84` and `0x87` sit next to these and have not been tried.

**Other results.**

- `0x8B` + 2 bytes shows `"<n> - <m>"`, the system and group numbers.
  Sweeping the first byte showed `0` to `31` correctly (32 systems).
  `[BENCH]`
- `0x8E` turns the TX icon off, as a side effect. `[BENCH]`
- `0xB3` is a prefix: the stock firmware always follows it with a second
  byte, `0x01`-`0x0D` (section 11.11). Sent on its own it **locks up the
  panel**, and `0xB3 0x0E` locked it up too. `0x01`-`0x0D` after `0xB3`
  showed nothing on the display. `[BENCH][FW]`
- Under `0x82`, some icon commands change the flashing of a different
  icon: TA off stops "(S)" flashing, and "(S)" on/off changes whether TA
  flashes. Only the TA / "(S)" pair was tried. `[BENCH]` `[OPEN]` To map it:
  send `0x82`, then each of one icon's on/off/flash bytes in turn, noting
  the other icon's state after each; reset with `0x80` and `0x82` and swap
  the two icons; then repeat for neighbouring pairs (TEL/BUSY, CALL/AUX,
  arrow/OPT). One icon changed per step shows whether icons share state in
  the panel.

**Why stock PROGRAM screens flash.** The mask ROM's firmware loader shows
`"PROGRAM "` (or its other screens) with `0x02` + 8 characters and then
sends `0x82` (`0290h`, `02CDh`), which by the bench result above makes the
text flash. `[MASK][BENCH]`

**`0x84` and `0x87`** (the `benchtest` probe, 2026-10-16) did nothing that
could be seen. Neither the stock firmware (AB00h, B5ECh, 8355h) nor the mask
ROM ever sends either one. `[BENCH][FW][MASK]`

**After `0xB3 0x07` the panel sends `0xAA` back**, once each time (beep 07,
section 11.11). The stock firmware ignores a panel byte `0xAA`: its event
table sends `0xA0`-`0xAF` to a bare `RET` (`B6DBh` in AB00), and the other
dispatchers don't look for it. `[BENCH][FW]` `[OPEN]` what it means; perhaps
"beep finished".

**Character set** (`0x02` + text, `benchtest`, 2026-10-16). The display is
14-segment, upper case only:

| Codes | Shown |
|---|---|
| `0x20` | space |
| `0x23`-`0x25`, `0x27`-`0x2D`, `0x2F` | `# $ %` (`#` and `%` rough), `' ( ) * + , -`, `/` |
| `0x2E` | a comma, the same as `0x2C` |
| `0x30`-`0x39` | `0`-`9` |
| `0x3D` | `=` |
| `0x40`-`0x5A` | `@`, `A`-`Z` |
| `0x5C`, `0x5F`, `0x60` | `\`, `_`, `` ` `` |
| `0x7F` | every segment of the character lit |
| `0x21 0x22 0x26 0x3A 0x3B 0x3C 0x3E 0x3F 0x5B 0x5D 0x5E`, `0x61`-`0x7E` | nothing (blank); lower case is blank |

The stock firmware's channel-name filter (`A8C7h`, section 7 of the panel
link notes) blanks almost exactly the blank ones, with four differences: it
also blanks `.` (which the panel draws as a comma) and `` ` `` (which it
draws), and it passes `?` and lower case (which the panel leaves blank).
KPG refuses `.` in a name; the CHIRP driver turns it into `,`. `[BENCH][FW]`

Not yet tried: `0xB1`, `0xB2`, and the rest of `0x88`-`0x8F` apart from
`0x8B` and `0x8E`.

Still open:

- what panel byte `0xAA` (sent after beep 07) means;
- verify any display cursor/partial-update behavior;
- investigate raw segment access only if custom glyphs become desirable.

Basic event reception, eight-character text output, and TX-annunciator control
are understood well enough to build the first replacement-firmware UI.

## 11.11 Panel-generated beeps

Panel CPU outputs:

| Panel pin | Function | Approximate service-manual frequency |
|---|---|---:|
| `P20` | high beep | 1500 Hz |
| `P21` | medium beep | 900 Hz |
| `P22` | low beep | 700 Hz |

`[SM]`

These are local UI tones, not the RF CTCSS/DCS generator. The main application
should eventually expose them through symbolic UI feedback once their panel
command bytes are decoded.

The stock firmware's beeps are `0xB3` followed by a number `0x01`-`0x0D`;
every beep routine also clears expander outputs `AM` and `AC` to unmute the
audio (see `docs/hardware/Front_Panel_Link.md`, section 7). With the audio unmuted
(the `benchtest` probe, 2026-10-16) every number plays: `[BENCH]`

| Beep | Name | Sound | Stops by itself | Stock use (AB00) `[FW]` |
|---|---|---|---|---|
| `01` | `BEEP_BUSY_CONT` | fast, urgent, like a busy signal | no | 8 places |
| `02` | `BEEP_SHORT` | normal key-press beep | yes | 3 places |
| `03` | `BEEP_WARBLE_CONT` | two-tone warble, "something is wrong"; one tone as `01`/`05`, the other higher | no | 2 places, one through `beep_03_wait` |
| `04` | `BEEP_TONE_CONT` | as `02`, continuous | no | entering dealer mode, tuning mode and the tuning session (where the code stops it with `0xB4` about half a second later) |
| `05` | `BEEP_VOLUME` | short beep, the `01` tone | yes | volume changes (tuning commands `1Dh`) |
| `06` | `BEEP_LOW_HIGH_LOW` | low-high-low, fast | yes | 2 places |
| `07` | `BEEP_DOUBLE_ALERT` | two beeps; **the panel then sends `0xAA`** | yes | a repeating alert: started at `DE56h`, then sent again every `22h` ticks while `FE7E.1` is set (`D6D6h`) |
| `08` | `BEEP_LONG_LOW` | long, lower pitch | yes | 2 places |
| `09` | `BEEP_ERROR` | 6 or 7 very short beeps, "wrong entry" | yes | 1 place |
| `0A` | `BEEP_KEY` | short beep, as `02` | yes | 22 places: most likely the key beep |
| `0B` | `BEEP_POWER_ON` | longer beep, the `02` tone | yes | **power-on** (`A949h`, right after `app_init`), and 6 more |
| `0C` | `BEEP_DOUBLE` | double short beep, the `02` tone | yes | 7 places, several next to the AUX icon |
| `0D` | `BEEP_ALERT` | three long high beeps, alert | yes | 6 places through `C449h`, only when codeplug `E66Dh` bit 0 is clear and `P0.5`/`P0.6` are high |

**`0xB4` (`PANEL_BEEP_STOP`) stops a beep that is still playing**, and is
harmless otherwise. A new `0xB3` (`PANEL_BEEP`) number replaces whatever is
playing. The names ending `_CONT` are the beeps that need it. `[BENCH]`

## 11.12 Panel interrupt/vector requirement

Two stock external vector stubs are especially important:

```text
0x8092   wrapper leading into resident 1F33
0x8097   panel-start/INTE0 routing involving resident 1F30
```

`[FW][RE]`

The stock application and mask ROM cooperate at interrupt level. For early
custom firmware:

1. preserve/reconstruct the original external vector infrastructure;
2. retain the resident panel ISR wrappers;
3. change only application-level event handling;
4. replace the transport itself only after there is a reason to do so.

This is much safer than rewriting the timing-sensitive link merely for
architectural purity.

## 11.13 Best remaining panel bench test

A logic analyzer is still useful, but it is no longer required to discover the
basic panel protocol. The fastest remaining annunciator test is to use the
known resident transmit service and send only bytes already observed in the
stock application's main-to-panel stream, one family at a time while watching
the LCD:

```text
90 91
93 94 95
96 97
99 9A
9C 9D 9E
A0 A1
A3 A4
A7 A8
A9 AA
```

For each byte, record:

- which annunciator changes, if any;
- whether it turns on, turns off, or selects a blink/alternate state;
- whether the effect is latched until another command;
- whether text or another panel state changes at the same time.

`0x96`/`0x97` should be used as the sanity check because firmware tracing
already identifies them very strongly as TX on/off.

A logic-analyzer capture remains valuable for the **receive** dictionary. Capture
both panel lines while:

- powering on;
- pressing and releasing each of the eight keys;
- causing high/medium/low beeps;
- entering PROGRAM, CLONE, Dealer, and Alignment modes.

Record byte sequences, timing, and visible effect. There is currently no need to
dump the µPD75308 mask ROM merely to build a normal replacement-firmware UI.

---

Part of the [TK-x4x hardware reference](README.md) · previous: [External flash programming](Flash_Programming.md) · next: [Accessory connector, PTT and audio, squelch, model inputs](Accessory_Audio_and_Inputs.md)
