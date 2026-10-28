#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
"""Generate synthetic TK-x4x channel tests from the documented codeplug layout.

These are EXPERIMENTAL BENCH TESTS, not normal programming channels.
The first 0x100 bytes use the CHIRP driver's *synthetic* feature-test
fixture's globals; system pointers/counts are rebuilt from scratch.
No real radio's serial number, EEPROM calibration, or firmware is included.
"""
from __future__ import annotations

import base64
import json
import struct
from dataclasses import dataclass
from pathlib import Path

import sys

# Output directory: this script's own directory, or the first argument
# (the content check regenerates into a scratch directory and compares).
OUT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parent
SIZE = 0x1800
POOL = 0x100
PTR_BIAS = 0x2600
MAGIC = b'\x00\xffchirp\xeeimg\x00\x01'

# Global section from CHIRP driver test-images/feature/Kenwood_TK-840.img.
# The final bytes through 0xFF are unused FF fill.
REFERENCE_GLOBAL_HEX = '''
0067a667ffffffffffffffff4c28ffffffffffffffffffffffffffffffffdf68ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7229050808ffffffff07ffffffffffffffff07ffffffffffffffffffffffffffffff060027fe01fffffe20fafefaffffffffffffffffffffffffffffffffffffff05e095e095209c209c30a030a081a581a5c0abc0abcdb2cdb241b841b880be80be40c240c260c560c5ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
'''

@dataclass(frozen=True)
class Ch:
    name: str
    rx: float
    tx: float | None
    inhibit: bool
    talkaround: bool = False
    absent_word: int = 0x7FFF
    note: str = ''

# Float MHz values are all on exact 12.5-kHz points.
EIGHT = [
 Ch('SIMPLEX0', 446.0000, 446.0000, False, note='Simplex: PTT keys at 446.0000'),
 Ch('TXINHBIT', 446.0125, 446.0125, True, note='Valid TX word, but PTT must be inhibited'),
 Ch('OFF7FFF0', 446.0250, None, True, absent_word=0x7FFF, note='Duplex off: TX word 7FFF, TX Inhibit enabled'),
 Ch('OFFFFFF0', 446.0375, None, True, absent_word=0xFFFF, note='Duplex off: TX word FFFF, TX Inhibit enabled'),
 Ch('SPLITTX0', 446.0500, 446.0750, False, note='Split: expect RF at 446.0750'),
 Ch('TALKARND', 446.0875, 446.1125, False, talkaround=True, note='Talk Around enabled: expect TX on RX 446.0875'),
 Ch('OFF2SIMP', 446.1000, 446.1000, True, note='Duplex restored to simplex, TX Inhibit still set; clear Extra manually'),
 Ch('LOWEDGE0', 420.0000, 420.0000, True, note='Receive/word boundary: inhibit TX'),
 Ch('HIGHEDGE', 450.0000, 450.0000, True, note='Receive/word boundary: inhibit TX'),
]
NINE = [
 Ch('STKOFFST', 860.0000, 861.0000, False, note='Stock TX 815.0000; patched uses 861.0000'),
 Ch('TXINHBIT', 860.0125, 861.0125, True, note='Valid stored TX, but inhibit PTT'),
 Ch('OFF7FFF0', 860.0250, None, True, absent_word=0x7FFF, note='Absent TX 7FFF and inhibited'),
 Ch('OFFFFFF0', 860.0375, None, True, absent_word=0xFFFF, note='Absent TX FFFF and inhibited'),
 Ch('SPLITTX0', 859.0000, 862.0000, False, note='Stock TX 814.0000; patched uses 862.0000'),
 Ch('TALKARND', 860.0500, 861.0500, False, talkaround=True, note='Stock Talk Around TX 860.0500; patched TX 861.0500'),
 Ch('OFF2SIMP', 860.0625, 860.0625, True, note='Simplex TX word but inhibit retained'),
 Ch('LOWEDGE0', 851.0000, 856.0000, True, note='Stock RX boundary, inhibit TX'),
 Ch('HIGHEDGE', 870.0000, 865.0000, True, note='Stock RX boundary, inhibit TX'),
]
ONE = [
 Ch('STKOFFST', 940.0000, 900.0000, False, note='Stock TX 901.0000; patched uses 900.0000'),
 Ch('TXINHBIT', 940.0125, 900.0125, True, note='Valid stored TX, but inhibit PTT'),
 Ch('OFF7FFF0', 940.0250, None, True, absent_word=0x7FFF, note='Absent TX 7FFF and inhibited'),
 Ch('OFFFFFF0', 940.0375, None, True, absent_word=0xFFFF, note='Absent TX FFFF and inhibited'),
 Ch('SPLITTX0', 939.0000, 899.5000, False, note='Stock TX 900.0000; patched TX 899.5000'),
 Ch('TALKARND', 940.0500, 900.0500, False, talkaround=True, note='Stock Talk Around TX 940.0500; patched TX 900.0500'),
 Ch('OFF2SIMP', 940.0625, 940.0625, True, note='Simplex TX word but inhibit retained'),
 Ch('HAM927RX', 927.0000, 902.0000, True, note='Ham 927/902 pair; TX inhibited by default; modified hardware only'),
 Ch('LOWEDGE0', 935.0000, 900.0000, True, note='Stock RX boundary, inhibit TX'),
 Ch('HIGHEDGE', 941.0000, 900.0000, True, note='Stock RX boundary, inhibit TX'),
]
MODELS = {'840': ('TK-840', EIGHT), '940': ('TK-940', NINE), '941': ('TK-941', ONE)}


