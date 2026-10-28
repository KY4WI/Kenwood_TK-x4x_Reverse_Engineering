# TK-x4x front panel link

**Status:** living engineering reference, 2026-10-14.

How the main CPU (µPD78312A, 12 MHz oscillator per schematic) talks to the
front panel's own CPU (µPD75308, approximately 4.194 MHz oscillator per
schematic), how the stock application hooks into the mask ROM to do it,
and what replacement firmware must do to use the link. This document goes
deeper than sections 11 and 17 of the
[hardware reference](README.md), and
corrects them where they differ.

Evidence tags are the hardware reference's: `[MASK]` read from the common
mask ROM's code, `[FW]` read from a stock application image, `[DS]` from
NEC's µPD78312A datasheet and user's manual (in `docs/NEC/`), `[RE]`
inferred strongly enough to design around, `[OPEN]` not known.

Mask ROM addresses are the same in every TK-x4x radio. Application
addresses in this document are from the **TK-941 image with byte sum
AB00h**. The hardware reference's application addresses (8F22h, A43Fh,
B1B0h, 9E4Ch and its CALLT table) are from the **TK-840 image with byte sum
8355h**; the two images are laid out differently.

## 1. Summary

- The panel and the main CPU swap **one byte per transfer, both ways at
  once**, framed like an asynchronous serial byte (start bit, 8 data bits
  LSB first, stop). The **panel starts every transfer**; the main CPU asks
  for one by pulling its line low. `[MASK]`
- The main CPU's side runs entirely in two mask ROM interrupt handlers:
  **EXIF0** (the panel's start edge) and **CRF11** (up/down counter 1's
  compare, twice per bit). Without both, nothing moves. `[MASK]`
- `1F2Dh` sends one byte: it waits (with **no time limit**) until the link
  is free, then starts the request and returns. Bytes from the panel land
  in a four-byte queue at `FE2Ch`/`FEBCh-FEBEh`. `[MASK]`
- **TPF, bit 1 of the CPU control word CCW (SFR FF4Eh), switches the
  interrupt vectors and the CALLT table** from the mask ROM (0000h-007Fh)
  to the application image (8000h-807Fh). Reset clears it. The stock
  application sets it in its start-up routine, just before it unmasks the
  panel interrupt. `[DS][MASK][FW]`
- The link runs at **about 4,800 bit/s**: a bit is 206 µs, a transfer
  2.4 ms. `[DS][MASK]`
- The byte stream to the panel is made of **sequences**, not single
  commands: `02h` then characters, `B3h` then a beep number, `8Bh` then two
  bytes. The same value means different things depending on what came just
  before it (`B3h 02h` is a beep, not text). `[FW]`

## 2. Lines

| Main CPU | Direction | Panel CPU | Idle |
|---|---|---|---|
| `P0.4` | out | `P10` | high |
| `P2.1` (external interrupt 0, `EXIF0`) | in | `P33` | high |

`[SM][MASK]`. The mask ROM's port set-up (`1F0Fh`) leaves `P0.4` high.
`INTM` (`FF48h`) selects the falling edge on `INTE0` from reset, and neither
ROM writes it. `[DS][MASK][FW]`

## 3. One transfer

**Starting it.** To send, the main CPU stores its byte and pulls `P0.4`
low (`1F2Dh`). The panel answers by pulling `P2.1` low: its start bit. The
panel can also start a transfer by itself, to report a key; the main CPU
then sends `FFh`, which means "nothing". `[MASK]`

**The start edge** (`EXIF0`, mask ROM `17A3h`): mask `EXIF0` (the data bits
make more edges), pull `P0.4` low (the main CPU's start bit; already low if
it asked), clear up/down counter 1 (`UDC1`, `FF1Eh`), set the bit count to 9, unmask
`CRF11`, and mark the link busy. `[MASK]`

**The ticks** (`CRF11`, mask ROM `1726h`), counted from the start edge:

