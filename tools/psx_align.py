#!/usr/bin/env python3
"""Byte-level alignment of two PS1 Resident Evil builds (e.g. SLUS_001.70 OG
vs SLUS_005.51 Director's Cut).

The two builds are different compilations with shifted addresses, so neither
address matching nor Ghidra's cross-binary fuzzy matcher (which ties on
identical small functions) works.  This disassembles both binaries with
Capstone at the function boundaries reported by Ghidra, fingerprints each
function as a set of 5-gram normalised-instruction tokens, and runs a
Needleman-Wunsch alignment that preserves function order.  Register names and
all immediates are masked so address shifts and register-allocation changes do
not affect the score.

Usage:
    # 1. dump the Ghidra function lists (REST bridge on :8089)
    python tools/psx_align.py dump SLUS_001.70 og_fn.txt
    python tools/psx_align.py dump SLUS_005.51 dc_fn.txt

    # 2. align the two executables
    python tools/psx_align.py align <og_exe> <dc_exe> og_fn.txt dc_fn.txt \
        -o pairs.json [--match 0.22]

    # 3. candidate rename TSV for functions still named FUN_/LAB_ in the
    #    target, whose source side has a real name
    python tools/psx_align.py rename-tsv pairs.json dc_fn.txt -o renames.tsv

The TSV is `address<TAB>name<TAB>source_address<TAB>source_name`, ready to POST
to the bridge's /rename_function_by_address (`strict_mode: off` for names that
trip the Hungarian/verb guardrails).

Requires: capstone, numpy (`pip install capstone numpy`).
"""
import argparse
import hashlib
import json
import re
import urllib.request

import numpy as np

TEXTADDR = 0x80010000
REST = 'http://127.0.0.1:8089'


def load_names(path):
    """Parse Ghidra `list_functions` output: '<name> at <hex>' per line."""
    out = {}
    for line in open(path, encoding='utf-8', errors='replace'):
        m = re.match(r'^(.*) at ([0-9a-fA-F]+)$', line.rstrip('\n'))
        if m:
            out[int(m.group(2), 16)] = m.group(1)
    return out


def dump(program, path):
    url = f'{REST}/list_functions?program={urllib.parse.quote(program)}'
    data = urllib.request.urlopen(url, timeout=60).read().decode()
    open(path, 'w', encoding='utf-8').write(data)
    print(f'{program}: {data.count(chr(10))} functions -> {path}')


def _norm(insn, md):
    parts = [p.strip() for p in insn.op_str.split(',')] if insn.op_str else []
    shapes = []
    for p in parts:
        if not p:
            continue
        if p.startswith('$'):
            shapes.append('r')
        elif re.match(r'^-?(0x[0-9a-f]+|\d+)$', p):
            shapes.append('#')
        else:
            p2 = re.sub(r'0x[0-9a-fA-F]+', '#', p)
            shapes.append(re.sub(r'\d+', '#', p2))
    return insn.mnemonic + ' ' + ','.join(shapes)


def build(exe, names):
    from capstone import Cs, CS_ARCH_MIPS, CS_MODE_MIPS32, CS_MODE_LITTLE_ENDIAN
    md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 + CS_MODE_LITTLE_ENDIAN)
    text = open(exe, 'rb').read()[0x800:]
    addrs = sorted(a for a in names if a >= TEXTADDR)
    funcs = []
    for k, a in enumerate(addrs):
        end = addrs[k + 1] if k + 1 < len(addrs) else a + 64
        n = min(end - a, 16384)
        toks = [_norm(i, md) for i in md.disasm(text[a - TEXTADDR:a - TEXTADDR + n], a)]
        grams = set(tuple(toks[j:j + 5]) for j in range(max(0, len(toks) - 4)))
        funcs.append({'a': a, 'n': names[a], 'toks': toks, 'grams': grams})
    return funcs


def sim(a, b):
    if a['toks'] == b['toks']:
        return 1.0
    ga, gb = a['grams'], b['grams']
    if not ga or not gb:
        return 0.0
    inter = len(ga & gb)
    return inter / (len(ga) + len(gb) - inter)


def align(og, dc, match, gap):
    n, m = len(og), len(dc)
    S = np.full((n + 1, m + 1), -1e9, dtype=np.float32)
    T = np.zeros((n + 1, m + 1), dtype=np.int8)
    S[:, 0] = np.arange(n + 1) * gap
    S[0, :] = np.arange(m + 1) * gap
    for i in range(1, n + 1):
        ai = og[i - 1]
        for j in range(1, m + 1):
            s = sim(ai, dc[j - 1])
            diag = S[i - 1, j - 1] + (s if s >= match else -1e9)
            up = S[i - 1, j] + gap
            left = S[i, j - 1] + gap
            best, t = diag, 0
            if up > best:
                best, t = up, 1
            if left > best:
                best, t = left, 2
            S[i, j], T[i, j] = best, t
    pairs = []
    i, j = n, m
    while i > 0 or j > 0:
        t = T[i, j]
        if i > 0 and j > 0 and t == 0:
            s = sim(og[i - 1], dc[j - 1])
            if s >= match:
                pairs.append((dc[j - 1], og[i - 1], s))
            i -= 1
            j -= 1
        elif i > 0 and (j == 0 or t == 1):
            i -= 1
        else:
            j -= 1
    pairs.reverse()
    return pairs


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)

    d = sub.add_parser('dump')
    d.add_argument('program')
    d.add_argument('out')

    a = sub.add_parser('align')
    a.add_argument('og_exe')
    a.add_argument('dc_exe')
    a.add_argument('og_names')
    a.add_argument('dc_names')
    a.add_argument('-o', '--out', default='pairs.json')
    a.add_argument('--match', type=float, default=0.22)
    a.add_argument('--gap', type=float, default=-0.50)

    r = sub.add_parser('rename-tsv')
    r.add_argument('pairs')
    r.add_argument('dc_names')
    r.add_argument('-o', '--out', default='renames.tsv')

    args = ap.parse_args()
    if args.cmd == 'dump':
        dump(args.program, args.out)
        return

    if args.cmd == 'align':
        og = build(args.og_exe, load_names(args.og_names))
        dc = build(args.dc_exe, load_names(args.dc_names))
        print(f'OG {len(og)} functions, DC {len(dc)} functions')
        pairs = align(og, dc, args.match, args.gap)
        named = [p for p in pairs if not p[1]['n'].startswith(('FUN_', 'LAB_'))]
        json.dump([(p[0]['a'], p[0]['n'], p[1]['a'], p[1]['n'], float(p[2]))
                   for p in pairs], open(args.out, 'w'))
        print(f'{len(pairs)} aligned pairs ({len(named)} with a real source name) -> {args.out}')
        return

    if args.cmd == 'rename-tsv':
        names = load_names(args.dc_names)
        rows = []
        for dca, dcn, oga, ogn, sc in json.load(open(args.pairs)):
            if names.get(dca, '').startswith(('FUN_', 'LAB_')) and \
               not ogn.startswith(('FUN_', 'LAB_')):
                rows.append((dca, ogn, oga))
        rows.sort()
        with open(args.out, 'w') as f:
            for dca, ogn, oga in rows:
                f.write(f'{dca:08x}\t{ogn}\t{oga:08x}\t{ogn}\n')
        print(f'{len(rows)} rename candidates -> {args.out}')


if __name__ == '__main__':
    main()
