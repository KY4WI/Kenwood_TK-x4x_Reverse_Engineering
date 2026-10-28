#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# Build the template probe image: ../panelprobe/crt0.s (borrowed
# unchanged) + ../panelprobe/panel.c (borrowed unchanged) +
# ../common/watchdog.c + template.c -> template.bin (flash image, base
# 0x8000, required exact size 26112/0x6600 bytes). See template.c's
# header for what this probe does and tools/probes/README.md for how to
# turn a copy of this directory into your own probe. Compiles every .c
# to .s itself first (vbcc78k3), same as every other probe's build.sh,
# so it's safe to run after editing any .c file without a separate
# manual compile step.
set -e
VBCC=${VBCC:-vbcc78k3}
VASM=${VASM:-vasm78k3_std}
VLINK=${VLINK:-vlink}
DIR="$(cd "$(dirname "$0")" && pwd)"
# panel.c/panel.h/hardware.h/crt0.s are borrowed from panelprobe/,
# unchanged, rather than copied -- a single proven source, not a second
# copy that could quietly drift out of sync with it. Other shared code
# (resident_api, watchdog) is in ../common; the mask ROM names and
# uPD78312A.inc come from the repository's annotations/maskrom/.
PANELPROBE="$(dirname "$DIR")/panelprobe"
COMMON="$(dirname "$DIR")/common"
INC="$(cd "$DIR/../../../annotations/maskrom" && pwd)"

CSOURCES="panel watchdog template"
# -O=287 (register alloc + optimize + cse/cp + const-prop + dead-
# assignment-elim + recalc_offsets) is REQUIRED, not optional -- see
# panelprobe.c's BETA_17 note. Without it, every image in this project
# that calls more than one or two functions from main() overflows the
# 30-byte stack budget below 0xFE00 before main() even reaches its own
# first statement.
for c in $CSOURCES; do
	case "$c" in
		panel) CFILE="$PANELPROBE/panel.c" ;;
		watchdog) CFILE="$COMMON/watchdog.c" ;;
		*) CFILE="$DIR/$c.c" ;;
	esac
	"$VBCC" -O=287 -c99 -I"$COMMON" -I"$PANELPROBE" -o="$DIR/$c.s" "$CFILE"
done

SOURCES="
$PANELPROBE/crt0.s
$COMMON/resident_api.s
$COMMON/debug_serial_asm.s
$DIR/panel.s
$DIR/watchdog.s
$DIR/template.s
"

OBJS=""
for src in $SOURCES; do
	obj="$DIR/$(basename "$src" .s).o"
	"$VASM" -Fvobj -I"$COMMON" -I"$INC" -o "$obj" "$src"
	OBJS="$OBJS $obj"
done

# -fill=0xFF: write the gaps between sections as FFh, the value of erased
# flash, instead of vlink's default 00h.
"$VLINK" -T "$DIR/template.ld" -b rawbin1 -fill=0xFF -o "$DIR/template.bin" $OBJS
rm -f $OBJS $(for c in $CSOURCES; do echo "$DIR/$c.s"; done)

SIZE="$(stat -c%s "$DIR/template.bin")"
echo "built $DIR/template.bin ($SIZE bytes, base 0x8000)"

REQUIRED_SIZE=26112
if [ "$SIZE" -ne "$REQUIRED_SIZE" ]; then
	echo "ERROR: template.bin is $SIZE bytes, expected exactly $REQUIRED_SIZE" >&2
	exit 1
fi