| Tick | Action |
|---|---|
| 1 | read `P2.1`: the panel's start bit (it drops out after nine reads) |
| 2 | `P0.4` = main CPU's bit 0 |
| 3 | read `P2.1`: panel's bit 0 |
| 4 ... 17 | the same for bits 1-7: even ticks send, odd ticks read |
| 18-21 | `P0.4` high: the stop bits (two bit times) |
| 22 | wait one more tick |
| 23 | end: mask `CRF11`, set the next byte to send to `FFh`, unmask `EXIF0`, queue the received byte unless it is `FFh`, mark the link free |

So a bit is two ticks. The main CPU changes its line at the start of a
bit and reads the panel's line half a bit later, in the middle of the
panel's bit. `[MASK]`

**Timing.** `[DS][MASK]` The reset code (`sub_009E`) sets:

| Address | Register | Value | Effect |
|---|---|---|---|
| `00A6h` | `STBC` | `00h` | Clock divider consistent with fCLK = 12 MHz / 2 = 6 MHz (inferred, not measured) |
| `00B8h` | `UDCC1` (`FF7Ah`) | `81h` | `UDC1` counts up at fCLK/3 = 2 MHz, in up/down modulo mode |
| `00BBh` | `CRC` (`FF74h`) | `00h` | `CR10`/`CR11` compare, not capture |
| `00C1h` | `CR11` (`FF0Eh`) | `00CDh` | in modulo mode `UDC1` clears on the count after it matches `CR11`: 206 counts per tick |

Under that inferred clock configuration, a tick is 103 µs and a bit 206 µs: **about 4,850 bit/s, which is 4,800
baud 1.1% fast**. A transfer is 23 ticks, 2.4 ms, plus however long the
panel takes to answer. Clearing `UDC1` at the start edge puts the first tick
half a bit after the edge, in the middle of the panel's start bit. Only
`sub_009E` writes these registers; it runs at reset, in the mask ROM's
other modes and in AB00's restarts.

**Interrupt use.** Both handlers re-enable interrupts at once (`EI`), so
others can nest. The tick handler uses register `A` in **register bank 1**
(it sets `RBS0` in the PSW), so the interrupted code must be running in
bank 0, and the bank 1 registers (RAM `FEE0h-FEEFh`; bank 0 is
`FEF0h-FEFFh`) are not safe for data. `[MASK][DS]`

## 4. State in the resident RAM

| Address | Name | Meaning |
|---|---|---|
| `FE25h` | `panel_tx_byte` | the byte to send, shifted out LSB first; `FFh` between transfers |
| `FE26h` | `panel_rx_byte` | the byte being received |
| `FE27h` | `panel_bit_count` | data bits still to read, then stop ticks |
| `FE2Ch` | `panel_rx_queue` | the oldest received byte, valid while `FEDAh` bit 2 is set |
| `FEBCh-FEBEh` | `panel_rx_queue_1..3` | the next three, valid while `FEDDh` bits 0-2 are set |
| `FED9h` bit 7 | | a transfer is running (start edge to end) |
| `FED9h` bit 4 | | the next tick reads the panel's bit (clear: sends ours) |
| `FED9h` bit 3 | | data done: stop bits |
| `FED9h` bits 5, 6 | | stop bits done; one tick left |
| `FEDAh` bit 1 | | **link busy**: a byte is waiting to go or going |
| `FEDAh` bit 2 | | `panel_rx_queue` holds a byte |
| `FEDDh` bits 0-2 | | `panel_rx_queue_1..3` hold a byte |

`[MASK]`. The other bits of `FED9h`, `FEDAh` and `FEDDh` belong to other
code. When all four queue slots are full, a fifth byte overwrites the
last. `1F0Ch` (clear `FE20h-FEEFh`) resets all of this.

## 5. Mask ROM entries

