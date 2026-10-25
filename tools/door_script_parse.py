#!/usr/bin/env python3
"""Parse RE1 door animation .dor files (scripts + data regions).

Validates the bytecode interpreter model derived from the original exe:
38 handlers at 0x00443950-0x004443a0 dispatched from FUN_00444540.

Usage: python door_script_parse.py <file.dor>
"""
import struct
import sys

# Opcode info: (name, operand_size_including_opcode, format_desc)
OPS = {
    0x00: ("end_all", 1, "end animation (state=0)"),
    0x01: ("clear_self", 1, "clear own command entry, yield"),
    0x02: ("wait_entry_free", 2, "yield until entry[b1] inactive, then data+=2"),
    0x03: ("yield", 2, "data+=2, yield"),
    0x04: ("jmp_if_byte", 6, "if byteVar[b2] relop[b3] b4: data += b1 (from fallthrough); data+=6"),
    0x05: ("jmp_if_short", 6, "if shortVar[b2] relop[b3] w4: data += b1; data+=6"),
    0x06: ("loop_push", 4, "push retaddr=data+4, counter=w2; data+=4"),
    0x07: ("loop", 2, "counters[N]--; if<1: N--,data+=2 else data=retaddr[N-1],yield"),
    0x08: ("activate", 4, "activate entry (w&0xff) with script at table[(w>>6)&~3]; data+=4"),
    0x09: ("clear_entry", 2, "clear entry (target = byte after opcode); data+=2"),
    0x0A: ("bytevar_set", 4, "byteVar[b1] = b2; data+=4"),
    0x0B: ("bytevar_add", 4, "byteVar[b1] += (signed char)b2; data+=4"),
    0x0C: ("shortvar_set", 4, "shortVar[b1] = w2; data+=4"),
    0x0D: ("shortvar_add", 4, "shortVar[b1] += (short)w2; data+=4"),
    0x0E: ("fade_in", 6, "fade: type=b1, counter=-(w2*0x80), state=w4; data+=6"),
    0x0F: ("fade_out", 6, "fade: type=b1, counter=+(w2<<7), state=w4; data+=6"),
    0x10: ("order_setup", 6, "order[b1]: owner=b3(0xff=none), flags=w4, model=b2; data+=6"),
    0x11: ("cam_matrix", 14, "camera matrix from 6 shorts << (b1&0x1f); data+=0xe"),
    0x12: ("delta_load", 14, "delta buffer = 6 shorts; data+=0xe"),
    0x13: ("matrix_add_delta", 2, "camera matrix += delta; data+=2"),
    0x14: ("delta_add", 4, "delta[b2] += (char)b3; data+=4"),
    0x15: ("order_pos_set", 8, "order[b1] pos = 3 shorts; data+=8"),
    0x16: ("order_vel_set", 8, "order[b1] vel = 3 shorts; data+=8"),
    0x17: ("order_pos_add_vel", 2, "order[b1] pos += vel; data+=2"),
    0x18: ("order_vel_add", 4, "order[b1] vel[sub b2] += (char)b3; data+=4"),
    0x19: ("order_rot_set", 8, "order[b1] rot = 3 shorts; data+=8"),
    0x1A: ("order_rotvel_set", 6, "order[b1] rotvel = 3 chars; data+=6"),
    0x1B: ("order_rot_add_vel", 2, "order[b1] rot += rotvel; data+=2"),
    0x1C: ("order_rotvel_add", 4, "order[b1] rotvel[sub b2] += (char)b3; data+=4"),
    0x1D: ("order_vert_set", 10, "order[b1] vert[b2] = 3 shorts; data+=10"),
    0x1E: ("order_vert_add", 10, "order[b1] vert[b2] += 3 shorts; data+=10"),
    0x1F: ("message", 4, "set_message_display(b1, w2); data+=4"),
    0x20: ("sfx", 4, "play_sfx(b1,b2,b3); data+=4"),
    0x21: ("order_flags", 4, "if order[b1] active: flags=w2; data+=4"),
    0x22: ("order_tpage", 4, "order[w2&0xff] tpage = w2>>8; data+=4"),
    0x23: ("buffer_advance", 4, "imgbuf += w2; data+=4; yield"),
    0x24: ("disable_dispatch", 2, "state &= ~2; data+=2"),
    0x25: ("clear_flags2", 2, "flags2 &= 0xff7fffff; data+=2"),
}


