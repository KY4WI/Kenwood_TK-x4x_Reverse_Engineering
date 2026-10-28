# TK-x4x microphone-port serial

**Status:** living engineering reference, 2026-10-15. Research and design
notes; nothing here has been built or bench-tested yet unless marked
`[BENCH]`.

How the main CPU (µPD78312A) talks to a PC through the microphone jack: the
software ("bit-banged") serial port, the three implementations of it in the
stock code, what limits its speed, and a design for a faster
channel-programming session and a codeplug CRC. It is the place to pick
this work up again.

Evidence tags are the hardware reference's: `[MASK]` read from the common
mask ROM's code, `[FW]` read from a stock application image, `[DS]` from
NEC's µPD78312A documents, `[BENCH]` seen on a radio, `[RE]` inferred
strongly enough to design around, `[OPEN]` not known.

Application addresses are from the **TK-941 image with byte sum AB00h**
unless another image is named. The other two images checked are the
**TK-941 B5ECh** image and the **TK-840 8355h** image.

Related: [Programming Protocol](Programming_Protocol.md)
(the PC protocols), [Front Panel Link](../hardware/Front_Panel_Link.md)
(the panel link and display bytes), and the
[hardware reference](../hardware/README.md)
(sections 1.5 clocks, 10.4 the two programming modes, 12.4 the hardware
UART).

## 1. Summary

- The microphone-jack data lines go to the main CPU: **transmit on `P1.5`,
  receive on `P2.2`** (external interrupt 1). They are **two separate pins**
  (Dave, from the hardware). They are not the panel link (`P0.4`/`P2.1`).
- They are the mic's own lines (schematic, Dave, 2026-10-16): **`P1.5` is
  wired to the PTT line** (jack pin 14, also `P2.3`) and **`P2.2` is the
  hanger line** (pin 13, also `P1.4`). So the radio's serial output pulls
  the PTT line low on every 0 bit, and a mic's PTT and hanger switches
  disturb the serial lines. Mic-port serial needs a cable in the jack, not
  a microphone, and PTT must be ignored while sending. Hardware reference
  section 4.1.
- The serial port is software: a falling edge on `P2.2` starts a byte, a
  timer interrupt samples or drives each bit. It is interrupt driven, so the
  rest of the firmware runs between bits. `[MASK][FW]`
- **"PROGRAM" typed by the PC is heard by the panel CPU, not the main CPU.**
  The panel sends `55h A0h` over the panel link; the application branches to
  the mask ROM monitor at `1F00h`, which runs the whole channel-programming
  session. `[FW][BENCH]` Confirmed 2026-10-16 with the `benchtest` probe
  (`tools/probes/`), which shows panel bytes in hex: `PROGRAM` at 1200 baud
  on the mic jack shows `55 A0`.
- What appears on the display ("PROGRAM ", " TUNING ") is sent by the radio
  from its own strings, not echoed from the PC. `[MASK][FW]`
- The mask ROM's serial speeds are fixed: 1200 and 9600 for programming,
  19200 for the raw-binary firmware loader. A faster channel-programming
  session needs code in the application image. Section 6 is a design for one.

## 2. Timing

The crystal is 12 MHz. The mask ROM's reset code writes `STBC = 00h` (the
only write to it in either ROM), which makes the CPU clock fCLK = crystal ÷ 2
= **6 MHz**. Timers 0 and 1 count at fCLK ÷ 6 = **1 MHz**, so one count is
1 µs, and a bit time of N µs is `MD = N - 1`. `[DS][MASK]`

| Baud | Counts | `MD` | Actual | Error | Used by |
|---|---|---|---|---|---|
| 1200 | 833 | `0340h` | 1200.5 | +0.04% | mask ROM |
| 1200 | 834 | `0341h` | 1199 | -0.08% | AB00 tuning session |
| 9600 | 104 | `0067h` | 9615 | +0.16% | mask ROM |
| 9600 | 105 | `0068h` | 9524 | -0.8% | AB00 tuning session |
| 19200 | 52 | `0033h` | 19231 | +0.16% | mask ROM raw firmware loader |
| 38400 | 26 | `0019h` | 38462 | +0.16% | (proposed) |
| 57600 | 17 | `0010h` | 58824 | +2.1% | (marginal) |

