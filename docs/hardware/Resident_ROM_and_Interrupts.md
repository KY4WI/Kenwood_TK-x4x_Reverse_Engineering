<!-- Resident mask ROM services and interrupts -->
Part of the [TK-x4x hardware reference](README.md) · previous: [Accessory connector, PTT and audio, squelch, model inputs](Accessory_Audio_and_Inputs.md) · next: [Replacement firmware: bring-up, open items, design rules](Replacement_Firmware_Guide.md)

# 16. C access patterns already verified

## 16.1 SFR access

`[RE, compiled+verified]`

```c
typedef unsigned char  u8;
typedef unsigned short u16;

#define SFR8(addr)  (*(volatile u8  *)(addr))
#define SFR16(addr) (*(volatile u16 *)(addr))
```

Verified examples:

```c
SFR16(0xFF8E) = 0x0067;
SFR8(0xFF01) |= 0x20;
SFR8(0xFF01) &= ~0x20;

u8 test_tmic1_flag(void) {
    return SFR8(0xFFD0) & 0x80;
}
```

Current vbcc78k3 code generation uses generic pointer/register-indirect SFR
access rather than the compact native `sfr[]`/`saddr` encodings. It is correct
but larger/slower. Use assembly for strict-timing hardware loops.

## 16.2 No-argument resident-ROM calls

`[RE, compiled+verified]`

```c
typedef void (*fnptr)(void);

void call_gpio_init(void) {
    fnptr f = (fnptr)0x1F0F;
    f();
}
```

For resident functions with register-based arguments, use a small assembly shim
rather than assuming the C ABI matches the mask-ROM ABI.

---

# 17. Application CALLT table and resident interrupt integration

The first `0x80` bytes of external program space are infrastructure, not
ordinary free application space. The stock image supplies external vectors,
CALLT targets, and resident-ROM cooperation there. `[FW][RE]`

## 17.1 External CALLT table in the analyzed stock image

From the TK-840 image with byte sum `8355h`. The "table address" column is
the slot's address as the CALLT instruction names it; the table itself is at
`8040h-807Fh` of the image, and the CPU reads it there only while `CCW` `TPF`
is set (section 2.3). The TK-941 `AB00h` image's table holds different
addresses: see [Front Panel Link](Front_Panel_Link.md), section 6.

| Table address | Target |
|---:|---:|
| `0040` | `8F22` |
| `0042` | `A43F` |
| `0044` | `D85A` |
| `0046` | `D919` |
| `0048` | `A506` |
| `004A` | `ADA3` |
| `004C` | `8235` |
| `004E` | `8311` |
| `0050` | `8238` |
| `0052` | `C33A` |
| `0054` | `C329` |
| `0056` | `C41E` |
| `0058` | `C6D5` |
| `005A` | `1F1B` |
| `005C` | `1F1E` |
| `005E` | `C729` |
| `0060` | `E082` |
| `0062` | `DAB5` |
| `0064` | `808C` |
| `0066` | `808F` |
| `0068` | `A886` |
| `006A` | `C784` |
| `006C` | `8EDD` |
| `006E` | `8178` |
| `0070` | `A88D` |
| `0072` | `818A` |
| `0074` | `CF13` |
| `0076` | `897A` |
| `0078` | `E101` |
| `007A` | `D851` |
| `007C` | `FFFF` |
| `007E` | `FFFF` |

Notable known targets include:

```text
0040 -> 8F22   application panel-byte send wrapper
005A -> 1F1B   resident EEPROM random read
005C -> 1F1E   resident EEPROM byte write
0064 -> 808C   branch stub -> resident 1F27 (4094)
0066 -> 808F   branch stub -> resident 1F24 (DAC)
006E -> 8178   hardware-UART send_byte(A) -- see section 12.4
0072 -> 818A   hardware-UART recv_byte() -> A -- see section 12.4
```

Do not let a linker fill this table accidentally. Either preserve the stock ABI
or deliberately rebuild every slot that resident/application code can reach.

## 17.2 Fixed external application entries

Resident startup/mode logic transfers to:

```text
8080   normal application
8083   Dealer Mode
8086   Panel Tuning / Alignment
8089   Clone-mode path
```

`[MASK][RE]`