def freq_word(mhz: float | None, family: str, absent_word=0x7FFF) -> int:
    if mhz is None:
        assert absent_word in (0x7FFF, 0xFFFF)
        return absent_word
    base = 300.0 if family == '840' else 851.0
    steps = round((mhz - base) * 80)
    assert 0 <= steps < 0x7FFF
    assert abs(base + steps / 80 - mhz) < 1e-7, (mhz, family)
    return steps | 0x8000


def make_image(systems: list[tuple[int, str, str, list[Ch]]], *, chirp_probe=False) -> bytes:
    img = bytearray(b'\xff' * SIZE)
    base = bytes.fromhex(REFERENCE_GLOBAL_HEX)
    assert len(base) <= 256
    img[:len(base)] = base
    img[:0x40] = b'\xff' * 0x40
    img[0x40] = len(systems)
    img[0x41:0x61] = b'\xff' * 32
    img[0x6C] |= 0x04  # retain mask-ROM firmware loader access
    pos = POOL
    for system_number, system_name, family, chans in systems:
        assert 1 <= system_number <= 32
        assert len(system_name) <= 8
        assert len(chans) <= 250
        img[(system_number-1)*2:(system_number-1)*2+2] = struct.pack('<H', (pos + PTR_BIAS) | 0x4000)
        img[0x41 + system_number-1] = len(chans)
        header = b'\x7e\xff\xfe\xff\xff\xff' + system_name.ljust(8).encode('ascii')
        assert len(header) == 14
        img[pos:pos+14] = header
        pos += 14
        for group_number, ch in enumerate(chans, 1):
            assert len(ch.name) == 8 and ch.name.isascii() and ch.name.isupper(), ch.name
            flags = 0xFF
            inhibited = ch.inhibit
            if chirp_probe and ch.tx is None:
                # CHIRP-only test: deliberately leave TX uninhibited on the
                # absent-word channels, so CHIRP's upload sanitizer must fix it.
                inhibited = False
            if inhibited: flags &= ~0x04
            if ch.talkaround: flags &= ~0x02
            rx = freq_word(ch.rx, family)
            tx = freq_word(ch.tx, family, ch.absent_word)
            rec = struct.pack('<BHHHHBB8s', group_number, rx, tx, 0xFFFF, 0xFFFF,
                              flags, 0xFF, ch.name.encode('ascii'))
            assert len(rec) == 19
            img[pos:pos+19] = rec
            pos += 19
    assert pos <= SIZE, pos
    return bytes(img)


def to_chirp_img(raw: bytes, model: str) -> bytes:
    # These are deliberately CHIRP fixtures, not Toolkit channel files.
    # Trailer schema matches the driver's checked-in CHIRP test fixtures.
    meta = {'rclass': f'TK{model}Radio', 'vendor': 'Kenwood', 'model': f'TK-{model}',
            'variant': '', 'chirp_version': 'py3dev'}
    return raw + MAGIC + base64.b64encode(json.dumps(meta).encode('utf-8'))


def verify_image(raw: bytes, systems, *, chirp_probe=False):
    assert len(raw) == SIZE
    assert raw[0x40] == len(systems)
    assert raw[0x6C] & 0x04
    off_expected = POOL
    for num, system_name, family, chans in systems:
        (ptr,) = struct.unpack_from('<H', raw, (num-1)*2)
        off = (ptr & 0x3FFF) - PTR_BIAS
        assert not (ptr & 0x8000) and (ptr & 0x4000)
        assert off == off_expected, (off, off_expected)
        assert raw[0x41+num-1] == len(chans)
        assert (raw[off] & 1) == 0
        assert raw[off+6:off+14].decode('ascii').strip() == system_name
        off += 14
        for i,ch in enumerate(chans, 1):
            g, rx, tx, xt, rt, flaga, flagb, name = struct.unpack_from('<BHHHHBB8s', raw, off)
            assert g == i
            assert rx == freq_word(ch.rx, family)
            assert tx == freq_word(ch.tx, family, ch.absent_word)
            assert name.decode('ascii') == ch.name
            assert xt == rt == 0xFFFF
            assert (not bool(flaga & 4)) == (ch.inhibit and not (chirp_probe and ch.tx is None))
            assert (not bool(flaga & 2)) == ch.talkaround
            assert flagb == 0xFF
            off += 19
        off_expected = off
    assert set(raw[off_expected:]) == {0xFF}