| Entry | Routine | Use |
|---|---|---|
| `1F2Dh` | `panel_tx_start` (`1714h`) | send `A`. Kick the watchdog; wait while `FEDAh` bit 1 is set; store the byte; set `FEDAh` bit 1; pull `P0.4` low; return. Returns before the byte is sent. |
| `1F2Ah` | `panel_rx_recover` (`170Ah`) | if no transfer is running and `P2.1` is already low, the start edge came while `EXIF0` could not be taken: set `EXIF0`'s request flag so the handler runs. The mask ROM's own wait loops call it; the AB00 application never does. |
| `1F30h` | `exif0_handler` (`17A3h`) | for the application's `EXIF0` vector. Ends in `RETI`: branch to it, never call it. |
| `1F33h` | `crf11_handler` (`1726h`) | for the application's `CRF11` vector. Ends in `RETI`. |

`[MASK]`. `1F2Dh`'s wait has no time limit and kicks the watchdog, so if
transfers never start (no panel answer, `EXIF0` masked, no `CRF11`), the
second send hangs for good.

The mask ROM's own text routines send `02h` and then 8 characters
(`15C8h`) or 4 (`15EEh`) from `HL`. `[MASK]`

## 6. How the application hooks in

### 6.1 CCW TPF: vectors and CALLT

SFR `FF4Eh` is `CCW`, the CPU control word. Its bit 1 is `TPF`, the table
position flag. Reset clears it, and the CPU then takes its interrupt vectors
from 0000h-003Fh and the CALLT table from 0040h-007Fh, in the mask ROM.
With `TPF` set it uses 8000h-803Fh and 8040h-807Fh in the application
image instead. `[DS]` (µPD78312A user's manual, chapter 3; datasheet
section 4.2.1.) The code agrees throughout:

- Every mask ROM entry that takes over the machine clears the bit first:
  reset (`0049h`), the KPG monitor (`0D3Dh`), CLONE mode (`1196h`). `[MASK]`
- The mask ROM never uses CALLT. Its 0040h-007Fh is reset code, so a CALLT
  through it would land in the middle of instructions. `[MASK]`
- The application uses CALLT more than a thousand times, and its own table
  at 8040h holds its own routines (`callt [00h]` is its panel sender). `[FW]`
- Each AB00 start-up routine (`app_init` at `B433h`, `app_init_service` at
  `A50Ch`) reaches the mask ROM with plain `CALL`s, sets the bit, unmasks
  `EXIF0`, and only then uses its first CALLT. The warm restart (`9862h`)
  clears the bit before re-running the mask ROM's start-up, then calls
  `app_init` again. `[FW]`
- The image at 8000h holds a complete vector table with the same slots as
  the hardware's, pointing at the application's own handlers. `[FW]`

For replacement firmware this means:

- **Leaving the bit clear** keeps the mask ROM's vectors: `EXIF0` and
  `CRF11` go straight to the panel handlers, which is all the link needs.
  But `CALLT` then reads the mask ROM's reset code, so **do not use
  CALLT**, and every other interrupt goes to the mask ROM's handlers
  (`TMF0`, `TMF1` and `EXIF1` there dispatch on `FEDEh` bits; most others are
  a bare `RETI`).
- **Setting the bit** makes the image's table at 8000h live. It must then
  route `EXIF0` (8004h) to `1F30h` and `CRF11` (8020h) to `1F33h`. The AB00
  `CRF11` vector clears `RSS` (PSW bit 5) before branching, so `A` means `A`
  in the mask ROM's handler; `RETI` restores it. Set the bit only once the
  table at 8000h-807Fh is complete. `[FW]`

### 6.2 Start-up

When the mask ROM jumps to 8080h, interrupts are **enabled**, `EXIF0` is
**unmasked**, the panel has answered the `B0h`/`B1h` handshake, and the
power-on key check is done. `[MASK]`