For the first replacement image, preserve these addresses even if they only
contain short branches to common code.

## 17.3 External panel vector stubs

At `0x8092`, stock code clears the required processor-status/control state and
branches to resident `1F33`.

At `0x8097`, stock code conditionally routes the panel-start/INTE0 interrupt
toward resident `1F30` or an application-side handler.

This is direct evidence that the panel link is a cooperative mask-ROM /
external-application interrupt subsystem, not merely a pair of ordinary
subroutine calls.

## 17.4 Callback at `0x8235`

Resident cooperative EEPROM entries `1F42` and `1F45` repeatedly call external
application address `0x8235`.

A replacement image that uses those services must provide a callable,
RET-compatible routine there. The blocking EEPROM routines do not require this
callback.

---

# 18. Resident mask-ROM service quick reference

| Entry | Function | Replacement-firmware use |
|---|---|---|
| `1F00` | PROGRAM monitor | resident PC programming mode, non-returning |
| `1F03` | binary16 -> packed BCD | utility |
| `1F06` | signed packed-BCD add | utility; special overflow behavior |
| `1F09` | signed packed-BCD subtract | utility; destructive source-sign behavior |
| `1F0C` | clear FE20-FEEF RAM | startup -- see 1.4 for why exactly FE20-FEEF (stack below, live register bank above) |
| `1F0F` | baseline GPIO init | startup |
| `1F12` | interrupt/peripheral baseline init | startup -- see section 19 for the exact per-register baseline and why Timer1 is left unmasked |
| `1F15` | hardware/model detect | startup/model selection |
| `1F18` | AT24C02 page/multi-byte write | calibration/service |
| `1F1B` | AT24C02 random byte read | calibration |
| `1F1E` | AT24C02 byte write | calibration/service |
| `1F21` | AT24C02 sequential read | calibration dump |
| `1F24` | M62363 DAC write | calibrated analog control |
| `1F27` | 4094 commit from FE28 | hardware control outputs |
| `1F2A` | panel RX/start recovery | panel transport |
| `1F2D` | panel TX byte | panel transport |
| `1F30` | panel INTE0 ISR | RETI-only interrupt handler |
| `1F33` | panel timing/state ISR | RETI-only interrupt handler |
| `1F36` | fixed 64-byte flash page write | specific E680 page helper |
| `1F39` | CLONE mode | non-returning resident mode |
| `1F3C` | flash SDP disable | flash service |
| `1F3F` | flash protected-program prefix | flash service |
| `1F42` | cooperative EEPROM byte write | requires callable 8235 hook |
| `1F45` | cooperative EEPROM byte read | requires callable 8235 hook |
| `1F48` | generic 64-byte flash page writer | firmware/codeplug self-update |
| `1F4B` | build model-ID field | model/programming identification |
| `009E` | system set-up: register bank, port controls, timer 1 period and start (`sub_sysclk_init`) | start-up. Not a `1Fxx` entry, but at the same address in every TK-x4x mask ROM; `reset_handler` (`0040h`) and CLONE mode (`118Dh`) both call it before using the panel |
| `0144` | bounded start-up wait: polls timer 1's request flag up to 65 times, kicking the watchdog (`sub_startup_wait`) | start-up, after `009E`. Same note |

---

# 19. Interrupt architecture and the mask-ROM interrupt-controller baseline

## 19.1 Full interrupt source / vector table

From the datasheet's Table 2 ("Interrupt Sources and Vector Addresses"). This
is the complete interrupt architecture of the chip -- every entry, not just
the ones this project has touched so far. `[DS]`

