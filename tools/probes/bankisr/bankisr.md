# bankisr

Tests whether the uPD78312A's general-register banks (`SEL RBn`) can
replace the usual `PUSH {ax}` / `POP {ax}` save/restore around a real,
firing interrupt, as a cheaper way to protect a register the ISR needs
to scratch. Pure assembly (`bankisr.s`, next to this file; no C), built
from its own `build.sh` and `bankisr.ld` the way `uartprobe`/`panelprobe`
are, since there's no CALLT or vbcc78k3 step for a single-file image.

## Why this is worth testing

The stock mask ROM never once executes a `SEL` instruction (confirmed by
grepping the full disassembly listing) -- all 8 register banks exist in
silicon (a 128-byte window at `FE80h`-`FEFFh`, 16 bytes per bank,
switched by a 2-byte `SEL RBn`) but Kenwood's own firmware only ever uses
bank 0. That leaves banks 1-7 completely free for us to use, and `SEL
RBn` is itself only 2 bytes/cheap, same ballpark as a `PUSH`/`POP` pair,
so an ISR that needs `AX` as scratch could instead switch to an unused
bank, use `AX` there freely, and switch back -- never touching the
caller's `AX` at all, with no stack traffic.

This probe repoints the real DDS interrupt (TMF0, timer 0 -- see
`docs/hardware/Accessory_Audio_and_Inputs.md` section 13.3.2 for how that
tick was traced and corrected) at a handler that does exactly this:

```
isr_tmf0_bankswitch:
	sel	rb7
	addw	ax,#1
	movw	PWM0,ax
	inc	isr_fires
	clr1	sfr[TMIC0].7		; acknowledge TMF0
	sel	rb0
	reti
```

`sel rb7`/`sel rb0` stand in for where the real ISR's `push {ax}`/`pop
{ax}` would go. In between, `ax` is bank 7's own `ax` -- untouched by
whatever bank 0 was doing in `main_loop` at the moment of interrupt.
`movw PWM0,ax` is a stand-in for the real ISR's own `PWM0` write (same
instruction, same operand, confirmed byte-identical to stock firmware's
`8B94: 1A 14`), so this isn't just toggling a scratch register -- it's
doing an access shaped like the real workload while banked.

The question this probe answers isn't whether `SEL` works in isolation
(the instruction set already guarantees that) -- it's whether register
banking is safe and correct **across a real, asynchronous interrupt**
hitting **real silicon**, at the real ~10.87 kHz (92 us) rate. That's
not something a disassembly reading or a toolchain build can settle by
itself.

## What it does

- Arms Timer 0 exactly as the real DDS tick does: `MD0=0x005C` (92),
  `TM0=0`, `TMIC0=0x42` then unmasked, `TMC0=0x80` (interval timer,
  free-running, fCLK/6). Same values, same registers, as the real
  `sub_B05E`/`loc_B0CC` tone-generation setup.
- The main loop spends almost all its time in a tight canary check: load
  `AX` with a known pattern (`0xA55A`), spin through a few `NOP`s (long
  enough for an interrupt to land there essentially every pass, 65536
  passes total so a quiet period over many seconds also gets covered),
  then compare `AX` against the same pattern again. If an interrupt
  landing on a bank-0 `AX` ever let `isr_tmf0_bankswitch` clobber it --
  wrong bank math, a stale `SEL`, anything -- the canary would read back
  wrong and `canary_fails` increments.
- Every 65536 passes, prints `TICKS=XX FAILS=YY\r\n` on the bit-banged
  debug serial (mic jack PTT line, `P15`, 2400 8N2 -- same channel every
  other probe here uses). `TICKS` is `isr_fires` (so you can see the ISR
  is actually running, not just that nothing broke); `FAILS` is
  `canary_fails`.

## What to record

- **`FAILS` should stay `00` forever**, while `TICKS` climbs steadily
  (roughly 10870 per second, so a large number quickly -- that's the
  real ~10.87 kHz Timer 0 tick firing throughout). A climbing `FAILS` is
  the interesting negative result: it would mean register banking is
  *not* safe to rely on across this interrupt as written here, and the
  idea should stop at the brainstorm stage.
- Optional corroboration: scope `P34`/`PWM0`, physical **pin 39** on the
  64-pin package (see section 1.3.1 of the uPD78312A manual). It should
  show the ISR's `addw ax,#1` ramp -- a slow sawtooth/staircase on the
  pin at the ISR's own output rate, visible proof the handler is really
  firing and really writing `PWM0` from inside the banked section, not
  just incrementing a counter that happens not to collide.
- Let it run for at least a few minutes before calling `FAILS=00`
  conclusive -- a one-cycle race, if there is one, may not show up in
  the first few thousand interrupts.

## Safety

- Nothing here transmits, writes flash, or writes the EEPROM.
- `PWM0` is written continuously by the ISR (a free-running ramp); this
  is the same register the real DDS uses for QT/DQT/LTR tone output, so
  expect a changing tone/noise on the discriminator output path while
  this image runs. Not harmful, just audible/visible if monitored.
- Pure busy-loop plus one timer ISR -- no panel link, no PTT, no RF.

## Build

From this directory:

```sh
VASM=/path/to/vasm78k3_std VLINK=/path/to/vlink ./build.sh
```

Links via `bankisr.ld` (same memory map as `uartprobe.ld`/`panelprobe.ld`
-- every address in it is a real hardware constraint, not specific to
this image). Includes the mask ROM names and `uPD78312A.inc` from the
repository's `annotations/maskrom/`, plus `common/resident_api.s` and
`common/debug_serial_asm.s` for `_wdt_service`/`_dbg_init`/`dbg_mark`.
The image is exactly 26112 bytes (`8000h`-`E5FFh`) with the `KENWOOD`
signature at `E580h`; the built image is `bankisr.bin`, next to this
file.

Verified by direct build-and-disassemble with the project's own
toolchain: `isr_tmf0_bankswitch` assembles to `05 af 2d 01 00 1a 14 26
40 08 9f ce 05 a8 57` at address `82B3h` (matching the vector table's
`TMF0` entry), several bytes of which (`1a 14` for `movw PWM0,ax`, the
`08 9f xx` `clr1` idiom, `57` for `reti`) are byte-identical to the
corresponding instructions in the real stock firmware. That confirms
the encoding is exactly what was intended; it does not by itself confirm
the technique is safe on real silicon across a live interrupt -- that's
what this probe's bench run is for.