The stock 9600 value would give about 19200 baud if the CPU ran at the full
12 MHz, so stock programming working at 9600 confirms the ÷2. `[RE]`

## 3. The stock implementations

All three use the same resident-RAM interface: `FECBh` holds a received byte,
`FED7.4` says one is ready, `FED7.7` says a byte is being sent, `FED7.5`
picks the speed (set = 9600, clear = 1200), `FEC8h` holds the byte being
sent. `[MASK][FW]`

### 3.1 Mask ROM KPG monitor (channel programming, `1F00h` → `0D34h`)

- **Send** `0F76h`: waits only if the previous byte is still going, then
  writes `MD1 = 0067h` or `0340h` (by `FED7.5`), pulls `P1.5` low for the
  start bit, starts timer 1 and **returns at once**. The timer-1 handler
  (`0FA4h`) shifts out 8 bits LSB first, then a stop bit of `MD1 = 0101h`
  (258 µs, about 2.5 bits at 9600) or `0683h` (2 bits at 1200). `[MASK]`
- **Receive** `0FDAh` (external interrupt 1, while `FEDE.2` is set): sets
  `MD0 = 0067h` or `0340h` again, starts timer 0 and masks itself; timer 0
  samples the bits; the byte lands in `FECBh` with `FED7.4` set. `[MASK]`
- Separate timers for each direction, so it could send and receive at once.
- **The speed is written for every byte**, from constants. Entering the
  monitor partway cannot change its speed, and the mask ROM cannot be
  changed. Leaving `TPF` set so interrupts go to application handlers fixes
  receive but not send: `0F76h` itself fixes the start bit at 104 µs before
  any handler runs. `[MASK]`

### 3.2 Mask ROM firmware loader (SYSTEM UP at power-on, `0273h`)

- **Receive only**, through a different start-bit handler (`17E5h`, used
  while `FEDE.2` is clear) that leaves `MD0` alone. The loader sets `MD0`
  once. `[MASK]`
- ASCII-hex path: **9600** (`MD0 = 0067h`), chosen by the first serial byte.
- Raw-binary path: chosen by panel byte `98h`, which is the **Scan key**
  (section 11.8 of the hardware reference). It sets **`MD0 = 0033h`, 19200
  baud**, waits for an `FFh`, then takes the whole `8000h-E5FFh` image as
  `CCh` pairs of 64-byte pages, receiving into `FE40h-FE7Fh` and
  `FE80h-FEBFh` alternately so one page is written while the next arrives.
  About 15 s for a full image. `[MASK]`; not bench-tested.
- Panel byte `9Ch` (the **AUX key**) calls `04CEh`. `[OPEN]` what it does.
- After `"PROGRAM "` (or its other screens) it sends `82h`, which makes the
  text flash. `[MASK][BENCH]`

### 3.3 AB00 tuning session (`tuning_session`, `BB63h`, panel byte `A2h`)

- **Send** `soft_serial_send_byte` (`C1AEh`, CALLT `09h`): `MD1 = 0068h` or
  `0341h` by `FED7.5`; the timer-1 handler (`soft_serial_tx_bit`, `C236h`)
  sends 8 bits and one stop bit, but the routine **waits** until the byte has
  gone. `[FW]`
- **Receive** `isr_exif1` (`C1E4h`, only while `FED7.1` is set) and
  `soft_serial_rx_bit` (`C209h`): timer 1 at `0068h`, 9600 only, stop bit not
  checked. `[FW]`
