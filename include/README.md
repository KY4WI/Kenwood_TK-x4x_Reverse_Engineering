# Names for hardware values

Once a value is understood, it gets a name here, so code says
`PANEL_BEEP_STOP` instead of `0xB4` and nobody has to remember the hex.

| File | What it names |
|---|---|
| [`panel_codes.h`](panel_codes.h) | the front-panel link: commands sent to the panel, icons, beep numbers, key codes and session requests coming back |

Each `.h` is the master. Its assembler version (`panel_codes.inc`, with
`.equ` lines) is made from it by [`make_inc.sh`](make_inc.sh): edit the
`.h`, run `./make_inc.sh`, and commit both. CI runs
`make_inc.sh --check` and fails if an `.inc` is out of date.

```c
#include "panel_codes.h"          /* C: add -I.../include */
panel_send_byte(PANEL_BEEP);
panel_send_byte(BEEP_KEY);
```

```asm
	.include "panel_codes.inc"	; assembly: add -I.../include
	mov	a, #PANEL_BEEP_STOP
	call	!trampoline_panel_tx_queue
```

## Rules for adding a name

- Name a value only when we know what it does: bench-tested, or used by
  the stock firmware with a clear meaning. Bytes still `[OPEN]` stay hex.
- Say which direction: the same byte can mean different things going to
  the panel and coming from it (`0x90` is "TA icon on" one way and
  "Volume Up pressed" the other). Prefixes: `PANEL_` (to the panel),
  `PANEL_KEY_` and `PANEL_REQ_` (from the panel), `BEEP_` (the number
  after `PANEL_BEEP`).
- One comment line per name: what it does, and anything that bites
  (`PANEL_BEEP` alone locks up the panel).
- Put the name next to the hex in the documents' tables too, so the two
  can be matched up from either side.
- The names are ours, not Kenwood's; rename freely while nothing much
  uses them yet.