def expected_tx(ch: Ch, family: str, patched: bool):
    if ch.inhibit or ch.tx is None: return 'INHIBITED'
    if family == '840':
        return f'{ch.rx if ch.talkaround else ch.tx:.4f}'
    if patched: return f'{ch.tx:.4f}'
    if ch.talkaround: return f'{ch.rx:.4f}'
    return f'{ch.rx - (45 if family == "940" else 39):.4f}'


def write_guide():
    lines = [
'# TK-x4x bench-test channel files',
'',
'**These are synthetic TEST codeplugs, not operational channel plans.**',
'Back up the radio first. Use a properly rated dummy load and an RF',
'counter/spectrum analyzer/service monitor. Do not radiate test transmissions.',
'Only transmit on channels intended for the connected model.',
'',
'## Files',
'',
'- `TK-x4x_UNIVERSAL_TEST_CH.bin`: one 6144-byte raw codeplug with all',
'  three sets. System 1 is named TK-840; System 2 is TK-940;',
'  System 3 is TK-941. Use the Toolkit to load/write this raw file.',
'  When placed in the wrong model, frequencies will decode differently.',
'  SYS/group numbers in the table below refer to the UNIVERSAL file;',
'  each standalone model file puts its channels in System 1.',
'- `TK-840_TEST_CH.bin`, `TK-940_TEST_CH.bin`, `TK-941_TEST_CH.bin`:',
'  raw Toolkit channel files with only the appropriate model\'s System,',
'  numbered System 1 for convenience.',
'- `TK-840_CHIRP_UPLOAD_TEST.img`, `TK-940_CHIRP_UPLOAD_TEST.img`,',
'  `TK-941_CHIRP_UPLOAD_TEST.img`: CHIRP-only model-tagged files. These',
'  DELIBERATELY have TX Inhibit CLEARED on the two Duplex-off channels,',
'  to test that the CHIRP driver forces inhibit BEFORE writing.',
'  The Toolkit rejects these larger CHIRP metadata-bearing files.',
'  CHIRP may change channel data during upload, notably enforcing',
'  TX Inhibit on absent-TX channels. Raw Toolkit writes leave data untouched.',
'',
'System names identify the radio model. Channel names are eight',
'uppercase ASCII characters describing the test. OFF means absent TX.',
'INH means TX Inhibit set; TA means Talk Around enabled.',
'',
'## Expected channels and transmit frequencies',
'',
'Frequency columns in MHz. "Inhibit" means the TX Inhibit bit is set.',
'An expected frequency is a PLL/programming expectation, **not** a',
'guarantee of PLL lock, RF output, or operation within hardware limits.',
'On the TK-840 the Enhanced TX Patch does not apply.',
'',
'| Model | SYS/group | Name | RX | Stored TX | Inhibit | TA | Stock TX | Patched TX |',
'|:--|--:|:--|--:|:--|:--:|:--:|:--|:--|',
]
    for family, (_, chans) in MODELS.items():
        sysnum = {'840': 1, '940': 2, '941': 3}[family]
        for i, ch in enumerate(chans, 1):
            tx_word = '7FFF' if ch.tx is None and ch.absent_word == 0x7FFF else 'FFFF' if ch.tx is None else f'{ch.tx:.4f}'
            lines.append(f'| {family} | {sysnum}/{i} | `{ch.name}` | {ch.rx:.4f} | {tx_word} | {"Y" if ch.inhibit else "N"} | {"Y" if ch.talkaround else "N"} | {expected_tx(ch,family,False)} | {expected_tx(ch,family,True)} |')
    lines += [
'',
'## How to use the tests',
'',
'1. Save a verified original full backup and channel backup.',
'2. Write the raw file using the Toolkit (or open the corresponding',
'   `.img` file in CHIRP when specifically testing the CHIRP upload policy).',
'3. Read channels back; compare eight-character labels and the model',
'   decoded RX/TX values. Model-mismatched Systems in the universal file',
'   will display nonsense frequencies. This is EXPECTED.',
'4. For the `TXINHBIT`, `OFF7FFF0`, `OFFFFFF0`, and `OFF2SIMP` cases,',
'   verify PTT does **not** produce RF or key the transmitter.',
'5. For `SIMPLEX0`, `STKOFFST`, `SPLITTX0`, and `TALKARND`, compare RF',
'   frequency with the stock or patched column for your model.',
'6. With `OFF2SIMP` in CHIRP, change Duplex to off and then back to',
'   simplex. Verify Extra / TX Inhibit stays enabled throughout.',
'   Only manual clearance should',
'   permit TX. After upload, read the radio back and verify.',
'7. For the CHIRP `.img` probe files, first confirm each OFF channel',
'   appears as Duplex off with **TX Inhibit unchecked** in Extra.',
'   Upload using CHIRP, then read back. Both absent-word channels',
'   should now have TX Inhibit enabled; their TX words should be 7FFF.',
'   This specifically tests the CHIRP upload sanitizer, not the Toolkit.',
'8. Read back a CHIRP-written raw image and verify an intentionally',
'   inhibited VALID-TX channel stays inhibited. Valid TX should never',
'   cause automatic uninhibiting.',
'',
'## Model/hardware notes',
'',
'These are expected outcomes for the test channels, not a complete band',
'or firmware-encoding reference. For the **full** explanation of the',
'851 MHz conventional base, TK-940/941 stock offsets, dealer/test',
'812 MHz base, and hardware versus CHIRP limits, see',
'[Frequency Encoding and Limits](../docs/codeplug/Frequency_Encoding_and_Limits.md).',
'',
'- TK-840: the normal firmware honors the stored TX word. No Enhanced TX',
'  firmware patch is applicable. Talk Around should use the RX word.',
'- TK-940 stock: normal TX = RX minus 45 MHz, ignoring the TX word.',
'  TK-940 patched: normal TX follows the stored TX word, which must',
'  encode 851 MHz or above. The stock TK-940 TX split is 806–825 MHz;',
'  patch targets above 851 MHz may not produce RF on unmodified hardware.',
'- TK-941 stock: normal TX = RX minus 39 MHz, ignoring the TX word.',
'  TK-941 patched: normal TX follows the stored TX word. 940/900 MHz',
'  allows a 1 MHz stock/patched difference on original RF hardware.',
'- `TALKARND`: stock uses RX for TX, while the 940/941 Enhanced TX',
'  patch bypasses Talk Around and uses the programmed TX word.',
'- `HAM927RX` is inhibited intentionally. Its 927 RX / 902 TX',
'  represents amateur operation only after suitable hardware work and',
'  with appropriate modified firmware. Stock firmware would derive',
'  TX as 888 MHz from 927 RX; leave TX Inhibit ON until verified.',
'- In all three models, valid program words do NOT guarantee PLL lock.',
'- CHIRP can normalize the `FFFF` absent TX word to `7FFF` on upload.',
'',
'## Source and integrity',
'',
'Generated using the channel block format documented in',
'`KY4WI/Kenwood_TK-x4x_Reverse_Engineering` and the existing CHIRP',
'driver fixture global-settings block. These are newly constructed',
'synthetic images, not radio-extracted codeplugs. All `_CH.bin` files',
'are exactly 6144 bytes. They are intended for controlled bench testing.',
'',
'An actual radio test has NOT yet been performed on these particular',
'generated files. Check every frequency and result on your bench.',
]
    (OUT/'TEST_GUIDE.md').write_text('\n'.join(lines)+'\n')


