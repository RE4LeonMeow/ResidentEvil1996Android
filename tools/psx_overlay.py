#!/usr/bin/env python3
"""Inspect the PS1 build: the main SLUS executable and the PROG2 overlays.

The 1997 PS1 release keeps only the boot/core engine resident; everything that
does not fit in 2 MB of RAM is split into overlay PS-EX executables under
``PROG2`` and swapped in on demand.  This tool parses those PS-EX headers,
disassembles entry points/regions with Capstone and extracts embedded strings so
the PS1 side can be matched against the PC decompilation.

Usage (from the repo root):

    python tools/psx_overlay.py info
    python tools/psx_overlay.py strings [--min 4]
    python tools/psx_overlay.py dis <overlay|main> [--at ADDR] [--count N]
    python tools/psx_overlay.py calls <overlay|main> [--target ADDR]

Addresses are MIPS virtual addresses (0x80......).  The main executable is named
``main``; overlays are ``LOGO``, ``TITLE``, ``STAGE1`` ... ``ENDING``.
"""

import argparse
import glob
import os
import struct
import sys

try:
    from capstone import Cs, CS_ARCH_MIPS, CS_MODE_MIPS32, CS_MODE_LITTLE_ENDIAN
except ImportError:
    sys.exit("capstone is required: pip install capstone")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets", "PSX")
PROG2_DIR = os.path.join(ASSETS, "PROG2")
# The main executable is not shipped with this repo; default to the extracted
# ISO copy used by the Ghidra project, overridable with --main.
MAIN_EXE = os.environ.get("RE1_PSX_MAIN") or os.path.join(ASSETS, "SLUS_001.70")
if not os.path.exists(MAIN_EXE):
    MAIN_EXE = r"G:\redecomp\iso_content\RE1_psx\SLUS_001.70"

PSX_MAGIC = b"PS-X EXE"
HEADER_SIZE = 0x800

_md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 + CS_MODE_LITTLE_ENDIAN)
_md.detail = True


class Psex(object):
    def __init__(self, path):
        self.path = path
        self.name = os.path.splitext(os.path.basename(path))[0]
        with open(path, "rb") as fh:
            self.data = fh.read()
        if self.data[:8] != PSX_MAGIC:
            raise ValueError("%s: not a PS-EX file (%r)" % (path, self.data[:8]))
        (self.entry, self.gp, self.load_addr, self.text_size,
         self.memfill_addr, self.memfill_size,
         self.sp_base, self.sp_offset) = struct.unpack_from("<8I", self.data, 0x10)

    @property
    def text(self):
        return self.data[HEADER_SIZE:HEADER_SIZE + self.text_size]

    def disasm(self, vaddr=None, count=40):
        va = self.entry if vaddr is None else vaddr
        off = va - self.load_addr + HEADER_SIZE
        if off < 0 or off >= len(self.data):
            raise ValueError("address %08x outside %s text" % (va, self.name))
        return list(_md.disasm(self.data[off:off + count * 4], va))[:count]

    def sweep(self):
        """Linear sweep the whole text, skipping words Capstone cannot decode."""
        base = self.load_addr
        end = self.load_addr + self.text_size
        off = HEADER_SIZE
        while off < HEADER_SIZE + self.text_size:
            chunk = self.data[off:off + 4]
            ins = list(_md.disasm(chunk, base + (off - HEADER_SIZE)))
            if ins:
                yield ins[0]
            off += 4


def overlays():
    out = []
    for path in sorted(glob.glob(os.path.join(PROG2_DIR, "*.EXE"))):
        try:
            out.append(Psex(path))
        except ValueError as exc:
            print("skip: %s" % exc, file=sys.stderr)
    return out


def load(name):
    if name.lower() in ("main", "slus", "slus_001.70"):
        return Psex(MAIN_EXE)
    for ov in overlays():
        if ov.name.lower() == name.lower():
            return ov
    raise SystemExit("unknown image %r (use info to list)" % name)


def cmd_info(args):
    rows = [Psex(MAIN_EXE)] + overlays()
    print("%-10s %8s %8s %8s %9s %9s  %s" %
          ("image", "file", "entry", "load", "textsize", "memfill", "range"))
    for img in rows:
        end = img.load_addr + img.text_size
        print("%-10s %8d %08x %08x %9d %9d  %08x-%08x" %
              (img.name, os.path.getsize(img.path), img.entry, img.load_addr,
               img.text_size, img.memfill_size, img.load_addr, end))


def iter_strings(data, minlen):
    cur = bytearray()
    start = 0
    for i, b in enumerate(data):
        if 0x20 <= b < 0x7F:
            if not cur:
                start = i
            cur.append(b)
        else:
            if len(cur) >= minlen:
                yield start, cur.decode("ascii")
            cur = bytearray()
    if len(cur) >= minlen:
        yield start, cur.decode("ascii")


def cmd_strings(args):
    rows = [Psex(MAIN_EXE)] + overlays()
    for img in rows:
        seen = []
        for off, s in iter_strings(img.text, args.min):
            if any(ch in s for ch in "\\/.") or s.isupper() or " " in s:
                seen.append((off, s))
        print("== %s (%d strings) ==" % (img.name, len(seen)))
        for off, s in seen[:args.limit]:
            va = img.load_addr + off - HEADER_SIZE
            print("  %08x  %s" % (va, s))


def cmd_dis(args):
    img = load(args.image)
    vaddr = img.entry if args.at is None else int(args.at, 16)
    print("%s @ %08x (%s)" % (img.name, vaddr, img.path))
    for ins in img.disasm(vaddr, args.count):
        print("  %08x  %-8s %s" % (ins.address, ins.mnemonic, ins.op_str))


def cmd_calls(args):
    target = int(args.target, 16)
    img = load(args.image)
    for ins in img.disasm(img.entry, img.text_size // 4):
        if ins.mnemonic in ("jal", "j") and ins.operands and \
                ins.operands[0].type == 2 and (ins.operands[0].imm & 0xFFFFFFFF) == target:
            print("%08x  %s %s" % (ins.address, ins.mnemonic, ins.op_str))


def cmd_extern(args):
    """List unique call targets outside this image's own load range."""
    img = load(args.image)
    lo, hi = img.load_addr, img.load_addr + img.text_size
    targets = {}
    for ins in img.sweep():
        if ins.mnemonic != "jal" or not ins.operands or ins.operands[0].type != 2:
            continue
        tgt = ins.operands[0].imm & 0xFFFFFFFF
        if not (0x80010000 <= tgt < 0x800b0000):
            continue
        if not (lo <= tgt < hi):
            targets.setdefault(tgt, []).append(ins.address)
    for tgt in sorted(targets):
        print("%08x  called from %d site(s)" % (tgt, len(targets[tgt])))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("info").set_defaults(func=cmd_info)

    p = sub.add_parser("strings")
    p.add_argument("--min", type=int, default=4)
    p.add_argument("--limit", type=int, default=60)
    p.set_defaults(func=cmd_strings)

    p = sub.add_parser("dis")
    p.add_argument("image")
    p.add_argument("--at", default=None, help="virtual address (hex)")
    p.add_argument("--count", type=int, default=40)
    p.set_defaults(func=cmd_dis)

    p = sub.add_parser("calls")
    p.add_argument("image")
    p.add_argument("--target", required=True, help="virtual address (hex)")
    p.set_defaults(func=cmd_calls)

    p = sub.add_parser("extern")
    p.add_argument("image")
    p.set_defaults(func=cmd_extern)

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
