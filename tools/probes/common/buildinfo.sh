#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# buildinfo.sh VBCC VASM VLINK OUT.c
#
# Writes OUT.c holding the versions of the tools that built the image:
#   const char build_tools[] = "vbcc 0.1, vasm 2.0f 0.1, vlink 0.18a";
# The image prints it at start, so a bench report says which compiler made
# the code that ran. vbcc and vasm report their own 78K/III backend's
# version (machine.c, cpu.c; NEC-78K3-Toolchain, not vbcc/vasm's own
# release version, which the second number already covers for vasm).
#
# Accept both current backend version banners ("code-generator 0.1",
# "cpu backend 0.1") and older revision banners ("code-generator, rev
# 770dbce", "cpu backend rev 9756920"). The output format is unchanged.
# Match the upstream vasm release (e.g. 2.0f), not the "78k3" CPU name.
set -e
OUT=$(cd "$(dirname "$4")" && pwd)/$(basename "$4")
TMPD=$(mktemp -d)
trap 'rm -rf "$TMPD"' EXIT
cd "$TMPD"   # vbcc and vlink leave files in the current directory when run bare
banner() { "$@" </dev/null 2>&1 || true; }
vbcc=$(banner "$1" | sed -n \
  -e 's/.*code-generator, rev \([^ ]*\).*/\1/p' \
  -e 's/.*code-generator \([^ ]*\).*/\1/p' | head -1)
vasmv=$(banner "$2" | sed -n \
  -e 's/^vasm \([0-9][0-9]*\.[0-9][^ ]*\).*/\1/p' \
  -e 's/^vasm V\([0-9][0-9]*\.[0-9][^ ]*\).*/\1/p' | head -1)
vasmr=$(banner "$2" | sed -n \
  -e 's/.*cpu backend rev \([^ ]*\).*/\1/p' \
  -e 's/.*cpu backend \([^ ]*\).*/\1/p' | head -1)
vlink=$(banner "$3" -v | sed -n 's/^vlink V\([^ ]*\).*/\1/p' | head -1)
printf 'const char build_tools[] = "vbcc %s, vasm %s %s, vlink %s";\n' \
  "${vbcc:-unknown}" "${vasmv:-unknown}" "${vasmr:-unknown}" "${vlink:-unknown}" > "$OUT"