| Default priority | Mnemonic | Source | Macroservice | Vector | IC register |
|---:|---|---|:---:|---|---|
| - | `BRK` | Software break instruction | No | `003EH` | - |
| - | `NMI` | External nonmaskable interrupt | No | `0002H` | - (not maskable) |
| - | `WDT` | Watchdog timer | No | `000AH` | - (not maskable) |
| 0 | `CRF00` | Up/down counter 0 | Yes | `001AH` | `CRIC00` @ `FFC0H` |
| 1 | `CRF01` | Up/down counter 0 | No | `001CH` | `CRIC01` @ `FFC2H` |
| 2 | `CRF10` | Up/down counter 1 | Yes | `001EH` | `CRIC10` @ `FFC4H` |
| 3 | `CRF11` | Up/down counter 1 | No | `0020H` | `CRIC11` @ `FFC6H` |
| 4 | `EXIF0` | External interrupt 0 | Yes | `0004H` | `EXIC0` @ `FFC8H` |
| 5 | `EXIF1` | External interrupt 1 | Yes | `0006H` | `EXIC1` @ `FFCAH` |
| 6 | `EXIF2` | External interrupt 2 | Yes | `0008H` | `EXIC2` @ `FFCCH` |
| 7 | `TMF0` | Timer flag 0 | Yes | `000EH` | `TMIC0` @ `FFCEH` |
| 8 | `TMF1` | Timer flag 1 | Yes | `0010H` | `TMIC1` @ `FFD0H` |
| 9 | `TMF2` | Timer flag 2 | Yes | `0012H` | `TMIC2` @ `FFD2H` |
| 10 | `SEF` | Serial port error | No | `0022H` | `SEIC` @ `FFDAH` |
| 11 | `SRF` | Serial port receive buffer | Yes | `0024H` | `SRIC` @ `FFDCH` |
| 12 | `STF` | Serial port transmit buffer | Yes | `0026H` | `STIC` @ `FFDEH` |
| 13 | `ADF` | A/D converter done flag | Yes | `0028H` | `ADIC` @ `FFE0H` |
| 14 | `TBF` | Timebase counter flag | No | `000CH` | `TBIC` @ `FFE2H` |
| - | `RESET` | External reset line | - | `0000H` | - |

Each `xxIC` interrupt-control register (`CRIC00` etc.) is R/W, reset state
`47H`, bit7=IF (request flag), bit6=MK (mask, 1=masked), bits2-0=PR
(priority, 000=highest, 111=lowest) -- same bitfield convention already
recorded for `TMIC1` in the earlier revision of this document; now confirmed
to apply uniformly to all 15 maskable sources. `[DS]`

