#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# Build stage1.s -> stage1.bin (flash image, base 0x8000).
#
# Uses the same vasm -Fvobj + vlink pipeline as asmlink.sh, for the same
# reason (see machine.c's ".text vs .org ABSLABEL" finding):
# assembling straight to -Fbin silently zeroes every symbolic branch
# displacement, since a plain .text section's labels never get vasm's
# ABSLABEL flag. Going through a relocatable object + vlink resolves
# them correctly. -Ttext here is 0x8000 (not 0, like asmlink.sh's
# compiler-output helper) because this image is meant to be flashed
# starting at the real mask-ROM handoff address, not simulated from 0.
set -e
VASM=${VASM:-vasm78k3_std}
VLINK=${VLINK:-vlink}
DIR="$(cd "$(dirname "$0")" && pwd)"
"$VASM" -Fvobj -I"$(cd "$DIR/../../../annotations/maskrom" && pwd)" -o "$DIR/stage1.o" "$DIR/stage1.s"
# -fill=0xFF: write the gaps between sections as FFh, the value of erased
# flash, instead of vlink's default 00h.
"$VLINK" -Ttext 0x8000 -b rawbin1 -fill=0xFF -o "$DIR/stage1.bin" "$DIR/stage1.o"
rm -f "$DIR/stage1.o"

SIZE="$(stat -c%s "$DIR/stage1.bin")"
echo "built $DIR/stage1.bin ($SIZE bytes, base 0x8000)"

# Required output size: full 0x8000-0xE5FF application flash region.
REQUIRED_SIZE=26112
if [ "$SIZE" -ne "$REQUIRED_SIZE" ]; then
	echo "ERROR: stage1.bin is $SIZE bytes, expected exactly $REQUIRED_SIZE" >&2
	exit 1
fi
