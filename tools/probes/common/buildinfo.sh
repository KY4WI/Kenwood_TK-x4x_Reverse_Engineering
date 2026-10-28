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
# NOTE: before 2026-10-28, these backends stamped a git commit hash here
# ("vbcc rev 770dbce", "vasm 2.0f rev 9756920") instead of a plain
# version number -- the sed patterns below match the CURRENT banner
# text ("code-generator 0.1", "cpu backend 0.1"); if you're building
# from an old private-repo checkout with the old git-rev banner instead
# of NEC-78K3-Toolchain, these will print "unknown" and need their old
# ", rev " patterns back.
set -e
OUT=$(cd "$(dirname "$4")" && pwd)/$(basename "$4")
TMPD=$(mktemp -d)
trap 'rm -rf "$TMPD"' EXIT
cd "$TMPD"   # vbcc and vlink leave files in the current directory when run bare
banner() { "$@" </dev/null 2>&1 || true; }
vbcc=$(banner "$1" | sed -n 's/.*code-generator \([^ ]*\).*/\1/p' | head -1)
vasmv=$(banner "$2" | sed -n 's/^vasm \([0-9][^ ]*\).*/\1/p' | head -1)
vasmr=$(banner "$2" | sed -n 's/.*cpu backend \([^ ]*\).*/\1/p' | head -1)
vlink=$(banner "$3" -v | sed -n 's/^vlink V\([^ ]*\).*/\1/p' | head -1)
printf 'const char build_tools[] = "vbcc %s, vasm %s %s, vlink %s";\n' \
  "${vbcc:-unknown}" "${vasmv:-unknown}" "${vasmr:-unknown}" "${vlink:-unknown}" > "$OUT"