**`WDT` is nonmaskable** -- it has no `xxIC` register and cannot be disabled
by masking. If the watchdog is running and not serviced, it will force
vector `000AH` regardless of any interrupt-controller state or global
`DI`/`EI`. Whether the watchdog itself can be fully *stopped* (as opposed to
just having its interrupt masked, which isn't possible anyway) is a separate
question -- see 19.3 and open item 10 below. `[DS]`

**`EXIF0`/`EXIC0` (`FFC8H`, vector `0004H`) is the panel-link's own INTE0
interrupt** referenced in sections 11.12/17.3 (the `0x8097` panel-start/
INTE0 routing stub). Per 19.2 below, it is left *masked* by the mask ROM's
own baseline init, so something else must unmask it before panel-link
interrupts can actually fire. `[MASK]` cross-checked against the `[FW]` panel
finding in section 17.3. Confirmed 2026-10-14: the TK-941 `AB00h` start-up
routines unmask it right after setting `CCW` `TPF` (`FF4Eh` bit 1) ([Front Panel Link](Front_Panel_Link.md),
section 6.2). `[FW]`

## 19.2 `1F12`'s real baseline state, byte-exact

`1F12` (`irq_periph_init`) trampolines to `0x0116`, whose real body writes
these 15 interrupt-control registers directly -- confirmed from mask-ROM
disassembly and now identified against the datasheet tables above. Every
register `1F12` touches is accounted for below; this table is exhaustive,
not a sample. `[MASK][DS]`

| SFR write | Register | Interrupt source | Value | MK | Priority | Result |
|---|---|---|---:|:---:|:---:|---|
| `sfr[0xC0]=0x41` | `CRIC00` | `CRF00` | `41H` | 1 | 1 | masked |
| `sfr[0xC2]=0x41` | `CRIC01` | `CRF01` | `41H` | 1 | 1 | masked |
| `sfr[0xC4]=0x41` | `CRIC10` | `CRF10` | `41H` | 1 | 1 | masked |
| `sfr[0xC6]=0x41` | `CRIC11` | `CRF11` | `41H` | 1 | 1 | masked |
| `sfr[0xC8]=0x41` | `EXIC0` | `EXIF0` (panel INTE0) | `41H` | 1 | 1 | masked |
| `sfr[0xCA]=0x41` | `EXIC1` | `EXIF1` | `41H` | 1 | 1 | masked |
| `sfr[0xCC]=0x41` | `EXIC2` | `EXIF2` | `41H` | 1 | 1 | masked |
| `sfr[0xCE]=0x40` | `TMIC0` | `TMF0` | `40H` | 1 | 0 | masked |
| `sfr[0xD0]=0x80` | `TMIC1` | `TMF1` (**Timer1** -- this project's baud-rate timer) | `80H` | **0** | (group) | **request flag set, unmasked** (see note) |
| `sfr[0xD2]=0x40` | `TMIC2` | `TMF2` | `40H` | 1 | 0 | masked |
| `sfr[0xDA]=0x47` | `SEIC` | `SEF` | `47H` | 1 | 7 | masked (= reset default, unchanged) |
| `sfr[0xDC]=0x47` | `SRIC` | `SRF` | `47H` | 1 | 7 | masked (= reset default, unchanged) |
| `sfr[0xDE]=0x47` | `STIC` | `STF` | `47H` | 1 | 7 | masked (= reset default, unchanged) |
| `sfr[0xE0]=0x47` | `ADIC` | `ADF` | `47H` | 1 | 7 | masked (= reset default, unchanged) |
| `sfr[0xE2]=0x47` | `TBIC` | `TBF` | `47H` | 1 | 7 | masked (= reset default, unchanged) |

Macroservice-control counterparts (`CRMS00`, `EXMS0-2`, `TMMS0-2`, `SRMS`,
`STMS`, `ADMS`) and `WDM` are **not** touched by `1F12` at all.

**The one genuinely load-bearing fact here:** `1F12` deliberately leaves
exactly one interrupt live -- `TMIC1`/`TMF1`, i.e. **Timer1**, the same
timer (`TMC1`/`TM1`/`MD1` @ `FF82H`/`FF8C-8DH`/`FF8E-8FH`) this project
already uses as the reload source for Stage-1's bit-bang serial. This is
not incidental -- of 15 possible sources, the mask ROM's own baseline picks
Timer1 and no other, at maximum priority. That strongly suggests Timer1's
flag is expected to matter even during otherwise-`DI` early execution on
real hardware (stock firmware almost certainly runs a real ISR at `0010H`,
even though our own Stage-1 doesn't yet). It also means: **the moment
replacement firmware calls `EI`, Timer1's interrupt is live and
highest-priority by default**, with no other source competing -- harmless
only as long as vector `0010H` either has a real handler installed or
Timer1 is explicitly re-masked (e.g. `SFR8(0xFFD0) = 0x47`, restoring the
reset default) before `EI` is ever used. See design rule 11 below.

**Correction (2026-10-14, user's manual sections 4.5 and 5):** bit 7 of
each `xxIC` register is the request flag, bit 6 the mask, bit 5 macro
service, bit 4 context switching, bits 2-0 the priority. `TMIC1` and
`TMIC2` have no priority bits of their own: the timer group's priority is
set in `TMIC0` (`TMF0 > TMF1 > TMF2` within the group). So `1F12`'s
`TMIC1 = 80H` does not mean "highest priority": it **sets the `TMF1` request
flag** with the interrupt unmasked. The reset code then polls that flag with
interrupts disabled (`sub_0144`). After the reset handler's `EI`, every
timer-1 period (3 ms, `MD1 = 0BB8h`) is taken through the mask ROM's
`tmf1_handler`, which returns at once while its `FEDE` mode bits are
clear, until the application masks `TMIC1`. `[DS][MASK]`

## 19.3 Watchdog

`WDM` (watchdog timer mode register) is at `FF42H`, reset `00H`. Resolved
from the datasheet's own "Watchdog Timer" narrative section, cross-checked
against firmware tracing: `[DS][MASK][FW]`

- The datasheet's watchdog summary: the watchdog guards against a program
  that has run away; if software does not restart it in time it raises a
  nonmaskable interrupt; software picks one of four intervals between
  5.5 ms and 349.3 ms; **software can turn it off**; and its control
  register (`WDM`) is protected, so only a special instruction can write
  it. That settles the open question of whether the watchdog can be
  stopped: yes, by software, through `WDM`.
- `WDM` is written only ever with one of two literal values, always as an
  immediately-adjacent pair: `MOV WDM,#0x00` followed by `MOV WDM,#0x86`.
  This pattern recurs constantly through **both** the mask ROM (82
  instances) and the external firmware (36 instances) -- it is the
  watchdog-kick/service idiom, called throughout normal execution, not a
  one-time init.
- **In practice, the watchdog is running.** Regardless of what the bare
  hardware reset default technically is, both ROMs service it so
  persistently and pervasively that it must be treated as continuously
  armed during real operation.
- **`WDM` bits (2026-10-14, user's manual section 4.6.3):** bit 7 `RUN`
  (0 stops the watchdog; writing 1 clears it and starts counting), bit 4
  `PRC` (0: the `NMI` pin outranks the watchdog), bits 2-0 the count clock
  (000, 010, 100, 110: 5.5, 21.8, 87.4, 349.5 ms at 6 MHz). So **`0x86` is
  "run, `NMI` first, 349.5 ms"**. `[DS]`
- **Why every kick is `0x00` then `0x86`:** the watchdog also fires if it is
  cleared too *early*, before its counter's bit 4 first sets (6.25% of the
  period, 22 ms at `0x86`), except right after a 0-to-1 change of `RUN`.
  Writing `0x00` first stops it, so the `0x86` is always such a change and
  a kick can never be too early, however often it comes. A replacement
  firmware should kick the same way; writing `0x86` alone while running
  can trip the early-clear rule. `[DS]`
- `WDT` remains nonmaskable (section 19.1) -- there is no way to mask its
  interrupt once armed, only to disable the timer itself via `WDM`, or to
  keep servicing it.

**Consequence for replacement firmware (see design rule 13, section 23):**
replacement firmware must either periodically execute the same
`MOV WDM,#0x00` / `MOV WDM,#0x86` kick sequence (or an equivalent valid
`WDM` write), or explicitly disable the watchdog via `WDM` during early
bring-up, deliberately and with the choice recorded -- not by omission. Going
quiet on `WDM` after boot, the way an inexperienced port might, will reset
the CPU via the nonmaskable vector `0x000A` somewhere between 5.5 and 349.3 ms
after the last kick.

---

## 19.4 Register banks, `BRKCS` and context switching

Written 2026-10-29 while following up the `bankisr` probe idea
([`tools/probes/bankisr/`](../../tools/probes/bankisr/bankisr.md)).

**Where the banks are.** `[DS]` (manual 3.2.2, Figure 3-5, p. 3-10; Table
3-2, p. 3-11) The 8 banks are 16 bytes each in `FE80H`-`FEFFH`: bank *n*
starts at `FEF0H` - 16*n, so bank 7 is `FE80H`-`FE8FH` and bank 0 is
`FEF0H`-`FEFFH`. `R0` is the first byte of a bank and `R15` the last:
`X`=R0, `A`=R1, `C`=R2, `B`=R3, `R4`/`R5`/`R6`/`R7` (no names while
`RSS`=0; they are `X`/`A`/`C`/`B` when `RSS`=1), `VP`=R9:R8, `UP`=R11:R10,
`E`/`D`/`L`/`H`=R12-R15. `AX`=`RP0`=R1:R0, so bank 7's `AX` is `FE80H`/
`FE81H`; bank 0's is `FEF0H`/`FEF1H`.

**The banks are ordinary RAM.** `[FW][MASK]` Not using `SEL` does not
make a bank free. By the names the listings print, the stock app uses
nearly all of `FE80H`-`FEECH` as variables (bank 7's `FE84H`/`FE85H` is
the DDS phase accumulator). The mask ROM names `FEBFH`, `FEC0H`-`FECFH`
(bank 3) and `FED3H`-`FEDFH` (bank 2), and never names anything in
`FE80H`-`FEAFH` (banks 7, 6, 5). `[OPEN]` Accesses through a pointer
register are not visible to that check. A bank is only usable in an image
that replaces the stock app, and (for banks 1-4) only where the mask ROM
routines that touch it are not called.

**Hardware context switching.** `[DS]` (manual 5.3, pp. 5-25 to 5-28) A
bank can be used as a complete, self-contained subroutine/ISR context:

- A maskable interrupt whose `xxIC` bit 4 (`xxCSEn`) is set, taken with
  `EI`, selects the register bank *corresponding to the priority level* of
  its group (the 78312A text says priority level; the related 78355/356
  manual says the low three bits of the vector-table word, so do not
  assume the two are the same). The 16-bit entry address must already be
  stored in the bank's `R5:R4` (an ordinary RAM write). The hardware
  exchanges it with `PC`, saves the old `PSW` in `R7:R6`, and sets
  `RSS`=0 and `IE`=0. Nothing goes on the stack. 11 states from request
  to the first instruction of the routine.
- `BRKCS RBn` (2 bytes) does the same thing on demand, with the bank
  named in the instruction. A routine entered this way is a "subroutine"
  whose registers persist between calls.
- Return is `RETCS !addr16` (3 bytes) only: it restores `PC` from `R4:R5`
  and `PSW` from `R6:R7`, **stores `addr16` back into `R4:R5` as the next
  entry address**, and resets the highest-priority `ISPR` bit. After a
  `BRKCS` (no `ISPR` bit was set) the manual says to execute `SET1 CCW.0`
  (the `EOS` flag) immediately before `RETCS`, or interrupt nesting
  control is destroyed. Using any other return after a context switch
  breaks later interrupts.
- One saved `PC` per bank: a bank's routine can not be re-entered or call
  itself.
- The project's opcode table has `BRKCS` and `RETCS` only; the 78352/
  78356-family manuals also document a `RETCSB`, which is not in the
  78312A table.

**In the stock firmware.** `[FW]` There is no `SEL`, `BRK`, `BRKCS` or
`RETCS` anywhere in the TK-840 image. Every direct immediate write to an
`xxIC` register in the app and mask ROM listings has bit 4 clear (values
`00H`, `40H`, `41H`, `42H`, `47H`, `80H`) and there is no bit-4 operation
on any of them, so every interrupt is a normal vectored one.

`[OPEN]` Nothing in 19.4 has been tried on hardware, and the states for
`BRKCS`/`RETCS` have not been looked up, so whether this beats `CALL`/
`RET` or `PUSH`/`POP` in cycles is unknown.

# 20. Useful SFR addresses already used by project code

| Symbol | Address | Notes |
|---|---|---|
| `P0` | `0xFF00` | P00-P07 port latch/read |
| `P1` | `0xFF01` | P10-P17 |
| `P2` | `0xFF02` | P20-P27 / alternate-function pins |
| `P3` | `0xFF03` | P30-P37 / alternate-function pins |
| `TMC0` | `0xFF80` | Timer0 control |
| `TMC1` | `0xFF82` | Timer1 control |
| `TM0` | `0xFF88/89` | Timer0 counter word |
| `MD0` | `0xFF8A/8B` | Timer0 modulus word |
| `TM1` | `0xFF8C/8D` | Timer1 counter word |
| `MD1` | `0xFF8E/8F` | Timer1 modulus word |
| `TMIC1` | `0xFFD0` | Timer1 interrupt control; bit7 IF, bit6 MK, bits0-2 PR |
| `SCM` | `0xFF50` | Serial communication mode -- real hardware UART, see 12.4 |
| `SCC` | `0xFF52` | Serial communication control -- real hardware UART, see 12.4 |
| `BRG` | `0xFF53` | Baud rate generator -- real hardware UART, see 12.4 |
| `RXB` | `0xFF56` | Serial receive buffer (R) -- real hardware UART, see 12.4 |
| `TXB` | `0xFF57` | Serial transmit buffer (W) -- real hardware UART, see 12.4 |

Port-mode/control SFR addresses should be added here only after they are
cross-checked against the µPD78312 documentation and current assembler
symbols. The known **values** used by `1F0F` are already recorded in section 3.

---

Part of the [TK-x4x hardware reference](README.md) · previous: [Accessory connector, PTT and audio, squelch, model inputs](Accessory_Audio_and_Inputs.md) · next: [Replacement firmware: bring-up, open items, design rules](Replacement_Firmware_Guide.md)
