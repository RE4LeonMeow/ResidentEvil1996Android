# dor_disasm.py - disassemble the .dor door-animation scripts (assets/USA/Item_m1/*.DOR)
#
#   python tools/dor_disasm.py assets/USA/Item_m1/DOOR00.DOR
#
# Prints the relocated script table, the TMD object table, and every script
# decoded with the 38-opcode interpreter implemented in src/game/DoorSystem.cpp.
#
# IF_BYTE / IF_SHORT branch when the comparison FAILS (0x004439e0 /
# 0x00443a60), so read `if var == N -> XXXX` as "skip the following block
# unless var == N". Byte var 0 = door record+0x08 (which script pair runs:
# 0..3 single door, 6..9 double door, ...), byte var 3 = record+0x0B & 0x3F
# (which handle/knob model). Those are the only two vars any shipped .dor
# tests.
import struct, sys, os

LEN = {
 0x00:2, 0x01:2, 0x02:2, 0x03:2, 0x04:6, 0x05:6, 0x06:4, 0x07:2,
 0x08:4, 0x09:2, 0x0A:4, 0x0B:4, 0x0C:4, 0x0D:4, 0x0E:6, 0x0F:6,
 0x10:6, 0x11:0xE, 0x12:0xE, 0x13:2, 0x14:4, 0x15:8, 0x16:8, 0x17:2,
 0x18:4, 0x19:8, 0x1A:6, 0x1B:2, 0x1C:4, 0x1D:10, 0x1E:10, 0x1F:4,
 0x20:4, 0x21:4, 0x22:4, 0x23:4, 0x24:2, 0x25:2,
}
NAME = {
 0x00:'END', 0x01:'CLEAR_SELF', 0x02:'WAIT_FREE', 0x03:'YIELD',
 0x04:'IF_BYTE', 0x05:'IF_SHORT', 0x06:'LOOP_PUSH', 0x07:'LOOP',
 0x08:'ACTIVATE', 0x09:'CLEAR_ENTRY', 0x0A:'BSET', 0x0B:'BADD',
 0x0C:'SSET', 0x0D:'SADD', 0x0E:'FADE_IN', 0x0F:'FADE_OUT',
 0x10:'ORDER_SETUP', 0x11:'CAM_MATRIX', 0x12:'DELTA_LOAD', 0x13:'MAT_ADD_DELTA',
 0x14:'DELTA_ADD', 0x15:'ORDER_POS', 0x16:'ORDER_VEL', 0x17:'POS+=VEL',
 0x18:'VEL_ADD', 0x19:'ORDER_ROT', 0x1A:'ORDER_ROTVEL', 0x1B:'ROT+=RVEL',
 0x1C:'ROTVEL_ADD', 0x1D:'VERT_SET', 0x1E:'VERT_ADD', 0x1F:'MESSAGE',
 0x20:'SFX', 0x21:'ORDER_FLAGS', 0x22:'ORDER_TPAGE', 0x23:'BUF_ADV',
 0x24:'DISABLE_DISPATCH', 0x25:'CLR_FLAGS2',
}
# relop index -> the comparison that makes the branch NOT taken
RELOP=['==','>','>=','<','<=','!=','(uncond)']

def s16(b,o): return struct.unpack_from('<h',b,o)[0]
def u16(b,o): return struct.unpack_from('<H',b,o)[0]
def s8(v): return v-256 if v>127 else v

