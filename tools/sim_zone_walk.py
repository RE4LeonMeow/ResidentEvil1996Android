# Simulate the zone-graph walkers (0x0045fdb0 ccw / 0x0045fae0 cw) with the
# ORIGINAL's exact memory model, over the real RDT zone tables:
#   - the candidate scan aliases zone indices through `1 << (z & 0x1f)` and
#     runs past the zone count (z = 34, 224+b, ...) - flags and midpoints for
#     z >= count read the raw RDT bytes past the zone table,
#   - the scratch arrays are the original's sizes (idx/dir 16, best 12,
#     step/prev 2) and index past the end hits the next array,
#   - the step counter is a SIGNED CHAR and wraps at 127.
# Also runs the BOUNDED port fix (scan limited to valid zone indices 0..count-1,
# int step) and compares the goals found on the valid cases.
import struct, glob

def load_rdt(path):
    data = open(path, 'rb').read()
    if len(data) < 0x5C: return None, None
    off = struct.unpack_from('<I', data, 0x58)[0]
    if off + 1 > len(data): return None, None
    count = data[off]
    zones = []
    for i in range(count):
        e = off + 2 + i * 0xC
        zones.append(struct.unpack_from('<6H', data, e))
    return zones, (data, off)

def flags_at(ctx, idx):
    """Flags of zone `idx`; for idx >= count read the raw RDT bytes (the
    original walks off the table into whatever follows it)."""
    data, off = ctx
    base = off + 0xC + idx * 0xC
    if base + 2 <= len(data):
        return struct.unpack_from('<H', data, base)[0]
    return 0

def midpoint_at(ctx, a, b):
    """FUN_004602b0 semantics with raw-byte reads for out-of-range zones."""
    data, off = ctx
    def entry(i):
        base = off + 2 + i * 0xC
        if base + 12 <= len(data):
            return struct.unpack_from('<6H', data, base)
        return (0, 0, 0, 0, 0, 0)
    ea = entry(a); eb = entry(b)
    if eb[2] == ea[0]:
        return ea[0], (max(ea[1], eb[1]) + min(ea[3], eb[3])) >> 1
    if ea[2] == eb[0]:
        return eb[0], (max(ea[1], eb[1]) + min(ea[3], eb[3])) >> 1
    dz = None
    if eb[3] == ea[1]: dz = ea[1]
    elif ea[3] == eb[1]: dz = eb[1]
    return ((max(ea[0], eb[0]) + min(ea[2], eb[2])) >> 1), dz

def sq_dist(ax, az, bx, bz):
    d = (ax - bx) ** 2 + (az - bz) ** 2
    return int(d ** 0.5)

# ------------------------------------------------------------- original model --
# One 37-byte block at 0xbe0edf..0xbe0f03, byte offsets exactly as the
# original addresses them (the arrays OVERLAP - dir starts one byte into idx,
# best starts one byte into dir, and the short pairs sit at the tail):
#   idx(i)   = byte at 0xbe0ee0 + i   = mem[1 + i]
#   dir(i)   = byte at 0xbe0ee1 + i   = mem[2 + i]
#   best(i)  = byte at 0xbe0ef0 + i   = mem[16 + i]
#   stepX(i) = short at 0xbe0efc + i*4 = mem[29 + i*4] (low byte)
#   stepZ(i) = short at 0xbe0efe + i*4 = mem[31 + i*4]
#   prevX(i) = short at 0xbe0f00 + i*4 = mem[33 + i*4]
#   prevZ(i) = short at 0xbe0f02 + i*4 = mem[35 + i*4]
# Reads past the block are clamped to 0 (the original reads its neighbours;
# the port's neighbours differ, so this is the honest upper bound on what the
# original could read).
def _char(v):
    """Signed char wrap, like the original's local_9 (a char)."""
    return ((v + 128) & 0xFF) - 128

