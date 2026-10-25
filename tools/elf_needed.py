#!/usr/bin/env python3
"""Print an ELF's class/machine and its DT_NEEDED list, in dynamic-table order.

The loader reports only the FIRST unresolvable soname, so the order matters when
diagnosing a missing-library error on a machine we cannot run readelf on.
"""
import struct
import sys

path = sys.argv[1]
d = open(path, "rb").read()

assert d[:4] == b"\x7fELF", "not an ELF"
ei_class, ei_data = d[4], d[5]
is32 = ei_class == 1
fmt = "<" if ei_data == 1 else ">"
print(f"class={'ELF32' if is32 else 'ELF64'} data={'LE' if ei_data == 1 else 'BE'}")

if is32:
    e_machine, = struct.unpack_from(fmt + "H", d, 18)
    e_phoff, = struct.unpack_from(fmt + "I", d, 28)
    e_phentsize, e_phnum = struct.unpack_from(fmt + "HH", d, 42)
else:
    e_machine, = struct.unpack_from(fmt + "H", d, 18)
    e_phoff, = struct.unpack_from(fmt + "Q", d, 32)
    e_phentsize, e_phnum = struct.unpack_from(fmt + "HH", d, 54)
print(f"machine={e_machine} (3=i386, 62=x86-64)")

PT_LOAD, PT_DYNAMIC = 1, 2
loads, dyn_off, dyn_size = [], None, 0
for i in range(e_phnum):
    o = e_phoff + i * e_phentsize
    if is32:
        p_type, p_offset, p_vaddr, _, p_filesz, _, _, _ = struct.unpack_from(fmt + "8I", d, o)
    else:
        p_type, p_flags, p_offset, p_vaddr, _, p_filesz, _, _ = struct.unpack_from(fmt + "2I6Q", d, o)
    if p_type == PT_LOAD:
        loads.append((p_vaddr, p_offset, p_filesz))
    elif p_type == PT_DYNAMIC:
        dyn_off, dyn_size = p_offset, p_filesz


def vaddr_to_off(va):
    for vaddr, off, sz in loads:
        if vaddr <= va < vaddr + sz:
            return off + (va - vaddr)
    return None


DT_NULL, DT_NEEDED, DT_STRTAB, DT_STRSZ = 0, 1, 5, 10
entsize = 8 if is32 else 16
strtab_va = None
needed = []
for o in range(dyn_off, dyn_off + dyn_size, entsize):
    if is32:
        tag, val = struct.unpack_from(fmt + "iI", d, o)
    else:
        tag, val = struct.unpack_from(fmt + "qQ", d, o)
    if tag == DT_NULL:
        break
    if tag == DT_NEEDED:
        needed.append(val)
    elif tag == DT_STRTAB:
        strtab_va = val

stroff = vaddr_to_off(strtab_va)
print("DT_NEEDED (in order):")
for n in needed:
    end = d.index(b"\0", stroff + n)
    print("  " + d[stroff + n:end].decode())