AB00's normal start (`8080h` → `A92Ah` → `app_init`), in order: kick the
watchdog; `1F0Ch` (clear `FE20h-FEEFh`, including the link state); `1F0Fh`
(ports, `P0.4` high); `1F12h` (every interrupt masked); `1F15h` (model);
set `FF4Eh` bit 1; clear `EXIF0`'s request flag; unmask `EXIF0`. From this
point the link works. Interrupts stay enabled throughout. `[FW]`

### 6.3 The two senders

AB00 does not call `1F2Dh`. It has two copies of the same send
(`FEDAh` bit 1 wait, store, set busy, `P0.4` low) behind CALLT slots:

- slot 00h, `8F07h`: while it waits it keeps three background routines
  running (CALLT slots 02h and 03h, and the EEPROM callback at 8235h).
  With `FEDCh` bit 2 set it also copies every panel byte to the hardware
  UART. `[FW]`
- slot 01h, `A4E1h`: while it waits it runs two timer-tick routines
  instead (`A109h`, `A168h`), then sends with `A4FAh`. `[FW]`

Which one a caller uses depends on context (for example `FE7Bh` bit 3 in
the beep routines). `[FW]`

### 6.4 Taking bytes from the queue

`panel_rx_pop` (`B26Ch`) returns `FE2Ch` in `A` and moves the queue up:
`FEDDh` bit 0 → `FEDAh` bit 2, bit 1 → bit 0, bit 2 → bit 1, bit 2 cleared;
`FEBCh` → `FE2Ch`, `FEBDh` → `FEBCh`, `FEBEh` → `FEBDh`, `FFh` → `FEBEh`.
Callers test `FEDAh` bit 2 first. `panel_event_dispatch` (`B547h`) instead
just clears `FEDAh` bit 2 and reads `FE2Ch`. The stock code does not hold
interrupts off while popping. `[FW]`

### 6.5 Holding the link off while transmitting

While `P1.7` (`KEY`) is high, and `FEDAh` bit 1, `FE7Fh` bit 4 and `FED4h`
bit 2 are all clear, AB00's `EXIF0` vector does not pass the panel's start
edge to the mask ROM. It masks `EXIF0`, keeps `P0.4` low and sets
`FE6Dh` bit 5 (`8B0Bh`). Two passes of a timer-tick routine later
(`A14Fh` or `D591h`) it sets `P0.4` high, clears the pending request and
unmasks `EXIF0`. Code that drops `KEY` first waits for any such hold to
end. Why is `[OPEN]`: perhaps to keep panel traffic off the transmitter's
start. `[FW]`

## 7. Bytes to the panel

All of these come from the stock code. What each one does on the display
comes from Dave's bench sessions (2026-10-08); the full table of icon
bytes, whole-display bytes and built-in strings is in the hardware
reference, section 11.10.
Names for every byte we understand are in
[`include/panel_codes.h`](../../include/panel_codes.h); they are shown in
brackets below.

| Sequence | Where | Meaning |
|---|---|---|
| `02h` (`PANEL_TEXT`) + characters | mask ROM `15C8h` (8), `15EEh` (4); AB00 `A5F0h`, `C265h`, `C285h` (8), `C279h` (4) | text. `02h` starts it; 4 or 8 characters follow. `[MASK][FW]` |
| `B3h` (`PANEL_BEEP`) + `01h`...`0Dh` (`BEEP_...`) | AB00 `C2A1h`-`C47Bh` | beep, by number. All 19 `B3h` sends in AB00 are followed by one of these. `B3h` alone, or followed by `0Eh`, locks up the panel. `[FW][BENCH]`; beep `[RE]` |
| `8Bh` (`PANEL_SYS_GROUP`) + 2 bytes | AB00 `A595h`, `A5CAh` | shown instead of a channel name when the name is blank; the bytes are `FE23h` and `FE24h`, and the display shows `"<n> - <m>"` (system and group number). `[FW][BENCH]` |
| `B0h` (`PANEL_SYNC`, `PANEL_CLEAR`) | mask ROM `0159h` | handshake at reset; the panel answers `B1h`. Sent later, it clears the screen. `[MASK][BENCH]` |
| `82h` (`PANEL_FLASH_ALL`) | mask ROM loader, after the PROGRAM screens (`0290h`, `02CDh`) | makes the text and all icons flash: why those screens flash. `[MASK][BENCH]` |
| `8Eh` (`PANEL_RESTART`) | mask ROM KPG monitor `0D71h` and CLONE mode `11CAh`; AB00 warm restart `9862h` | the first byte after each restart, before the first text. It turns the TX icon off; anything else it does is `[OPEN]`. `[BENCH]` |
| `96h` / `97h` (`PANEL_TX_ON` / `_OFF`) | mask ROM `0514h`; AB00 around `KEY` | TX icon on / off; see below. `[BENCH]` |
| `85h` / `86h` (`PANEL_SHOW_SCAN` / `_AUTOTEL`) | not used by the stock code | built-in strings `"- SCAN -"` / `"AUTONTEL"`. `[BENCH]` |