def orig_walk(ctx, zones, count, variant, startZone, targetZone, targetX, targetZ):
    mem = [0] * 37
    def rd(j):
        if j < 0 or j >= len(mem): return 0
        return mem[j]
    def wr(j, v):
        if 0 <= j < len(mem): mem[j] = v & 0xFF
    def idx(i):  return rd(1 + i)
    def dir_(i): return rd(2 + i)
    def best(i): return rd(16 + i)
    def s16(lo, hi):
        v = rd(lo) | (rd(hi) << 8)
        return v - 0x10000 if v & 0x8000 else v
    def stepX(i): return s16(29 + i * 4, 30 + i * 4)
    def stepZ(i): return s16(31 + i * 4, 32 + i * 4)
    def prevX(i): return s16(33 + i * 4, 34 + i * 4)
    def prevZ(i): return s16(35 + i * 4, 36 + i * 4)
    def set_idx(i, v):  wr(1 + i, v)
    def set_dir(i, v):  wr(2 + i, v)
    def set_best(i, v): wr(16 + i, v)
    def set_sx(i, v):   wr(29 + i * 4, v & 0xFF); wr(30 + i * 4, (v >> 8) & 0xFF)
    def set_sz(i, v):   wr(31 + i * 4, v & 0xFF); wr(32 + i * 4, (v >> 8) & 0xFF)
    def set_px(i, v):   wr(33 + i * 4, v & 0xFF); wr(34 + i * 4, (v >> 8) & 0xFF)
    def set_pz(i, v):   wr(35 + i * 4, v & 0xFF); wr(36 + i * 4, (v >> 8) & 0xFF)
    set_idx(0, startZone); set_best(0, startZone)
    set_dir(0, count if variant == 'ccw' else 0xFF)
    # NOTE: idx(1) aliases dir(0) - the goal branch writes dir(i) = target
    # BEFORE recording best(1) = idx(1), which is how the direct-adjacency
    # case (goal at step 0) reads the target zone as the first step.
    step = 0
    bestd = 0xFFFFFFFF
    dist = 0
    iters = 0
    MAXIT = 100000
    while True:
        iters += 1
        if iters > MAXIT: return ('HANG', iters)
        i = step               # signed char, already wrapped
        flags = flags_at(ctx, idx(i))
        goal = (1 << (targetZone & 0x1f)) & flags
        if goal:
            set_dir(i, targetZone)
            disp, dz = midpoint_at(ctx, idx(i), targetZone)
            if dz is None: dz = 0
            dist += sq_dist(prevX(i), prevZ(i), disp, dz)
            dist += sq_dist(disp, dz, targetX, targetZ)
            if dist < bestd:
                n = step + 1
                while n != 0:
                    set_best(n, idx(n))
                    bestd = dist
                    n -= 1
            if step != 0:
                dist -= sq_dist(disp, dz, targetX, targetZ)
                dist -= sq_dist(prevX(i), prevZ(i), disp, dz)
                dist -= sq_dist(stepX(i), stepZ(i), prevX(i), prevZ(i))
            step = _char(step - 1)
            if step < 0:
                return (best(1) if bestd != 0xFFFFFFFF else 0xFF, iters)
            continue
        if variant == 'cw':
            dead = (flags & ~((1 << ((dir_(i) + 1) & 0x1f)) - 1)) == 0
        else:
            dead = (flags & ((1 << (dir_(i) & 0x1f)) - 1)) == 0
        if dead:
            if step != 0:
                dist -= sq_dist(stepX(i), stepZ(i), prevX(i), prevZ(i))
            step = _char(step - 1)
            if step < 0:
                return (best(1) if bestd != 0xFFFFFFFF else 0xFF, iters)
            continue
        newLen = step + 1
        step = _char(newLen)
        scan = 0
        while True:
            scan += 1
            if scan > 600: return ('HANG', iters)
            z = (idx(newLen) + (1 if variant == 'cw' else -1)) & 0xFF
            bit = 1 << (z & 0x1f)
            set_idx(newLen, z)
            if flags & bit: break
        rep = any(idx(b) == z for b in range(newLen - 1, -1, -1))
        if rep:
            step = _char(step - 1)
            if step < 0:
                return (best(1) if bestd != 0xFFFFFFFF else 0xFF, iters)
            continue
        disp, dz = midpoint_at(ctx, idx(newLen), idx(newLen - 1))
        if dz is None: dz = 0
        d = sq_dist(stepX(newLen), stepZ(newLen), disp, dz)
        dist += d
        if bestd <= dist:
            dist -= d
            step = _char(step - 1)
            if step < 0:
                return (best(1) if bestd != 0xFFFFFFFF else 0xFF, iters)
            continue
        if variant == 'cw':
            set_sx(newLen, disp); set_sz(newLen, dz)
            set_dir(newLen, 0xFF)
        else:
            set_px(newLen, disp); set_pz(newLen, dz)
            set_dir(newLen, count)

