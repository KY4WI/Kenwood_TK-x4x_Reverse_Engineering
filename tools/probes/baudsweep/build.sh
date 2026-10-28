#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# Build the hardware-UART bit-rate sweep image: crt0.s + resident_api.s +
# debug_serial_asm.s + uart.c/watchdog.c/baudsweep.c -> baudsweep.bin (flash
# image, base 0x8000, required exact size 26112/0x6600 bytes). See
# crt0.s's header for why this image exists. Compiles every .c to .s
# itself first (vbcc78k3), same as every other image's build.sh, so
# it's safe to run after editing any .c file without a separate manual
# compile step.
set -e
VBCC=${VBCC:-vbcc78k3}
VASM=${VASM:-vasm78k3_std}
VLINK=${VLINK:-vlink}
DIR="$(cd "$(dirname "$0")" && pwd)"
# Shared code (resident_api, debug_serial) is in ../common; the mask ROM
# names and uPD78312A.inc come from the repository's
# annotations/maskrom/.
COMMON="$(dirname "$DIR")/common"
INC="$(cd "$DIR/../../../annotations/maskrom" && pwd)"

CSOURCES="uart watchdog baudsweep"
# -O=287 (register alloc + optimize + cse/cp + const-prop + dead-
# assignment-elim + recalc_offsets) -- same flag every other image's
# build.sh uses since panelprobe BETA_17's stack-overflow fix; see that
# image's build.sh for the full derivation. Required here too: SP starts
# at 0xFE20 with only 30 bytes before the external codeplug flash at
# 0xFE00, same budget as every other image.
for c in $CSOURCES; do
	case "$c" in
		watchdog) CFILE="$COMMON/watchdog.c" ;;
		*) CFILE="$DIR/$c.c" ;;
	esac
	"$VBCC" -O=287 -c99 -I"$COMMON" -o="$DIR/$c.s" "$CFILE"
done

SOURCES="
$DIR/crt0.s
$COMMON/resident_api.s
$COMMON/debug_serial_asm.s
$DIR/uart.s
$DIR/watchdog.s
$DIR/baudsweep.s
"

OBJS=""
for src in $SOURCES; do
	obj="$DIR/$(basename "$src" .s).o"
	"$VASM" -Fvobj -I"$COMMON" -I"$INC" -o "$obj" "$src"
	OBJS="$OBJS $obj"
done

# -fill=0xFF: write the gaps between sections as FFh, the value of erased
# flash, instead of vlink's default 00h.
"$VLINK" -T "$DIR/baudsweep.ld" -b rawbin1 -fill=0xFF -o "$DIR/baudsweep.bin" $OBJS
rm -f $OBJS $(for c in $CSOURCES; do echo "$DIR/$c.s"; done)

SIZE="$(stat -c%s "$DIR/baudsweep.bin")"
echo "built $DIR/baudsweep.bin ($SIZE bytes, base 0x8000)"

REQUIRED_SIZE=26112
if [ "$SIZE" -ne "$REQUIRED_SIZE" ]; then
	echo "ERROR: baudsweep.bin is $SIZE bytes, expected exactly $REQUIRED_SIZE" >&2
	exit 1
fi
