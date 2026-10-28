# Kenwood TK-x4x tuning probe (experimental)

Standalone read-only test utility for Kenwood TK-840 / TK-940 / TK-941 tuning data. Not part of TKx4xToolkit. Based on reverse engineering of the KPG-25D DOS host and TK-840 8355 / TK-941 AB00 application firmware.

## Build (Linux / POSIX)

```sh
cc -std=c99 -O2 -Wall -Wextra -o kpg25_tuning_probe kpg25_tuning_probe.c
```

## Use

Connect the same USB-TTL programming cable used for TKx4xToolkit. Start the radio normally, not in firmware PROGRAM/recovery mode. On Linux:

```sh
./kpg25_tuning_probe /dev/ttyUSB0 --output tune_readback.txt
```

Optionally, **TK-940/941 10-byte response only**:

```sh
./kpg25_tuning_probe /dev/ttyUSB0 --read-e0 --output tune_extra.txt
```

The `--read-e0` option adds tuning command `0x2C` to read EEPROM bytes E9 through E0. This is *not* an entire AT24C02 dump. Output files are never overwritten. A successful 17-byte reply is expected for TK-840 firmware 8355; 10 bytes for TK-940/941 AB00; a 16-byte TK-840 variant is also supported as a raw readback. The utility reports the original raw byte positions, and known fields are decoded with stated revision qualifications.

The utility uses the stock DOS host's ASCII `TUNING ` (trailing space!) entry at 1200 baud, then 9600 baud, sending only the `0x05` read command, optional `0x2C` read command, `0x06` ACKs, and `0x45` tuning exit. It includes **no radio write, EEPROM restore, EEPROM erase, or RF transmit commands**. It was compiled with `-Wall -Wextra -Werror -pedantic` and tested against simulated serial replies of 10, 16 and 17 bytes plus the optional 0x2C path. It has **not** been bench-tested on an actual radio.

**Caution:** This is reverse-engineered service-mode access. Do not use it during transmission or on a radio with unknown firmware behavior. If the radio remains in TUNING mode after an error, power-cycle it normally. Save the raw results before experimenting with any calibration writes elsewhere. The data here is *not* yet a complete, verified calibration restore backup.