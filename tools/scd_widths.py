#!/usr/bin/env python3
"""
scd_widths.py - derive SCD command argument widths from the original exe.

For each entry of script_command_funcs_table (0x4c1110 in the exe), disassemble
the function and sum every `ADD dword ptr [0x00bf0800], imm` — that is the total
advance of g_ScdOpcodes including the opcode byte. Width of the argument body is
advance - 1.

Image base mapping: file offset is NOT VA - 0x400000. The PE's sections have
different raw and virtual offsets (.text is VA 0x401000 at raw 0x600, so .text
addresses map as VA - 0x400A00). Use va_to_off() below, which walks the section
table. Disassembling with the naive VA - 0x400000 lands 0xA00 bytes off and
produces plausible-looking garbage rather than an obvious failure.
"""
import os
import struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
EXE = os.path.join(PROJECT_DIR, 'assets', 'ResidentEvil.exe')
IMAGE_BASE = 0x400000
TABLE_VA = 0x4c1110
GSCO = 0x00bf0800  # g_ScdOpcodes

data = open(EXE, "rb").read()
md = Cs(CS_ARCH_X86, CS_MODE_32)


def va_to_off(va):
    rva = va - IMAGE_BASE
    # section map (image base 0x400000)
    secs = [
        (0x1000, 0xae000, 0x600),      # .text
        (0xaf000, 0x2000, 0xae600),    # .rdata
        (0xb1000, 0x8e2000, 0xb0400),  # .data
        (0x993000, 0x2000, 0xd5800),   # .idata
        (0x995000, 0x2000, 0xd6a00),   # .rsrc
        (0x997000, 0x1b000, 0xd8400),  # .reloc
    ]
    for sva, ssize, soff in secs:
        if sva <= rva < sva + ssize:
            return rva - sva + soff
    raise ValueError(f"VA {va:#x} not in any section")


def read_u32(off):
    return struct.unpack_from("<I", data, off)[0]


def disasm_fn(va):
    off = va_to_off(va)
    insns = []
    for i in md.disasm(data[off:off + 0x200], va):
        insns.append(i)
        if i.mnemonic == "ret":
            break
    return insns


def compute_advance(va):
    total = 0
    for i in disasm_fn(va):
        if i.mnemonic in ("add", "sub") and i.op_str.startswith(f"dword ptr [{GSCO:#x}]"):
            imm = i.op_str.split(",")[1].strip()
            try:
                val = int(imm, 0)
            except ValueError:
                continue  # register-based advance (dynamic), ignore
            total += val if i.mnemonic == "add" else -val
    return total


table_off = va_to_off(TABLE_VA)
for idx in range(0x51):
    fn_va = read_u32(table_off + idx * 4)
    if fn_va == 0:
        continue
    advance = compute_advance(fn_va)
    print(f"0x{idx:02X}: {fn_va:#010x} advance={advance} width={advance - 1}")