**Text.** AB00 shows a channel's 8-character name only if at least one
character is one it shows (`A8C7h`): `23h`-`7Ah` except `&`, `.`, `:`, `;`,
`<`, `>`, `[`, `]`, `^` and the backquote (`60h`). Others go out as spaces,
and an all-blank name shows the `8Bh` sequence instead. `[FW]` The panel's
real character set is in the hardware reference, section 11.10: upper case
only, lower case and `? ! " & : ; < > [ ] ^ { | } ~` blank, `.` drawn as a
comma. `[BENCH]`

**Beeps.** Each beep routine sets `FED3h` bit 2, sends `B3h` and the
number, and unmutes the audio (clears expander outputs `AM` and `AC`,
`FE28h` bits 0 and 1). Most also load a tick count into `FEABh`; the timer
routine clears `FED3h` bit 2 and sets bit 3 when it runs out, and some
callers wait for that. The panel makes the beep tones itself (hardware
reference 11.11), so these are taken to be beeps. Which sound each number
makes is `[OPEN]`. Because `B3h` takes the next byte as its number, **a
byte's meaning depends on the byte before it**: `B3h 02h` is beep 2, not
"text follows". `[FW][RE]`

The audio path matters: every stock start-up writes the expander as `24h`
or `E4h` (`AM` and `AC` clear), and each beep routine clears both again. The
mask ROM's reset value `25h` leaves `AM` set; firmware that keeps it should
not expect to hear a beep. The volume DAC (probably channel 5, hardware
reference 7.1.1) may need setting too. `[FW][RE]`

**96h / 97h.** These are the TX annunciator on and off (bench-confirmed;
AB00 sends them around keying). The mask ROM also sends
them at the end of a firmware load: `96h` when the image's computed sum
(`FE42h`) equals `FE40h`, else `97h`, right after showing the sum on the
display. So they are a two-state indicator that the mask ROM also uses
for "checksum good": the TX icon lights when the sum matches.
`[MASK][FW][BENCH]`

**From the panel, `55h A0h` starts programming mode:** AB00 branches
straight to the mask ROM's KPG monitor at `1F00h`, which takes the radio
over for a PC programming session and shows `"PROGRAM "`. **`55h A2h`**
starts AB00's own tuning session (`BB63h`, `" TUNING "`), and **`A1h`**
erases the codeplug and then starts the tuning session. Three of the four
dispatchers want the `55h` first; `A1h` never does. See
[Programming Protocol](../programming/Programming_Protocol.md). `[FW]`

The microphone-jack data lines are not the panel link. The sessions talk
to the PC on the main CPU's `P1.5` (out) and `P2.2` (in); the panel link is
`P0.4`/`P2.1`. Only the request to start a session arrives over the panel
link. `[FW]`

