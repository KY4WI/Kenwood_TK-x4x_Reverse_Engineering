#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# Make the assembler versions of the C headers here:
#   NAME.h -> NAME.inc   (#define NAME VALUE /* note */ -> .equ NAME, VALUE ; note)
#   ./make_inc.sh          write every .inc
#   ./make_inc.sh --check  exit 1 if any .inc is out of date (used by CI)
# Edit the .h, never the .inc.
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
CHECK=0; [ "$1" = "--check" ] && CHECK=1
STATUS=0
for H in "$DIR"/*.h; do
	INC="${H%.h}.inc"
	TMP="$INC.tmp"
	{
		echo "; $(basename "$INC") -- made from $(basename "$H") by make_inc.sh; do not edit."
		echo "; See $(basename "$H") for what these are and where the evidence is."
		echo "; SPDX-License-Identifier: GPL-3.0-only"
		awk '
		/^#define[ \t]+[A-Z_][A-Z0-9_]*_H[ \t]*$/ { next }	# include guard
		/^#define[ \t]/ {
			name = $2; value = $3; note = ""
			if (match($0, /\/\*.*\*\//)) {
				note = substr($0, RSTART + 2, RLENGTH - 4)
				gsub(/^[ \t]+|[ \t]+$/, "", note)
			}
			line = sprintf("\t.equ\t%s, %s", name, value)
			if (note != "") {
				pad = 40 - (length(name) + length(value) + 10)
				if (pad < 1) pad = 1
				line = line sprintf("%" pad "s; %s", "", note)
			}
			print line; next
		}
		/^\/\* ----.*---- \*\/$/ {
			s = $0; sub(/^\/\* /, "", s); sub(/ \*\/$/, "", s)
			print ""; print "; " s; next
		}
		' "$H"
	} > "$TMP"
	if [ $CHECK -eq 1 ]; then
		if ! cmp -s "$TMP" "$INC"; then
			echo "$(basename "$INC") is out of date: run include/make_inc.sh" >&2
			STATUS=1
		fi
		rm -f "$TMP"
	else
		mv "$TMP" "$INC"
		echo "wrote $(basename "$INC")"
	fi
done
exit $STATUS
