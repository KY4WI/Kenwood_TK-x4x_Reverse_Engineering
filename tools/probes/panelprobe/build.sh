#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# Build the minimal panel-link probe image: crt0.s + resident_api.s +
# panel.c/watchdog.c -> panelprobe.bin (flash image, base 0x8000,
# required exact size 26112/0x6600 bytes). See crt0.s's header for why
# this image exists. BETA_16 added back a handful of debug_serial_asm.s
# breadcrumbs (NOT the fragile multi-marker crt0 instrumentation that
# caused 13 betas of confusion in selftest/phase1 -- just plain
# characters confirming how far this much-simpler image gets), since a
# blank screen with zero debug output is ambiguous between "never got
# here" and "ran fine, panel isn't answering". Compiles every .c to .s
# itself first (vbcc78k3), same as the other probe build scripts, so it's safe to
# run after editing any .c file without a separate manual compile step.
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

CSOURCES="panel watchdog panelprobe"
# BETA_17: -O=287 (1 register-alloc + 2 optimize + 4 cse/cp + 8 const-
# prop + 16 dead-assignment-elim + 256 recalc_offsets -- see opt.c's own
# comment above optimize() for this exact bit table) was added here
# after tracing BETA_16's "0000000" report to a real stack-frame
# overflow: no build.sh anywhere in this project had EVER passed an -O
# flag, so every function was compiled with register allocation OFF and
# every intermediate expression value permanently and separately spilled
# to the stack with no slot reuse -- confirmed by disassembly (main()'s
# own frame was 32 bytes for a function with one real local variable).
# SP starts at 0xFE20 (crt0.s) with only 0x1E (30) bytes before it hits
# 0xFE00, below which is NOT spare RAM but the external codeplug/
# configuration flash (hardware.h) -- main() alone blew that budget
# before reaching its own first statement. -O=287 cut main()'s frame
# from 32 bytes to 5 (measured) by finally letting the compiler keep
# short-lived values in registers and reclaim dead stack slots instead
# of giving every one its own permanent byte. Verified against
# emulator/tests/run_tests.sh (all 7 pass with this flag) before being
# applied to real firmware; deliberately NOT the more aggressive 32
# (multi-pass global opt), 64 (block-wise reg alloc under vbcc), or 2048
# (loop unrolling) bits -- those touch more of the optimizer this
# backend has never been exercised against, for headroom this firmware
# doesn't need.
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
$DIR/panel.s
$DIR/watchdog.s
$DIR/panelprobe.s
"

OBJS=""
for src in $SOURCES; do
	obj="$DIR/$(basename "$src" .s).o"
	"$VASM" -Fvobj -I"$COMMON" -I"$INC" -o "$obj" "$src"
	OBJS="$OBJS $obj"
done

# -fill=0xFF: write the gaps between sections as FFh, the value of erased
# flash, instead of vlink's default 00h.
"$VLINK" -T "$DIR/panelprobe.ld" -b rawbin1 -fill=0xFF -o "$DIR/panelprobe.bin" $OBJS
rm -f $OBJS $(for c in $CSOURCES; do echo "$DIR/$c.s"; done)

SIZE="$(stat -c%s "$DIR/panelprobe.bin")"
echo "built $DIR/panelprobe.bin ($SIZE bytes, base 0x8000)"

REQUIRED_SIZE=26112
if [ "$SIZE" -ne "$REQUIRED_SIZE" ]; then
	echo "ERROR: panelprobe.bin is $SIZE bytes, expected exactly $REQUIRED_SIZE" >&2
	exit 1
fi
