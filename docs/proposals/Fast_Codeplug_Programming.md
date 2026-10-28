# Fast Codeplug Programming Idea

This note is only a memory dump for a possible future feature. It records the
ideas and reasoning discussed so they are not lost. Nothing here is currently
being implemented, and the details may change substantially if the idea is
developed later.

## Basic idea

A future custom PROGRAM monitor could remain compatible with the normal
Kenwood programming protocol while optionally supporting a faster serial rate.

The intent would be to keep PROGRAM mode as close to stock behavior as
possible. The only meaningful additions being considered are:

- a firmware capability flag indicating that faster programming is supported;
- a private command to negotiate a faster serial rate;
- a checksum/CRC on programming transfers.

A stock radio would remain completely unaffected. Host software would first
read the firmware feature flags using the normal programming protocol. It would
only send the private speed-change command when the firmware explicitly says
that the extension is supported.

This is a natural use for one of the reserved firmware feature bits.

## Compatibility concept

The normal entry sequence would remain stock-compatible. The radio and host
would initially communicate at the normal programming speed.

If the fast-programming capability bit is not set, everything would continue
exactly as it does today.

If the capability bit is set, the host could request a higher rate, such as
19200 or 38400 baud. The change would be negotiated while both sides are still
at the normal rate.

After the radio acknowledges the requested speed, both ends would switch and
confirm communication at the new rate.

If the confirmation fails, the radio should automatically fall back to the
normal programming rate after a short timeout. The host would do the same.
This prevents either end from becoming stranded at the wrong baud rate.

The exact private command and negotiation details are intentionally left open.

## PROGRAM monitor approach

The eventual implementation could use the stock mask-ROM PROGRAM monitor as
the behavioral reference and reproduce the parts that are needed in
application firmware, with only the desired extensions added.

The goal is not to create a new general-purpose loader. It is to preserve the
existing PROGRAM behavior as much as possible and add faster communication and
transfer integrity.

The enhanced monitor should not gain unrelated features. Firmware flashing,
radio operation, calibration editing, and other service functions belong
elsewhere.

## RAM considerations

RAM is limited, but PROGRAM mode has complete control of the radio and ends in
a reboot. Normal application state therefore does not need to be preserved.

The currently understood RAM layout gives a substantial amount of scratch RAM
that can be reused during PROGRAM mode. This appears sufficient for the same
128-byte block size used by the stock programming protocol, along with the
small amount of additional state needed for checksum/CRC handling and baud
negotiation.

Because the radio reboots when programming is finished, there is no reason to
preserve ordinary application working RAM during this temporary mode.

## Faster baud rate

The main MCU uses a 12 MHz crystal, but the fastest CPU clock is the crystal
divided by two, so the CPU itself runs at no more than 6 MHz.

The existing software-serial timing suggests that 38400 baud may be practical.
The timer division works out cleanly enough that baud-rate accuracy itself does
not appear to be the main concern.

The more important question is whether the software receive/transmit paths can
comfortably service the shorter bit period at a 6 MHz CPU clock. That would
need to be proven on hardware before relying on 38400.

Supporting 19200 as an intermediate fallback also seems sensible.

## Checksum / CRC idea

The faster protocol should add a checksum or CRC to each data-bearing transfer.

The preferred idea is CRC-16/CCITT-FALSE.

The integrity check should cover the entire meaningful transaction, not just
the 128-byte data payload. In particular, it should also protect:

- the command byte;
- the address bytes;
- the transfer length;
- the data payload.

This is important because corrupted address bytes could otherwise direct valid
data to the wrong location without being detected.

The existing 128-byte transfer could simply gain two CRC bytes at the end,
making 130 bytes of data plus CRC for the transfer body.

One attractive property of CRC-16 is that the transmitted CRC can be appended
so that processing the complete protected frame, including the CRC bytes,
produces the expected zero residue. That makes receiver validation simple.

No write should occur until the complete transfer has been received and its
CRC has validated.

The same protection should be used for read data so the host can detect
corruption in either direction.

## Keep stock behavior otherwise

The central idea is to avoid changing PROGRAM mode any more than necessary.

The future enhanced monitor should try to preserve:

- the normal PROGRAM entry behavior;
- the normal identification and password behavior;
- the existing read and write concepts;
- the existing 128-byte block size;
- the normal codeplug/configuration write restrictions;
- the normal exit/reset behavior.

The feature should amount to:

**stock-style PROGRAM mode, optionally faster, with CRC protection.**

## Open questions for later

If this idea is pursued, the main things to investigate are:

- how much code space the replacement PROGRAM monitor requires;
- where it can live safely in the application firmware;
- whether 38400 baud is fully reliable at the 6 MHz CPU limit;
- which reserved firmware feature bit should permanently represent the feature;
- the exact private baud-negotiation command;
- whether 19200 and 38400 should both be advertised;
- the precise CRC byte order and frame boundaries;
- whether any stock PROGRAM behavior has hidden side effects that must be
  duplicated exactly.

Those questions do not need to be resolved now. The purpose of this note is
only to preserve the concept and the reasoning behind it for future work.