Other single bytes AB00 sends: `80h 81h 85h 90h 91h 93h 94h 95h 99h 9Ah
9Ch 9Dh 9Eh A0h A1h A3h A4h A7h A8h A9h AAh B4h`. The hardware reference
(11.10) has candidate meanings for most of them. Bytes from the panel to the
main CPU are a separate set: the same value can mean something else in
that direction.

## 8. Checklist for replacement firmware

1. Leave the mask ROM's reset path alone until 8080h: it does the panel
   handshake.
2. If you call `1F12h`, unmask `EXIF0` again afterwards (clear `EXIC0`
   bits 7 and 6) and enable interrupts.
3. Either leave `CCW` `TPF` (`FF4Eh` bit 1) clear and do not use CALLT, or
   provide a complete table at 8000h-807Fh with `EXIF0` → `1F30h` and
   `CRF11` → `1F33h`, then set it.
4. Do not hold interrupts off for more than a fraction of a tick while
   the link may be active, and do not run the main code in register bank 1.
5. Change port 0 only with bit instructions (`SET1`/`CLR1`), or with
   read-modify-write sequences that interrupts cannot split. `P0.4` changes
   inside the tick handler. `P2.1` must stay an input.
6. Keep variables and the stack out of `FE20h-FEEFh` except as the mask ROM
   documents (bank 1, `FEE0h-FEEFh`, is used by the tick handler).
7. Write `FE2Ch`, `FEBCh-FEBEh` and the queue flags only as `panel_rx_pop`
   does.
8. Do not call `1F2Dh` again while `FEDAh` bit 1 is still set unless you can
   afford to wait for ever; a bounded wait on that bit first is a sensible
   safety net.
9. Keep the stack inside internal RAM. The mask ROM's start-up sets `MM` to
   `07h`: 56 KB external memory, so everything below `FE00h` that isn't the
   internal ROM is the external bus (the flash chip's codeplug area at
   `FDxxh`). A stack that grows below `FE00h` reads back flash contents.
   With `SP` starting at `FE20h` there are only 32 bytes, and both panel
   handlers allow nesting. `[DS][MASK]`
10. For beeps, clear expander outputs `AM` and `AC` first (section 7).

**If a send never completes**, look at `FED9h` bit 7 after the first send:

- **Set and stuck:** the start edge was taken but no ticks came. `CRF11`
  is not reaching `1726h` (wrong vector, `FF4Eh` bit 1 set without a
  table) or up/down counter 1 is not running.
- **Clear:** no start edge was taken. Either `EXIF0` is masked or not
  reaching `17A3h`, interrupts are off, or the panel is not answering
  `P0.4` going low. A scope on `P0.4`/`P2.1` separates the two. If `P2.1`
  is already low, `1F2Ah` restarts the transfer.

**If sends complete but nothing shows**, the link works and the question
is the byte sequence: start with the mask ROM's own `02h` + 8 characters.

**The simplest text routine is the mask ROM's.** Point `HL` at exactly 8
characters (pad with spaces) and `CALL 15C8h`: it sends `02h` and the 8
characters, each through `1F2Dh`, from its own loop. Stock AB00 shows a
channel name the same way (`panel_show_name`, `A5F0h`), sending any
character the display can't show (outside `23h-7Ah`, or one of
`&` `.` `:` `;` `<` `>` `[` `]` `^` and the backtick) as a space. `[MASK][FW]`

## 9. Open questions

- A scope capture of one transfer, to confirm the 206 µs bit and see how
  long the panel takes to answer.
- What makes the panel send `A2h` or `A1h`. (`PROGRAM` at 1200 baud on the
  mic jack does arrive as `55h A0h`: confirmed on the bench, 2026-10-16.)
- What `8Eh` does besides turning the TX icon off, and what the panel's
  `AAh` after beep 07 means. (The beeps, `B4h` and `84h`/`87h` are in the
  hardware reference, sections 11.10-11.11.)
- Why AB00 holds the link off while keyed.
- Whether the TK-840 image (8355h) uses `FF4Eh` bit 1 the same way (its
  CALLT table at 8040h says it must).