# ------------------------------------------------------------- bounded port ---
# The fix: scan limited to valid zone indices (0..count-1); int step. All
# other logic identical to the decompile.
def port_walk(zones, count, variant, targetZone, targetX, targetZ, idx, dir_, best, stepX, stepZ, prevX, prevZ):
    step = 0
    bestd = 0xFFFFFFFF
    dist = 0
    iters = 0
    MAXIT = 200000
    if variant == 'ccw':
        # fresh-position marker, mirrors zone_walk_ccw in EntityCommon.cpp
        for k in range(1, 0x10):
            idx[k] = count
    while True:
        iters += 1
        if iters > MAXIT: return ('HANG', iters)
        i = step
        if i < 0 or i >= count + 2: return ('HANG', iters)
        flags = zones[idx[i]][5]
        if (1 << (targetZone & 0x1f)) & flags:
            dir_[i] = targetZone
            ea = zones[idx[i]]; eb = zones[targetZone]
            if eb[2] == ea[0]:
                disp = ea[0]; dz = (max(ea[1], eb[1]) + min(ea[3], eb[3])) >> 1
            elif ea[2] == eb[0]:
                disp = eb[0]; dz = (max(ea[1], eb[1]) + min(ea[3], eb[3])) >> 1
            else:
                dz = ea[1] if eb[3] == ea[1] else (eb[1] if ea[3] == eb[1] else 0)
                disp = (max(ea[0], eb[0]) + min(ea[2], eb[2])) >> 1
            dist += sq_dist(prevX[i], prevZ[i], disp, dz)
            dist += sq_dist(disp, dz, targetX, targetZ)
            if dist < bestd:
                n = step + 1
                while n != 0:
                    # mirrors the aliasing repair in EntityCommon.cpp: the
                    # original's idx[1] IS dir[0] (just written = target), so
                    # the goal step's first step is the target zone itself
                    best[n] = targetZone if n == step + 1 else idx[n]
                    bestd = dist
                    n -= 1
            if step != 0:
                dist -= sq_dist(disp, dz, targetX, targetZ)
                dist -= sq_dist(prevX[i], prevZ[i], disp, dz)
                dist -= sq_dist(stepX[i], stepZ[i], prevX[i], prevZ[i])
            step -= 1
            if step < 0:
                return (best[1] if bestd != 0xFFFFFFFF else 0xFF, iters)
            continue
        if variant == 'cw':
            dead = (flags & ~((1 << ((dir_[i] + 1) & 0x1f)) - 1)) == 0
        else:
            dead = (flags & ((1 << (dir_[i] & 0x1f)) - 1)) == 0
        if dead:
            if step != 0:
                dist -= sq_dist(stepX[i], stepZ[i], prevX[i], prevZ[i])
            step -= 1
            if step < 0:
                return (best[1] if bestd != 0xFFFFFFFF else 0xFF, iters)
            continue
        newLen = step + 1
        if newLen >= 16:
            return (best[1] if bestd != 0xFFFFFFFF else 0xFF, iters)
        step = newLen
        # Probe the valid candidate zones in order - ascending for cw (starting
        # at idx+1, so zone 0 is skipped exactly like the original's scan),
        # descending for ccw (starting at idx-1, where the fresh marker `count`
        # lands on the top zone count-1). A probe that leaves 0..count-1 means
        # every candidate at this position has been tried: dead end. The scan
        # never wraps, so a backtrack can never re-probe a consumed candidate.
        matched = False
        while True:
            z = (idx[newLen] + (1 if variant == 'cw' else -1)) & 0xFF
            if z >= count:
                break                     # past the last valid zone
            bit = 1 << (z & 0x1f)
            idx[newLen] = z
            if flags & bit:
                matched = True
                break
        if not matched:
            # Exhausted: no candidate at this step. Dead end - backtrack past
            # it (step = newLen-1 is the step whose extension failed).
            if i != 0:
                dist -= sq_dist(stepX[i], stepZ[i], prevX[i], prevZ[i])
            step = newLen - 2
            if step < 0:
                return (best[1] if bestd != 0xFFFFFFFF else 0xFF, iters)
            continue
        rep = any(idx[b] == z for b in range(newLen - 1, -1, -1))
        if rep:
            step -= 1
            if step < 0:
                return (best[1] if bestd != 0xFFFFFFFF else 0xFF, iters)
            continue
        ea = zones[idx[newLen]]; eb = zones[idx[newLen - 1]]
        if eb[2] == ea[0]:
            disp = ea[0]; dz = (max(ea[1], eb[1]) + min(ea[3], eb[3])) >> 1
        elif ea[2] == eb[0]:
            disp = eb[0]; dz = (max(ea[1], eb[1]) + min(ea[3], eb[3])) >> 1
        else:
            dz = ea[1] if eb[3] == ea[1] else (eb[1] if ea[3] == eb[1] else 0)
            disp = (max(ea[0], eb[0]) + min(ea[2], eb[2])) >> 1
        d = sq_dist(stepX[newLen], stepZ[newLen], disp, dz)
        dist += d
        if bestd <= dist:
            dist -= d
            step -= 1
            if step < 0:
                return (best[1] if bestd != 0xFFFFFFFF else 0xFF, iters)
            continue
        if variant == 'cw':
            stepX[newLen] = disp; stepZ[newLen] = dz
            dir_[newLen] = 0xFF
        else:
            prevX[newLen] = disp; prevZ[newLen] = dz
            dir_[newLen] = count

