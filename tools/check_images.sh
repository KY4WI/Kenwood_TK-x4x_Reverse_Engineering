#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# The rule for radio images in this repository, also run by the content
# check on every push. Run it from anywhere before committing.
#
# Allowed images:
#   - probe images under tools/probes/: NAME.bin must sit next to a source
#     file with the same name (NAME.c, NAME.s, NAME.S or NAME.asm) and be
#     26112 bytes (application flash 8000H-E5FFH);
#   - the synthetic channel tests in channel_tests/ (*.bin, *.img): they are
#     regenerated from build_tkx4x_test_codeplugs.py and must match exactly.
# Other common firmware/ROM-image and generated-listing extensions are
# rejected. This cannot detect disguised/renamed proprietary material,
# nor prove that a probe binary matches its neighboring source file.
set -euo pipefail
cd "$(dirname "$0")/.."
bad=0

# Channel tests: regenerate into a scratch directory and compare.
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
python3 -I channel_tests/build_tkx4x_test_codeplugs.py "$tmp" > /dev/null
for f in "$tmp"/*; do
	n=${f##*/}
	if ! cmp -s "$f" "channel_tests/$n"; then
		echo "channel_tests/$n differs from the generator's output (or is missing)" >&2
		bad=1
	fi
done

while IFS= read -r f; do
	case "$f" in
	./tools/probes/*.bin)
		base=${f%.bin}
		src=""
		for ext in c s S asm; do
			[ -f "$base.$ext" ] && { src="$base.$ext"; break; }
		done
		if [ -z "$src" ]; then
			echo "no source for $f (needs ${base##*/}.c, .s, .S or .asm beside it)" >&2
			bad=1
		elif [ "$(wc -c < "$f")" -ne 26112 ]; then
			echo "$f is $(wc -c < "$f") bytes, expected 26112" >&2
			bad=1
		fi
		;;
	./channel_tests/*.bin|./channel_tests/*.img)
		[ -f "$tmp/${f##*/}" ] || { echo "$f is not made by the channel test generator" >&2; bad=1; }
		;;
	*)
		echo "unexpected radio image or master listing: $f" >&2
		bad=1
		;;
	esac
done < <(find . -type f \( -iname '*.bin' -o -iname '*.img' -o -iname '*.hex' -o -iname '*.lst' -o -iname '*.rom' -o -iname '*.s19' -o -iname '*.srec' -o -iname '*.ihx' \) -not -path './.git/*')
exit $bad