- One timer for both directions, so it cannot send and receive at once.
- Timer 1 is also the application's 3 ms tick (`MD1 = 0BB8h`); both routines
  put it back afterwards. `[FW]`
- The radio keeps running during this session, which shows the serial port
  can share the CPU with normal operation. `[FW]`

## 4. What is possible

- **Listening is fine.** Receive is interrupt driven in all three
  implementations; nothing about it forces a reset. The KPG monitor resets
  at the end only because it re-runs the start-up and uses half of internal
  RAM as its write buffer.
- **Full duplex**: use timer 0 for receive and timer 1 for send, as the mask
  ROM does.
- **Different speeds each way**: possible on the radio (two timers). On a
  PC, termios has separate input and output speeds (`cfsetispeed`,
  `cfsetospeed`, termios2 `c_ispeed`/`c_ospeed`), but 16550-style UARTs and
  the common USB adapters have one baud generator, so a PC can't do it with
  one port. Two adapters (one for RX, one for TX, common ground) work.
- **Background send**: a small transmit buffer drained by the timer
  interrupt makes sending a string almost free.
- **Speed limits.** While the radio runs, the panel link's tick interrupt
  fires every 103 µs during a panel transfer. A receive sample delayed by it
  has 52 µs of margin at 9600 but only 13 µs at 38400. So:
  - 9600 interrupt driven alongside normal operation: safe;
  - 19200: comfortable;
  - 38400 interrupt driven while the radio runs: needs the receive interrupt
    at top priority or no display updates while receiving;
  - 38400 with the radio stopped (interrupts off, polled bit loops): easy;
    57600 might work and needs measuring.
- **The hardware UART** (`P24`/`P25`, accessory connector) reaches 115200
  (`SCC = 0`, `BRG = 13`) with almost no CPU load, but it is not on the mic
  jack. `[BENCH]` (2026-10-16: clean at 1200-115200). Hardware reference
  section 12.4.