def main():
    files = sorted(glob.glob('assets/USA/Stage1/ROOM*.RDT'))
    o_hang = 0; p_hang = 0; o_goal = 0; p_goal = 0; same_goal = 0; total = 0
    for f in files:
        zones, ctx = load_rdt(f)
        if zones is None or len(zones) < 2: continue
        count = len(zones)
        for variant in ('cw', 'ccw'):
            for start in range(count):
                for target in range(count):
                    if start == target: continue
                    total += 1
                    # target at the target zone's center, like the SCD path
                    tx = (zones[target][0] + zones[target][2]) >> 1
                    tz = (zones[target][1] + zones[target][3]) >> 1
                    og = orig_walk(ctx, zones, count, variant, start, target, tx, tz)
                    idx = [0]*64; dir_ = [0]*64; best = [0]*64
                    sx = [0]*64; sz = [0]*64; px = [0]*64; pz = [0]*64
                    idx[0] = start; best[0] = start
                    dir_[0] = count if variant == 'ccw' else 0xFF
                    pg = port_walk(zones, count, variant, target, tx, tz,
                                   idx, dir_, best, sx, sz, px, pz)
                    if og[0] == 'HANG': o_hang += 1
                    if pg[0] == 'HANG':
                        p_hang += 1
                        print(f'  PORT HANG: {f} {variant} start={start} target={target}')
                    if og[0] != 0xFF and og[0] != 'HANG': o_goal += 1
                    if pg[0] != 0xFF and pg[0] != 'HANG':
                        p_goal += 1
                        if og[0] == pg[0]: same_goal += 1
    print(f'rooms: {len(files)}')
    print(f'pairs: {total}')
    print(f'original: hangs={o_hang} goals={o_goal}')
    print(f'port:     hangs={p_hang} goals={p_goal} (same first-step as original: {same_goal})')

if __name__ == "__main__":
    main()