def main():
    files = []
    universal = [(1, MODELS['840'][0], '840', EIGHT), (2, MODELS['940'][0], '940', NINE), (3, MODELS['941'][0], '941', ONE)]
    raw = make_image(universal)
    verify_image(raw, universal)
    name = 'TK-x4x_UNIVERSAL_TEST_CH.bin'
    (OUT/name).write_bytes(raw); files.append(name)
    for fam, (sysname, chans) in MODELS.items():
        one = [(1, sysname, fam, chans)]
        raw = make_image(one)
        verify_image(raw, one)
        n = f'TK-{fam}_TEST_CH.bin'
        (OUT/n).write_bytes(raw); files.append(n)
        # CHIRP upload enforcement fixture deliberately has absent TX w/o inhibit.
        probe = make_image(one, chirp_probe=True)
        verify_image(probe, one, chirp_probe=True)
        n = f'TK-{fam}_CHIRP_UPLOAD_TEST.img'
        saved = to_chirp_img(probe, fam)
        assert saved[:SIZE] == probe
        assert saved[SIZE:SIZE+len(MAGIC)] == MAGIC
        trailer = json.loads(base64.b64decode(saved[SIZE+len(MAGIC):]))
        assert trailer['rclass'] == f'TK{fam}Radio'
        (OUT/n).write_bytes(saved); files.append(n)
    write_guide()
    files.append('TEST_GUIDE.md')
    for n in files:
        print(f'{n}: {(OUT/n).stat().st_size} bytes')

if __name__ == '__main__': main()