- **Our mic-port output** (the probes' `debug_serial_asm.s`): send only,
  polled, 2400 bit/s 8N2 on `P1.5`. `[BENCH]` (2026-10-16). It was slowed
  from 9600 while the vbcc string bug was garbling every message; 9600 is
  untested since the fix.
- **Other user I/O**: display text, icons and beeps over the panel link;
  front-panel keys as input. No cable needed.

## 5. Transfer times

Full codeplug, `E600h-FDFFh` (6144 bytes, 48 blocks of 128). Estimates from
the protocol, not measured:

| | 9600 | 38400 |
|---|---|---|
| Read | about 8 s | about 2 s |
| Write | about 9 s | about 3 s |
| Write and read back | about 17 s | about 5 s |

About 1 s of a full write is flash programming time (96 pages × 10 ms) and
does not change with speed. Each block has three changes of direction; with
an FTDI adapter's default 16 ms latency timer that adds up to about 2 s per
full read, so set the latency timer to 1 ms. Programming cables with simple
transistor or RC level shifting may round the edges at 38400; scope the
cable first.

## 6. Design: a faster channel-programming session

Not built. The idea (Dave's): patch the stock application so panel byte
`A0h` goes to a new routine in the image's free space instead of `1F00h`.
Everything else in the stock firmware stays as it is. The routine behaves
exactly like the stock session unless the PC asks for a higher speed. When
the session ends it stops servicing the watchdog, and the radio reboots
normally, as after a stock session.

### 6.1 Patch points and free space

| Image | `br !1F00h` sites (panel `A0h`) | Free space | Kick + RET on a CALLT slot | Free CALLT slots |
|---|---|---|---|---|
| TK-941 AB00h | `9F3Fh`, `AB3Bh`, `AF04h`, `E033h` | `E328h-E57Fh`, 600 bytes | `1Dh` → `D4E2h` | `1Eh`, `1Fh` |
| TK-840 8355h | `9EBEh`, `AAA5h`, `AE4Eh` | `E450h-E57Fh`, 304 bytes | `1Dh` → `D851h` | `1Eh`, `1Fh` |
| TK-941 B5ECh | `9F5Dh`, `ABA7h`, `AF70h`, `E196h` | `E492h-E57Fh`, 238 bytes | none | `1Dh`, `1Eh`, `1Fh` |

Each site is a three-byte `br !1F00h` (`2C 00 1F`); only the target changes.
In AB00 three of the four take `A0h` only right after `55h`; the one at
`9F3Fh` takes it alone. Patch all of them. `[FW]`

All three images also have 65 bytes of `FFh` at `E5BFh-E5FFh` and 18 at
`E59Eh-E5AFh`, around the signature area: `"KENWOOD Corporation LTR Mobile"`
at `E580h`, then `"HEY!  MOTOROLA "` at `E5B0h`. The mask ROM's signature
check (`018Bh`) compares only `E580h-E586h` (`"KENWOOD"`) with its own copy
at `08D7h`, so everything from `E587h` up can be used; the two strings
there are not checked. `[FW][MASK]`

The inline watchdog kick is 8 bytes; a kick-and-RET routine is 9. Through a
CALL (3 bytes) it pays from the second use, through CALLT (1 byte) the
same. In AB00 and 8355h it already exists on CALLT `1Dh`, so a kick costs 1
byte. B5ECh needs the 9-byte routine and a free slot. CALLT works only
while `CCW` `TPF` is set; the routine never clears it. The mask ROM has no
kick-and-RET (all 38 of its kicks are inline). `[FW][MASK]`

### 6.2 Mask ROM code to call instead of copying

Every KPG monitor command handler sends and receives through `0F76h`/`0F65h`
(fixed speeds), so the handlers must be rewritten. These parts do not depend
on the speed: `[MASK]`

| Address | What it does |
|---|---|
| `1F48h` (`1129h`) | flash page write: unlock (`AAh`/`55h`/`A0h` to `D555h`/`AAAAh`), 64 bytes from `HL` (internal RAM) to `DE`, then a fixed 10 ms wait on timer 1 (`MD1 = 2710h`, polls `TMIC1.7`). No erase needed. No verify. |
| `0F32h` | the same, address from `FECEh`, half from `FEDD.6`, buffers `FE40h`/`FE80h` (fits the `W` command) |
| `0E35h` | sets `FEDE.4` if a password is set (10 bytes at `E670h` not all `FFh`): the `Y`/`N` after the model ID |
| `1F4Bh` | fills the six model-ID bytes, `FEC5h` down to `FEC0h` |
| `102Bh` | exit housekeeping after a write: EEPROM clean-up and `E66Fh` copied to EEPROM |
| `15C8h` + `1E00h` | `02h` + `"PROGRAM "` to the panel (6 bytes of code) |
| `1F2Dh` | one byte to the panel (the `8Eh`) |
| `0DEEh` | `nop` / `br $-1`: jump there after the last `06h` and the watchdog resets the radio |

No single panel byte is known to show "PROGRAM"; the built-in strings found
so far are `85h` `"- SCAN -"` and `86h` `"AUTONTEL"`. `84h` and `87h` have
not been tried. `[BENCH]`

### 6.3 Rules the routine must follow

1. **Interrupts off around every `1F48h` call.** During the 10 ms the flash
   chip cannot be read, and the vectors and handlers live in it. The stock
   application does exactly this (`81E8h`: `di`, `call !1F48h`, `ei`).
   `1F48h` also reprograms timer 1, so reload `MD1` afterwards. `[MASK][FW]`
2. **Take the radio over first.** Show `8Eh` + `"PROGRAM "`, then stop using
   the panel link. Only then use `FE40h-FEBFh` as the 128-byte buffer: it
   covers the panel queue (`FEBCh-FEBEh`) and register banks. Internal RAM is
   only `FE00h-FEFFh`; below `FE00h` is the flash bus. The stack is 32 bytes.
3. **The stock `W` rules:** only addresses `≥ E600h`, only lengths `40h` or
   `80h`, only after `02h` has been answered with `06h`; anything else gets
   `15h`.
4. **Protect the page `E640h-E67Fh`.** Bit 2 of `E66Ch` enables the firmware
   loader; the page also holds the password (`E670h-E679h`) and `E66Fh`. The
   flash rewrites all 64 bytes of a page on every write, so a bad write or a
   power drop anywhere in that page can clear the loader enable. With the
   loader disabled and the firmware broken, the radio is bricked. During
   development:
   - put the guard in the page-write wrapper, on `DE`, right before `1F48h`,
     so no parsing bug can reach the page;
   - keep a way to the genuine monitor (leave one `br !1F00h` site
     unpatched, or go to `1F00h` while a key is held), so stock KPG can
     always repair the codeplug.
5. **On exit after a write, call `102Bh`**, as the monitor does.
6. **If the fast link fails** (no valid command within a couple of seconds
   after switching), stop kicking the watchdog. The radio reboots and the PC
   retries at 9600.
7. Use **polled** bit loops with interrupts off once the radio is taken
   over: no handlers, no panel-link interference, 38400 easy.

### 6.4 Choosing the speed without confusing stock firmware

Dave's patches carry a **bit-mapped feature byte** in the image. Assign a
bit to "fast programming". The CHIRP driver reads the byte (a stock `R`
reads anywhere) and sends the speed command only if the bit is set, so
stock firmware never receives a command it doesn't know. `[RE]`

The driver must know the bit before the session it wants to speed up. Three
ways:

1. **Cache it per radio** (preferred, no radio code): the first session runs
   at 9600 and reads the bit; later sessions open with the speed command.
   If stock firmware comes back, the command goes unanswered, so the driver
   falls back to 9600 and clears the cache.
2. **Two sessions:** read the bit, send `E` (reboot, about a second), start a
   fast session.
3. **The routine answers `02h` and `R` itself at 9600** so the driver can read
   the bit and switch in one session. About 100 more bytes; fits AB00 only.

Speed switch: a byte the monitor ignores (for example `42h` + a speed code),
answered with `06h` at 9600; then both sides switch after a few ms.

### 6.5 Size

Hand-written assembly, ballpark:

| Part | Bytes |
|---|---|
| Polled send/receive at a variable bit time, receive timeout | 80-100 |
| Entry (display, DI, timers, 1200 ACK), speed switch, command loop | 90-110 |
| `02h`, `03h`, `04h` | 90-110 |
| `R` | 50-60 |
| `W` with range checks, page guard, two page writes | 70-90 |
| `E`, odds and ends | 20-30 |
| **Full copy** | **about 400-500** (350-450 with the reuse above) |

A full copy fits AB00 only. A smaller variant fits all three: do the
monitor's start-up calls (`0D44h-0D7Ch`, all mask ROM subroutines), show
PROGRAM, send the ACK at 1200, read the first command at 9600. If it is the
speed command, run the fast session (`R`, `W`, `E`, optionally `02h`).
Otherwise put the byte in `FECBh`, set `FED7.4`, set up the monitor's
receive the way it expects (`TPF` clear, `FEDE.2` and `FED7.5` set, `EXIF1`
unmasked) and branch into the **real monitor's command loop at `0D99h`**.
Stock software then gets the genuine Kenwood session. About **250-300
bytes**. `[RE]`; the hand-over at `0D99h` is not fully traced (`[OPEN]`
whether the loop needs anything else its own start-up sets).

## 7. Firmware updates

- The mask ROM's loader already has a **19200** path (raw binary, Scan key),
  section 3.2. It needs no custom code.
- A custom loader in flash can rewrite the whole image, its own page
  included. It writes every other page with `CALL !1F48h` (interrupts off;
  the 10 ms wait runs in the mask ROM). For its own page, push a mask ROM
  address as the return address and branch instead of calling:

  ```asm
  	di
  	movw	hl,#buffer	; 64 bytes in internal RAM
  	movw	de,#lastpage	; the loader's own page
  	movw	ax,#0DEEh	; mask ROM "nop / br $-1"
  	push	ax		; becomes 1F48h's return address
  	br	!1F48h		; write the page, then "return" into the mask ROM
  ```

  The CPU never fetches from the rewritten page; the watchdog resets the
  radio and the new image boots. `[RE]`
- A damaged image fails the boot signature check, or SYSTEM UP forces the
  loader, so this cannot brick the radio while `E66Ch` bit 2 keeps the
  loader enabled.

## 8. Design: codeplug CRC-16

Not built. Dave's idea: add a CRC-16 so a codeplug transfer is known to be
good, with everything else unchanged.

**The codeplug only changes during programming.** The images write flash in
only three places: the mask ROM monitor's `W`, the hardware-UART memory
session (`81E9h` in AB00), and the panel-`A1h` codeplug erase (`96B5h`);
B5ECh (`81F0h`, `96BEh`) and 8355h (`8173h`, `96B8h`) match. So a CRC of
`E600h-FDFFh` stays valid in normal operation. `[FW]`

Two places a CRC helps, cheapest first:

1. **Per block, in the fast protocol** (Dave's choice). Each block carries
   its CRC after the data, high byte first, and the receiver runs the CRC
   over everything including those two bytes: the result is **zero** for a
   good block. The routine checks a `W` block **before** writing flash and
   NAKs a bad one, so a corrupted block never reaches the flash (or the
   protected page). `R` replies carry a CRC the same way. The stock protocol
   has no check at all.
   - Cover the header too (`W`/`R`, `AH`, `AL`, length), not only the 128
     data bytes: otherwise a damaged address byte writes a good block to the
     wrong place, possibly the protected page. A `W` frame is then 4 + 128 +
     2 = 134 bytes.
   - The zero result needs a CRC with no final XOR and the CRC sent in the
     CRC's own bit order. CRC-16/CCITT-FALSE sent high byte first gives zero
     (checked with Python's `binascii.crc_hqx`); low byte first does not.
     CRC-16/MODBUS (reflected) gives zero with the low byte first; CRCs with
     a final XOR, such as X-25, leave a fixed non-zero value instead.
   - The radio can run the CRC after the whole block has arrived rather than
     between bytes: about 128 × a few tens of µs, a few ms, while the PC
     waits for the ACK anyway.
2. **A CRC command instead of reading back.** A fast-session command returns
   the CRC of a range (for example `E600h-FDFFh`) computed by the radio. The
   driver compares it with its own, so a full write is verified without
   reading 6 KB back: about 3 s instead of 5 at 38400.

Storing a CRC in the codeplug is the weaker option: it needs two bytes that
the stock firmware and KPG never use (`[OPEN]` which), and any stock-tool
write would leave it wrong. Checking it at boot would change stock
behaviour. The on-demand command avoids both.

CRC-16/CCITT-FALSE (polynomial `1021h`, initial `FFFFh`) is a good choice:
Python's `binascii.crc_hqx(data, 0xFFFF)` computes it, which suits a CHIRP
driver. A bitwise loop is about 30-40 bytes of 78K/III code and costs a few
milliseconds per 128-byte block. A whole codeplug takes a fraction of a
second; a 32-byte nibble table makes it several times faster. `[RE]`

## 9. Open questions

- Find what makes the panel send `A1h` or `A2h`.
- Whether the mic-jack data line also reaches the panel CPU (schematic).
- Scope the programming cable at 38400.
- The raw-binary loader at 19200: bench test; and what panel byte `9Ch`
  (AUX) does in the loader (`04CEh`).
- Whether anything else must be set up before entering the monitor's loop at
  `0D99h`.
- Codeplug bytes free for a stored CRC, if ever wanted.
- What `84h` and `87h` show on the display.
