# Planned radio diagnostics

This note collects **unimplemented diagnostic proposals**. Actual
experimental images and their measured results are in
[tools/probes/](../../tools/probes/README.md); tested components can
subsequently move into the Custom Firmware repository.

The project has **already built** panel and key-observation probes
(`panelprobe`, `benchtest`), hardware-UART experiments (`uartprobe`,
`baudsweep`), and a preliminary model/calibration/RX PLL bring-up
(`phase1`). They are not all finished or fully verified; see their
individual notes before relying on a result.

One remaining idea is a **PLL lock-limit sweep** that steps across the
radio's receive range and records transitions between locked and
unlocked, giving real per-radio limits. A corresponding transmit-side
sweep could be conducted only into a proper dummy load using suitable
RF test equipment. Neither sweep is an implemented feature here.

Other open work includes systematic programming-protocol diagnostics
and integrating validated test routines into replacement firmware.