def parse_script(data, base, end, label, indent=0):
    """Disassemble a script block from `base` to `end`."""
    p = base
    out = []
    guard = 0
    while p < end and guard < 4000:
        guard += 1
        op = data[p]
        if op not in OPS:
            out.append("%08X: OP 0x%02X  <-- UNKNOWN OPCODE" % (p, op))
            return out, p
        name, size, desc = OPS[op]
        raw = data[p:p + size]
        hexs = " ".join("%02X" % b for b in raw)
        if op == 0x04 or op == 0x05:
            var = raw[2]
            relop = raw[3]
            relopname = {0: "==", 1: ">", 2: ">=", 3: "<", 4: "<=", 5: "!="}.get(relop, "?")
            if op == 0x04:
                val = raw[4]
                extra = "byteVar[%d] %s %d (jmp+%d)" % (var, relopname, val, raw[1])
            else:
                val = struct.unpack_from("<h", data, p + 4)[0]
                extra = "shortVar[%d] %s %d (jmp+%d)" % (var, relopname, val, raw[1])
        elif op == 0x06:
            extra = "count=%d" % struct.unpack_from("<H", data, p + 2)[0]
        elif op == 0x08:
            w = struct.unpack_from("<H", data, p + 2)[0]
            extra = "entry=%d table=0x%X" % (w & 0xFF, (w >> 6) & ~3)
        elif op == 0x09:
            extra = "entry=%d" % raw[1]
        elif op == 0x0A:
            extra = "byteVar[%d] = %d" % (raw[1], raw[2])
        elif op == 0x0B:
            extra = "byteVar[%d] += %d" % (raw[1], struct.unpack("b", bytes([raw[2]]))[0])
        elif op == 0x0C:
            extra = "shortVar[%d] = %d" % (raw[1], struct.unpack_from("<h", data, p + 2)[0])
        elif op == 0x0D:
            extra = "shortVar[%d] += %d" % (raw[1], struct.unpack_from("<h", data, p + 2)[0])
        elif op == 0x0E or op == 0x0F:
            w2 = struct.unpack_from("<H", data, p + 2)[0]
            w4 = struct.unpack_from("<H", data, p + 4)[0]
            cnt = -(w2 * 0x80) if op == 0x0E else (w2 << 7)
            extra = "type=%d counter=%d state=%04X" % (raw[1], cnt, w4)
        elif op == 0x10:
            extra = "order=%d model=%d owner=%s flags=%04X" % (
                raw[1], raw[2], "none" if raw[3] == 0xFF else str(raw[3]),
                struct.unpack_from("<H", data, p + 4)[0])
        elif op == 0x11 or op == 0x12:
            sh = struct.unpack_from("<6h", data, p + 2)
            extra = "shift=%d shorts=(%s)" % (raw[1] & 0x1f, ",".join(str(s) for s in sh))
        elif op == 0x13:
            extra = "matrix += delta"
        elif op == 0x14:
            extra = "delta[%d] += %d" % (raw[2], struct.unpack("b", bytes([raw[3]]))[0])
        elif op == 0x15 or op == 0x19:
            sh = struct.unpack_from("<3h", data, p + 2)
            extra = "order=%d (%s)" % (raw[1], ",".join(str(s) for s in sh))
        elif op == 0x16:
            sh = struct.unpack_from("<3h", data, p + 2)
            extra = "order=%d (%s)" % (raw[1], ",".join(str(s) for s in sh))
        elif op == 0x17 or op == 0x1B:
            extra = "order=%d" % raw[1]
        elif op == 0x18 or op == 0x1C:
            extra = "order=%d sub=%d += %d" % (raw[1], raw[2],
                                               struct.unpack("b", bytes([raw[3]]))[0])
        elif op == 0x1A:
            extra = "order=%d (%d,%d,%d)" % (raw[1],
                                             struct.unpack("b", bytes([raw[2]]))[0],
                                             struct.unpack("b", bytes([raw[3]]))[0],
                                             struct.unpack("b", bytes([raw[4]]))[0])
        elif op == 0x1D or op == 0x1E:
            sh = struct.unpack_from("<3h", data, p + 4)
            extra = "order=%d vert=%d (%s)" % (raw[1], raw[2], ",".join(str(s) for s in sh))
        elif op == 0x1F:
            extra = "msg=%d flags=%04X" % (raw[1], struct.unpack_from("<H", data, p + 2)[0])
        elif op == 0x20:
            extra = "sfx(%d,%d,%d)" % (raw[1], raw[2], raw[3])
        elif op == 0x21:
            extra = "order=%d flags=%04X" % (raw[1], struct.unpack_from("<H", data, p + 2)[0])
        elif op == 0x22:
            w2 = struct.unpack_from("<H", data, p + 2)[0]
            extra = "order=%d tpage=%d" % (w2 & 0xFF, w2 >> 8)
        elif op == 0x23:
            extra = "buf += %d" % struct.unpack_from("<H", data, p + 2)[0]
        out.append("%08X  %-16s %-46s | %s" % (p, name, hexs, extra))
        p += size
    return out, p


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "DOOR00.DOR"
    with open(path, "rb") as f:
        data = f.read()

    print("=== %s (%d bytes) ===" % (path, len(data)))
    dword0, dword1, dword2 = struct.unpack_from("<3I", data, 0)
    print("header: dword0=0x%X (table offset) dword1=0x%X (anim data) dword2=0x%X (texture)" %
          (dword0, dword1, dword2))

    # Model table at 0x0C
    entries = []
    p = 0x0C
    while True:
        v = struct.unpack_from("<I", data, p)[0]
        if v == 0:
            break
        entries.append(v)
        p += 4
    print("model table: %d entries" % len(entries))
    for i, v in enumerate(entries):
        print("  [%2d] 0x%05X -> file 0x%05X" % (i, v, v + 0x0C))

    # Script region: between end of table and anim data
    table_end = 0x0C + (len(entries) + 1) * 4
    print("\n=== script blocks (table entries 1..%d, file %04X..%04X) ===" %
          (len(entries) - 1, table_end, dword1))
    for i in range(1, len(entries)):
        start = entries[i] + 0x0C
        end = entries[i + 1] + 0x0C if i + 1 < len(entries) else dword1
        lines, endp = parse_script(data, start, end, i)
        print("--- block %d @ 0x%04X (len %d) ---" % (i, start, end - start))
        for l in lines:
            print("  " + l)

    # Anim data
    print("\n=== anim data @ 0x%04X ===" % dword1)
    flag = data[dword1]
    count = struct.unpack_from("<I", data, dword1 + 4)[0]
    print("resolve flag=%d count=%d" % (flag, count))
    for i in range(min(count, 20)):
        base = dword1 + 8 + i * 0x1C
        a, b, c = struct.unpack_from("<3I", data, base)
        print("  anim[%2d] @0x%04X: [+0]=0x%05X->0x%05X  [+8]=0x%05X->0x%05X  [+0x10]=0x%05X->0x%05X" %
              (i, base, a, base + a, b, base + b, c, base + c))

    # Vertex buffer (table entry 0)
    vstart = entries[0] + 0x0C
    print("\n=== table[0] region @ 0x%04X (first 96 bytes as shorts) ===" % vstart)
    for off in range(0, min(96, len(data) - vstart), 16):
        sh = struct.unpack_from("<8h", data, vstart + off)
        print("  %04X: %s" % (vstart + off, " ".join("%6d" % s for s in sh)))


if __name__ == "__main__":
    main()
