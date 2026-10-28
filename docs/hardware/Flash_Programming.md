<!-- External flash programming -->
Part of the [TK-x4x hardware reference](README.md) · previous: [D/A converter, output expander and calibration EEPROM](DAC_Expander_EEPROM.md) · next: [Front panel CPU, display, buttons and beeps](Front_Panel.md)

# 10. External flash programming

## 10.1 P37 / WRPRO behavior

P37 is functionally confirmed by the resident flash code. Flash writers do:

```text
P3.7 = 0
issue/program AT29C256 sequence
write page
P3.7 = 1
wait for completion
```

`[MASK][RE]`

This is board-level write-enable/protect control. Even a correctly issued
flash erase/program command sequence cannot alter Flash ROM unless this gate is
enabled. The AT29C256 does not expose a simple conventional WP input matching
this signal, so replacement firmware should treat P37 as part of the board's
programming gate rather than as a normal flash address/data pin. Resident
flash-writing routines explicitly control the gate; it is protection against
stray commands, not an absolute barrier to custom code that invokes a writer.
For the distinct conditions required to make loader recovery inaccessible,
see [Hard-Brick Recovery](Hard_Brick_Recovery.md).

## 10.2 Resident flash API

```text
1F36   fixed 64-byte page write (FE40 -> E680)
1F3C   disable AT29C256 software data protection
1F3F   protected page-program command prefix
1F48   generic 64-byte page writer
```

The generic writer is the important service for application-managed flash
updates. It can write executable flash as well as codeplug flash; the caller is
responsible for destination safety. `[MASK][RE]`

## 10.3 AT29C256 protected-program command mapping

Because the chip is CPU-mapped at 8000:

```text
chip 5555 -> CPU D555
chip 2AAA -> CPU AAAA
```

Resident `1F3F` emits:

```text
AA @ D555
55 @ AAAA
A0 @ D555
```

then the caller loads the 64-byte page. `[MASK][DS]`

**Rule:** prefer the resident flash writer instead of executing home-grown flash
programming code from the same external flash device being modified.

## 10.4 Normal PROGRAM mode versus firmware-loader mode

These are two different programming paths and should not be conflated.

### Normal PROGRAM mode

Resident entry `1F00` is Kenwood's standalone KPG PROGRAM monitor. It is a
top-level mode, not a normal subroutine, and does not return to the caller.
`[MASK][RE]`

This is the mode used for normal codeplug/service communication. It requires
the external application firmware to be working well enough to enter and
service the programming interface.

The normal programming path can read throughout the CPU address space,
including the internal mask ROM and external flash. Its ordinary `W` command,
however, protects the executable region by rejecting writes below `0xE600`.
Normal writes are therefore restricted to the codeplug/configuration region.

### Firmware-loader mode

Firmware-loader mode is a separate resident mask-ROM path used to program the
external application firmware region at `0x8000-0xE5FF`.

It does **not** provide the normal memory-read interface. Its job is firmware
programming.

Because this loader is in the CPU's internal mask ROM, reflashing the external
AT29C256 cannot erase it. Firmware loader access is controlled by bit `0x04`
at channel/configuration address `0xE66C`.

This architecture makes the radio unusually hard to brick in software. A bad
external firmware image does not remove the loader. To disable loader access,
the running application has to be functional enough to rewrite the
channel/configuration bit controlling it, so accidental self-bricking during
ordinary firmware development is unlikely.

Keep the capabilities separate:

```text
normal PROGRAM mode:
    requires working application firmware
    reads memory throughout the address space
    ordinary writes limited to E600-FDFF

firmware-loader mode:
    resident in internal mask ROM
    cannot be erased by external firmware writes
    writes application firmware at 8000-E5FF
    does not provide general memory reads
```

### 10.4.1 Hard-brick recovery

The mask-ROM loader and application firmware form a one-way programming chain:

```text
mask ROM loader -> programs application firmware
application firmware -> programs channel/configuration settings
```

The loader cannot change the channel/configuration bit that enables access to itself, and normal PROGRAM mode cannot rewrite the application firmware. A true hard brick therefore requires unusable application firmware and disabled firmware loader access at the same time. Normal programming cannot create that combination.

The detailed forced-loader mechanism, schematics, switch sequence, and recovery procedure are documented in [TK-x4x Hard-Brick Recovery](Hard_Brick_Recovery.md).

## 10.5 Resident CLONE mode

Entry `1F39` is a complete non-returning CLONE-mode application in mask ROM.
It initializes the machine, displays `" CLONE  "`, and transfers the normal
`0xE600-0xFDFF` codeplug region. `[MASK][RE]`

It is useful as evidence for hardware initialization and panel behavior, but it
should be entered as a standalone mode, not called as a normal function that is
expected to return.

Its start, `118Dh`, repeats the whole reset sequence (`DI`, watchdog, stack,
`009Eh`, `1F0F`, `1F0C`, `1F12`, `1F27`, `1F15`, `EXIC0` unmasked, `EI`),
with one extra write, `PM1 = 13h`, before `1F0C`. Then: `8Eh` alone, the
screen (`02h` + `" CLONE  "` through `panel_text8`, `15C8h`), `1F4B` (model
field), `FED7.5` cleared, `PM1 = 2Bh`, `EXIC1` bits 7 and 6 set, `EXIC2`
bits 7 and 6 cleared, `FEDE.6` set. That last block runs **after** the panel
sends, so it is CLONE's own set-up for the transfer (port directions and the
external-interrupt controls of the clone data lines), not something the
panel link needs. `[MASK]`

---

Part of the [TK-x4x hardware reference](README.md) · previous: [D/A converter, output expander and calibration EEPROM](DAC_Expander_EEPROM.md) · next: [Front panel CPU, display, buttons and beeps](Front_Panel.md)
