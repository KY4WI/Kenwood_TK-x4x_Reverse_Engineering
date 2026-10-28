#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# Build a single-file probe in this directory: NAME.s, NAME.S or NAME.asm
# -> NAME.bin (application flash 8000H-E5FFH, 26112 bytes).
#   VASM=/path/to/vasm78k3_std ./build.sh benchtest
# Probes with more than a file or two have their own subdirectory and
# their own build.sh (panelprobe/, uartprobe/, baudsweep/).
set -e
VASM=${VASM:-vasm78k3_std}
DIR=$(cd "$(dirname "$0")" && pwd)
INC=$(cd "$DIR/../../annotations/maskrom" && pwd)	# mask ROM names, uPD78312A.inc
NAMES=$(cd "$DIR/../../include" && pwd)				# panel_codes.inc and other names
[ $# -ge 1 ] || { echo "usage: $0 NAME [NAME ...]" >&2; exit 2; }
for NAME in "$@"; do
	NAME=${NAME%.*}
	SRC=""
	for ext in s S asm; do
		[ -f "$DIR/$NAME.$ext" ] && { SRC="$DIR/$NAME.$ext"; break; }
	done
	if [ -z "$SRC" ]; then
		if [ -f "$DIR/$NAME.c" ]; then
			echo "$NAME.c: single-file C probes need a crt0 and linker script; give it a subdirectory with its own build.sh, like panelprobe/" >&2
		else
			echo "no $NAME.s, $NAME.S or $NAME.asm in $DIR" >&2
		fi
		exit 1
	fi
	"$VASM" -Fbin -quiet -I"$INC" -I"$NAMES" -I"$DIR/common" -L "$DIR/$NAME.lst" \
		-o "$DIR/$NAME.bin" "$SRC"
	SIZE=$(wc -c < "$DIR/$NAME.bin")
	if [ "$SIZE" -ne 26112 ]; then
		echo "$NAME.bin is $SIZE bytes, expected 26112" >&2
		exit 1
	fi
	SUM=$(od -An -v -tu1 "$DIR/$NAME.bin" | awk '{for(i=1;i<=NF;i++)s+=$i} END{printf "%04X", s%65536}')
	echo "built $NAME.bin: 26112 bytes, 16-bit byte sum $SUM"
done
