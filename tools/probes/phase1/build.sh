#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# Build phase1 (an experimental replacement-firmware probe used on the
# bench; the current PLL-lock result remains unverified): compile with vbcc78k3, then crt0.s +
# resident_api.s + the compiled modules -> phase1.bin (flash image, base
# 0x8000, required exact size 26112/0x6600 bytes). Same vasm -Fvobj +
# vlink pipeline as stage1's build.sh, using phase1.ld instead of a plain
# -Ttext base (see that script's comment for why -Fvobj, not -Fbin).
set -e
VBCC=${VBCC:-vbcc78k3}
VASM=${VASM:-vasm78k3_std}
VLINK=${VLINK:-vlink}
DIR="$(cd "$(dirname "$0")" && pwd)"
COMMON="$(dirname "$DIR")/common"
INC="$(cd "$DIR/../../../annotations/maskrom" && pwd)"
NAMES="$(cd "$DIR/../../../include" && pwd)"

# buildinfo.c: which vbcc/vasm/vlink built this image (printed at boot).
sh "$COMMON/buildinfo.sh" "$VBCC" "$VASM" "$VLINK" "$DIR/buildinfo.c"

CSOURCES="model eeprom calibration panel pll outexp ptt rf ui watchdog debug_serial phase1 buildinfo"
# -O=287: see panelprobe/build.sh for why.
for c in $CSOURCES; do
	case "$c" in
		watchdog) CFILE="$COMMON/watchdog.c" ;;
		*) CFILE="$DIR/$c.c" ;;
	esac
	"$VBCC" -O=287 -c99 -I"$DIR" -I"$COMMON" -I"$NAMES" -o="$DIR/$c.s" "$CFILE"
done
# Debug output on the hardware UART (CN3, 115200 bps 8N1), shared from common/.
for c in uart debug_uart; do
	"$VBCC" -O=287 -c99 -I"$DIR" -I"$COMMON" -I"$NAMES" -o="$DIR/$c.s" "$COMMON/$c.c"
done

SOURCES="
$DIR/crt0.s
$COMMON/resident_api.s
$DIR/uart.s
$DIR/debug_uart.s
$DIR/model.s
$DIR/eeprom.s
$DIR/calibration.s
$DIR/panel.s
$DIR/pll.s
$DIR/outexp.s
$DIR/ptt.s
$DIR/rf.s
$DIR/ui.s
$DIR/watchdog.s
$DIR/debug_serial.s
$DIR/phase1.s
$DIR/buildinfo.s
"

OBJS=""
for src in $SOURCES; do
	obj="$DIR/$(basename "$src" .s).o"
	"$VASM" -Fvobj -I"$COMMON" -I"$INC" -I"$NAMES" -o "$obj" "$src"
	OBJS="$OBJS $obj"
done

# -fill=0xFF: write the gaps between sections as FFh, the value of erased
# flash, instead of vlink's default 00h.
"$VLINK" -T "$DIR/phase1.ld" -b rawbin1 -fill=0xFF -o "$DIR/phase1.bin" $OBJS
rm -f $OBJS "$DIR/buildinfo.c" $(for c in $CSOURCES uart debug_uart; do echo "$DIR/$c.s"; done)

SIZE="$(stat -c%s "$DIR/phase1.bin")"
echo "built $DIR/phase1.bin ($SIZE bytes, base 0x8000)"

REQUIRED_SIZE=26112
if [ "$SIZE" -ne "$REQUIRED_SIZE" ]; then
	echo "ERROR: phase1.bin is $SIZE bytes, expected exactly $REQUIRED_SIZE" >&2
	exit 1
fi
