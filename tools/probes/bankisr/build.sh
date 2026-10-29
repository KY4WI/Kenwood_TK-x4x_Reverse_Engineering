#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# Build the register-bank-ISR probe image: bankisr.s + resident_api.s +
# debug_serial_asm.s -> bankisr.bin (flash image, base 0x8000, required
# exact size 26112/0x6600 bytes). See bankisr.s's header for why this
# image exists. Pure assembly, no C -- this probe's subject is
# register-level behavior across a real interrupt, so there is no
# vbcc78k3 step at all, unlike uartprobe/panelprobe/baudsweep.
set -e
VASM=${VASM:-vasm78k3_std}
VLINK=${VLINK:-vlink}
DIR="$(cd "$(dirname "$0")" && pwd)"
COMMON="$(dirname "$DIR")/common"
INC="$(cd "$DIR/../../../annotations/maskrom" && pwd)"

SOURCES="
$DIR/bankisr.s
$COMMON/resident_api.s
$COMMON/debug_serial_asm.s
"

OBJS=""
for src in $SOURCES; do
	obj="$DIR/$(basename "$src" .s).o"
	"$VASM" -Fvobj -I"$COMMON" -I"$INC" -o "$obj" "$src"
	OBJS="$OBJS $obj"
done

# -fill=0xFF: write the gaps between sections as FFh, the value of erased
# flash, instead of vlink's default 00h.
"$VLINK" -T "$DIR/bankisr.ld" -b rawbin1 -fill=0xFF -o "$DIR/bankisr.bin" $OBJS
rm -f $OBJS

SIZE="$(stat -c%s "$DIR/bankisr.bin")"
echo "built $DIR/bankisr.bin ($SIZE bytes, base 0x8000)"

REQUIRED_SIZE=26112
if [ "$SIZE" -ne "$REQUIRED_SIZE" ]; then
	echo "ERROR: bankisr.bin is $SIZE bytes, expected exactly $REQUIRED_SIZE" >&2
	exit 1
fi
