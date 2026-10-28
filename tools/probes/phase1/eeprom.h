/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Dave, KY4WI */
/* AT24C02 EEPROM access (hardware doc section 4 / D above).
 *
 * Thin conveniences over the confirmed resident_api.h shims
 * (eeprom_seq_read/eeprom_page_write/eeprom_byte_read). Page size is 8
 * bytes; eeprom_page_write() (the resident 1F18) does NOT protect
 * against crossing an 8-byte page boundary -- eeprom_write() below
 * splits a multi-byte write at page boundaries itself so callers don't
 * have to reason about EEPROM page geometry.
 *
 * CURRENT PROJECT POLICY: READ-ONLY, ENFORCED AT BUILD TIME, NOT JUST BY
 * CONVENTION. Until the full 256-byte calibration EEPROM has actually
 * been backed up off-target and its format is genuinely understood (not
 * just the 3 confirmed DAC-calibration bytes -- see calibration.h), this
 * project writes nothing to it. eeprom_write() below -- the only
 * function in this codebase that reaches the resident write primitive
 * (1F18/eeprom_page_write) -- is compiled in only when EEPROM_ALLOW_WRITE
 * is defined. No build.sh in this project defines it, so eeprom_write()
 * is not merely unused right now, it is not even PRESENT in either
 * phase1.bin or selftest.bin's compiled object code -- a future call
 * site added to phase1.c/calibration.c/diag.c etc. would fail to link
 * unless someone deliberately turns this gate on first, which is the
 * point: the policy survives an accidental future call site, not just
 * today's absence of one. Once a real, backed-up, understood EEPROM
 * write is actually wanted, define EEPROM_ALLOW_WRITE for that build
 * and update this comment to say why it's now safe.
 *
 * eeprom_verify()/eeprom_dump_all() below are NOT gated -- both are
 * genuinely read-only (eeprom_seq_read only, never eeprom_page_write),
 * so they carry none of the write risk this gate exists for; they're
 * ungated on their own merits, not as an oversight.
 *
 * Hardware doc's own explicit guidance (section 4.3/"design rules") once
 * writes ARE wanted: dump/back up the full 256 bytes before any write
 * experiments, keep ordinary firmware read-mostly, verify every write by
 * reading it back (eeprom_verify() below is for that last step). */
#ifndef EEPROM_H
#define EEPROM_H

#include "hardware.h"

#define EEPROM_PAGE_SIZE 8
#define EEPROM_SIZE      256
#define EEPROM_BLANK_BYTE 0xFF   /* mask ROM's own "never formatted" sentinel */

/* Read `count` bytes starting at `addr` into `dest`. Thin pass-through
 * to eeprom_seq_read() -- reads have no page-boundary restriction. */
void eeprom_read(u8 addr, void *dest, u8 count);

#ifdef EEPROM_ALLOW_WRITE
/* Write `count` bytes from `src` starting at `addr`, splitting at
 * 8-byte page boundaries as needed (the resident page-write routine
 * does not do this itself). Only compiled in when EEPROM_ALLOW_WRITE is
 * defined -- see this file's header comment. */
void eeprom_write(u8 addr, const void *src, u8 count);
#endif

/* Read back `count` bytes from `addr` and compare against `expect`.
 * Returns 1 if they match, 0 if not. Intended for the "verify every
 * calibration write by reading it back" rule -- callers decide what to
 * do on a mismatch (retry, abort, flag an error) since this module has
 * no opinion on recovery policy. */
u8 eeprom_verify(u8 addr, const void *expect, u8 count);

/* Dump the full 256-byte EEPROM into `dest` (which must be at least
 * EEPROM_SIZE bytes). For the "back up before any calibration write
 * experiments" rule.
 *
 * WARNING, read before calling this on-target: total internal RAM on
 * this CPU is 256 bytes FULL STOP (hardware.h's confirmed constraint,
 * doc section 1.4) -- a 256-byte `dest` cannot be a local/stack
 * variable here (the stack itself is only 32 of those 256 bytes) and
 * cannot be `.bss` either (phase1.ld's ramdata_region is deliberately
 * only 32 bytes, and even the full 208-byte free-RAM budget couldn't
 * hold it alongside everything else already living there). This
 * function is only genuinely usable if `dest` points at memory this
 * firmware does NOT otherwise manage on this CPU (there isn't any), or
 * on a future/different target with more RAM. For this project, on
 * this hardware, prefer eeprom_is_blank() or eeprom_checksum_all()
 * below, which read the complete EEPROM the same way but without
 * needing anywhere to put 256 bytes at once. */
void eeprom_dump_all(void *dest);

/* True if the EEPROM's blank-sentinel byte (address 0xFF, the very
 * last byte) reads as EEPROM_BLANK_BYTE (0xFF) -- the same check the
 * mask ROM's own EEPROM-format routine (0x102B) uses to decide the
 * EEPROM has "never [been] formatted" (hardware doc section 9.4.1).
 * Cheap (one byte read), safe to call any time calibration data is
 * about to be relied on. */
u8 eeprom_is_blank(void);

/* Read the complete 256-byte EEPROM, one byte at a time via
 * eeprom_byte_read(), and return a running 8-bit sum (wrapping,
 * plain byte addition) of every byte read. Genuinely reads the whole
 * device -- satisfying doc section 9.4's "dump all 256 EEPROM bytes"
 * step in spirit -- without ever holding more than the one accumulator
 * byte in RAM (see hardware.h's RAM-budget comment for why a real
 * 256-byte buffer isn't possible on this CPU). This is NOT a
 * substitute for the doc's actual backup policy (save a real 256-byte
 * dump with radio identity/date, captured out-of-band, before any
 * calibration write experiment) -- it is a lightweight on-target
 * "does the EEPROM still look the same as last time" signal a caller
 * can log/display/compare, cheap enough to run on every boot. */
u8 eeprom_checksum_all(void);

#endif
