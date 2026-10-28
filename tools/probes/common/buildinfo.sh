#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# buildinfo.sh VBCC VASM VLINK OUT.c
#
# Writes OUT.c holding the versions of the tools that built the image:
#   const char build_tools[] = "vbcc rev 770dbce, vasm 2.0f rev 9756920, vlink 0.18a";
# The image prints it at start, so a bench report says which compiler made
# the code that ran. vbcc and vasm report the git revision of their 78K/III
# backend (machine.c, cpu.c), with "-modified" if it had uncommitted changes.
set -e
OUT=$(cd "$(dirname "$4")" && pwd)/$(basename "$4")
TMPD=$(mktemp -d)
trap 'rm -rf "$TMPD"' EXIT
cd "$TMPD"   # vbcc and vlink leave files in the current directory when run bare
banner() { "$@" </dev/null 2>&1 || true; }
vbcc=$(banner "$1" | sed -n 's/.*code-generator, rev \([^ ]*\).*/\1/p' | head -1)
vasmv=$(banner "$2" | sed -n 's/^vasm \([0-9][^ ]*\).*/\1/p' | head -1)
vasmr=$(banner "$2" | sed -n 's/.*cpu backend rev \([^ ]*\).*/\1/p' | head -1)
vlink=$(banner "$3" -v | sed -n 's/^vlink V\([^ ]*\).*/\1/p' | head -1)
printf 'const char build_tools[] = "vbcc rev %s, vasm %s rev %s, vlink %s";\n' \
  "${vbcc:-unknown}" "${vasmv:-unknown}" "${vasmr:-unknown}" "${vlink:-unknown}" > "$OUT"