def dis(data, scr_base, off, tbl, limit=4000):
    out=[]
    p=off
    end=off+limit
    while p < len(data) and p < end:
        op=data[p]
        if op>=0x26:
            out.append('  %04X: ??? %02X' % (p,op)); break
        n=LEN[op]
        b=data[p:p+n]
        txt='%-16s %s' % (NAME[op], ' '.join('%02X'%x for x in b[1:]))
        extra=''
        if op==0x04:
            extra=' ; if byteVar%d %s %d -> +%d (=%04X) else %04X' % (b[2], RELOP[min(b[3],6)], b[4], b[1], p+6+b[1], p+6)
        elif op==0x05:
            extra=' ; if shortVar %s %d -> %04X else %04X' % (RELOP[min(b[3],6)], s16(b,4), p+6+b[1], p+6)
        elif op==0x08:
            w=u16(b,2); idx=w&0xFF; toff=(w>>6)&~3
            tgt = tbl[toff//4] if toff//4 < len(tbl) else -1
            extra=' ; cmd[%d] = script[%d] (tblOff=%02X -> %04X)' % (idx, toff//4, toff, tgt)
        elif op==0x09:
            extra=' ; cmd[%d] cleared' % b[1]
        elif op==0x10:
            extra=' ; order[%d] model=%d parent=%s flags=%04X' % (b[1], b[2], ('none' if b[3]==0xFF else str(b[3])), u16(b,4))
        elif op==0x15:
            extra=' ; order[%d] pos=(%d,%d,%d)' % (b[1], s16(b,2), s16(b,4), s16(b,6))
        elif op==0x16:
            extra=' ; order[%d] vel=(%d,%d,%d)' % (b[1], s16(b,2), s16(b,4), s16(b,6))
        elif op==0x19:
            extra=' ; order[%d] rot=(%d,%d,%d)' % (b[1], s16(b,2), s16(b,4), s16(b,6))
        elif op==0x1A:
            extra=' ; order[%d] rotvel=(%d,%d,%d)' % (b[1], s8(b[2]), s8(b[3]), s8(b[4]))
        elif op in (0x17,0x1B):
            extra=' ; order[%d]' % b[1]
        elif op==0x21:
            extra=' ; order[%d].flags = %04X' % (b[1], u16(b,2))
        elif op==0x22:
            w=u16(b,2); extra=' ; order[%d] tpage=%02X' % (w&0xff, w>>8)
        elif op==0x06:
            extra=' ; loop %d times' % u16(b,2)
        elif op==0x1E:
            extra=' ; order[%d] vert[%d] += (%d,%d,%d)' % (b[1], b[2], s16(b,4), s16(b,6), s16(b,8))
        elif op==0x1D:
            extra=' ; order[%d] field7C+%d = (%d,%d,%d)' % (b[1], b[2]*8, s16(b,4), s16(b,6), s16(b,8))
        elif op==0x11:
            extra=' ; shift=%d cam=(%s)' % (b[1]&0x1f, ','.join(str(s16(b,2+i*2)) for i in range(6)))
        elif op==0x12:
            extra=' ; delta=(%s)' % ','.join(str(s16(b,2+i*2)) for i in range(6))
        elif op==0x20:
            extra=' ; sfx(%d,%d,%d)' % (b[1],b[2],b[3])
        out.append('  %04X: %s%s' % (p, txt, extra))
        p+=n
    return out

def main(path):
    data=bytearray(open(path,'rb').read())
    h=struct.unpack_from('<3I', data, 0)
    print('=== %s  size=%d  hdr: scriptTbl=%08X tmd=%08X tim=%08X' % (os.path.basename(path), len(data), h[0],h[1],h[2]))
    base=h[0]
    tbl=[]
    p=base
    while True:
        v=struct.unpack_from('<i',data,p)[0]
        if v==0: break
        tbl.append(v+base)
        p+=4
    print('script table (%d entries, base %04X):' % (len(tbl), base))
    for i,t in enumerate(tbl):
        print('   [%2d] off=%04X' % (i,t))
    # tmd header
    t=h[1]
    print('TMD hdr: id=%08X flags=%08X nobj=%d' % struct.unpack_from('<3I',data,t))
    nobj=struct.unpack_from('<I',data,t+8)[0]
    for i in range(min(nobj,16)):
        e=struct.unpack_from('<7I',data,t+12+i*28)
        print('   obj%2d vert=%06X nv=%3d norm=%06X nn=%3d prim=%06X np=%3d scale=%d' % (i,e[0],e[1],e[2],e[3],e[4],e[5],e[6]))
    ends = tbl[1:]+[h[1]]
    for i,t2 in enumerate(tbl):
        print('--- script[%d] @ %04X ---' % (i,t2))
        for l in dis(data, base, t2, tbl, ends[i]-t2):
            print(l)

for f in sys.argv[1:]:
    main(f)
