<!-- I/O map and the shared serial bus -->
Part of the [TK-x4x hardware reference](README.md) · previous: [CPU, memory map and boot](CPU_Memory_and_Boot.md) · next: [PLL and frequency generation](PLL.md)

# 4. Complete main-CPU I/O map

The table below is the current programming-oriented map. Direction is the
functional direction in normal stock operation; some pins are mode-switched.

| CPU pin | Dir. | Board/net role | Firmware significance | Evidence |
|---|---|---|---|---|
| `P00` | Out | `EP` | MC145190 PLL ENABLE/latch | `[SM][FW]` |
| `P01` | Out | `CK` | Shared clock: PLL, AT24C02, 4094, M62363 DAC | `[SM][MASK][FW]` |
| `P02` | Out | `DT` | Shared serial data: PLL, 4094, M62363 DAC | `[SM][MASK][FW]` |
| `P03` | Out | `ES` | BU4094/XRU4094 strobe/enable | `[SM][MASK]` |
| `P04` | Out | `MTX` | Main CPU -> front-panel CPU serial | `[SM][MASK]` |
| `P05` | In | `DBD` | Option-signaling Deadbeat Disable | `[SM]` |
| `P06` | In | `PT` | Option/transpond PTT request | `[SM]` |
| `P07` | Out | `EN` | M62363 DAC enable/latch | `[SM][MASK]` |
| `P10` | In | `PTT` | External/accessory PTT input | `[SM]` |
| `P11` | In | `HK` | External hook input | `[SM]` |
| `P12` | Out | `RST` | **Option-signaling reset**, not CPU RESET | `[SM]` |
| `P13` | - | tied to +V | Unused by stock design | `[SM][SCH]` |
| `P14` | Out | wired to `P22`: the mic **hanger** line (jack pin 13) | Drives the hanger line: CLONE mode's data output | `[SCH][MASK]` |
| `P15` | I/O | wired to `P23`: the mic **PTT** line (jack pin 14) | Drives the PTT line: the mic-port serial output (programming, tuning). As an input, it is how the stock firmware reads mic PTT | `[SCH][MASK][FW]` |
| `P16` | In | `DTC` | Data-group / MDT control input | `[SM]` |
| `P17` | Out | `KEY` | **RF transmitter key, active high** | `[SM][FW]` |
| `P20/NMI` | - | GND | NMI held inactive | `[SM][SCH]` |
| `P21/INTE0` | In | `MRX` | Front-panel CPU -> main serial receive | `[SM][MASK]` |
| `P22/INTE1` | In | `HK`: mic hanger (jack pin 13) | Hanger switch; also the mic-port serial input | `[SCH][MASK]` |
| `P23/INTE2` | In | `PTT`: mic PTT (jack pin 14) | Mic PTT; CLONE mode's data input. The stock application never reads it (it reads `P15`) | `[SCH][MASK][FW]` |
| `P24/TXD` | Out | `TXD` | External asynchronous serial TX -- real on-chip UART, see 12.4 | `[SM][DS][MASK][FW]` |
| `P25/RXD` | In | `RXD` | External asynchronous serial RX -- real on-chip UART, see 12.4 | `[SM][DS][MASK][FW]` |
| `P26/SCK` | Out | mic-mute path (perhaps `MM`) | Microphone mute control | `[SM][SCH]` |
| `P27/CTS` | - | GND | Grounded | `[SM][SCH]` |
| `P30/CI0` | In | `SQ` detector | Receiver noise-pulse input for carrier/squelch decision | `[SM][SCH]` |
| `P31/CTRL0` | - | tied to +V | Unused by stock design | `[SM][SCH]` |
| `P32/CI1` | In | `LD` | PLL lock/unlock indication | `[SM][FW]` |
| `P33/CTRL1` | In | `MT` | Option-signaling decode-latch input | `[SM]` |
| `P34/PWM0` | Out | `DQT/QT` | Low-speed signaling waveform: CTCSS/DCS/etc. | `[SM][FW]` |
| `P35/PWM1` | ? | `DATA2` | Alternate function enabled at boot (`PMC3.5=1`, part of a bulk 6-pin config write in `1F0F`'s baseline); no code anywhere references `PWM1` (`0xFF17`) or bit-tests `0xFF03.5` directly, so what actually drives this pin post-boot is still unproven | `[MASK][SCH][OPEN]` |
| `P36/CLR0/TO0` | I/O | `SDA` | AT24C02 bidirectional data | `[MASK][SCH]` |
| `P37/CLR1/TO1` | Out | `WRPRO` | Flash write-enable/protect gating | `[MASK][SCH]` |
| `P40-P47` | Bus | `AD0-AD7` | Multiplexed low address/data bus | `[SM][SCH]` |
| `P50-P56` | Bus | `A8-A14` | Upper AT29C256 address bits | `[SM][SCH]` |
| `P57/A15` | Bus | `A15` | External region/chip-select logic, **not flash A15** | `[SCH][RE]` |
| `/ALE` | Out | ALE | Latch low address A0-A7 | `[SM][SCH]` |
| `/RD` | Out | read strobe | External memory read / flash OE path | `[SM][SCH]` |
| `/WR` | Out | write strobe | External memory write path | `[SM][SCH]` |
| `/EA` | In | external-access config | CPU external-memory configuration | `[SM]` |
| dedicated `RESET` | In | CPU reset | Real CPU reset pin; **not P12** | `[SM]` |

### 4.1 The mic jack's two lines (P14/P22 and P15/P23)

Per the schematic (Dave, 2026-10-16) the mic jack's PTT and hanger lines
each go to two CPU pins, an input and an output on the same net: `[SCH]`

| Jack pin | Line | Input pin | Output pin |
|---:|---|---|---|
| 14 | mic PTT | `P23/INTE2` | `P15` |
| 13 | mic hanger (hook) | `P22/INTE1` | `P14` |

So the same two wires carry everything the mic jack does:

- **Normal use:** PTT and hanger are inputs. The stock application reads
  PTT through `P15` (38 tests, never `P23`) together with the accessory PTT
  on `P10`, and the hanger on `P22`. `[FW]`
- **Programming and tuning** (the mic-port serial port): the radio sends on
  `P15`, i.e. **on the PTT line**, and receives on `P22`, the hanger line.
  `[MASK][FW]`
- **CLONE mode:** the other way round: it sends on `P14` (hanger line) and
  receives on `P23` (PTT line), so two radios cross over. `[MASK]`

Consequences for replacement firmware:

- While the radio sends serial data, every 0 bit pulls the PTT line low:
  anything watching PTT then sees it pressed. Never key on mic PTT while
  sending on the mic port. `[RE]`
- With a microphone plugged in, its PTT switch can pull the line low
  against our output, and its hanger switch makes edges on the serial
  input. Mic-port serial is for a cable in the jack, not alongside a mic. `[RE]`
- In normal running the stock code leaves `P15` as an input (released) and
  only sometimes drives it low itself (`E142h`, `E169h`, with `FED8.6` set; released
  again at `E227h`), under conditions involving `P10`, `P06` and codeplug
  `E66Dh` bit 1. What that low means is `[OPEN]`. `[FW]`

---

# 5. Shared serial hardware bus

The board deliberately reuses P01/P02 for several peripherals:

```text
              P01 / CK
                 |
      +----------+-----------+-----------+
      |          |           |           |
     PLL       4094        M62363      AT24C02 SCL

              P02 / DT
                 |
      +----------+-----------+
      |          |           |
     PLL       4094        M62363

AT24C02 data is separate: P36 / SDA
```

Dedicated selects/latches keep transactions separated:

```text
P00   PLL enable/latch
P03   4094 strobe
P07   DAC enable/latch
P36   EEPROM SDA
```

**Important:** careless startup activity on P01/P02 can shift garbage into the
PLL, output expander, or DAC. Keep selects/latches in their safe inactive state
while changing clock/data direction or level. `[SM][MASK][RE]`

For timing-critical bit-banging, hand-written assembly is preferred over the
current generic C SFR lowering.

---

Part of the [TK-x4x hardware reference](README.md) · previous: [CPU, memory map and boot](CPU_Memory_and_Boot.md) · next: [PLL and frequency generation](PLL.md)
