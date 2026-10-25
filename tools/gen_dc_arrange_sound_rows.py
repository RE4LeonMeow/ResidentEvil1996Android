#!/usr/bin/env python3
"""gen_dc_arrange_sound_rows.py - the DC arrange rooms' enemy sound banks.

WHY THERE IS A TABLE AT ALL
---------------------------
An arrange (ADVANCED) room is the same physical room as its base stage's, so the
port indexes every per-room table at the BASE row - `Room_LoadEnemySoundBanks`
reads `g_RoomSoundNameTable[stage_data_row() * 29 + roomId]` on purpose. That is
right for everything the room shares and wrong for everything ADVANCED changed:
where the DC swapped the enemy, those slots still hold the base room's animal,
and where the base room had no enemy at all they hold nothing.

WHERE THE ANSWER COMES FROM - the RDT's own bank, not a guess
-------------------------------------------------------------
A PS1/DC RDT carries its room's sound bank inside itself:

    RDT+0x88   48 x 4 bytes: byte[1] = VAB program, byte[2] = tone in it
    RDT+0x8C   VAB header ('pBAV'): 32 + 128*16 progAtr + ps*16*32 vagAtr
               + 256 u16 VAG sizes (/8)
    RDT+0x90   VAB body: the VAGs, packed in index order

so slot -> tone -> VAG -> a byte range of the body. The PC engine ignores all
three blocks (it plays .wav), but they are the DC's own answer to "what does
this room's slot N hold", and the arrange RDTs are on the disc.

Naming a slot is then a lookup rather than a guess: hash the VAG bytes and find
the same bytes in a BASE room, whose PC row DOES name that slot. The identity of
a sound is (VAG hash, note), not the hash alone - the PS1 plays one VAG at
several notes and the PC pre-rendered each pitch as its own file, which is why
cer_taoA and cer_taoB, ft_wdA and ft_wdB, are one VAG each.

Three things this turned up that a per-enemy group table cannot express:

  * room 212's ADVANCED zombie is the z_osou / z_unaruA / z_Hkick / z_Ugoron
    variant, NOT the z_k01..z_k03 one. There is no single "the zombie group" -
    the base rooms carry at least four variants and the arrange rooms pick
    among them per room.
  * room 718's crows use EIGHT slots: RVpatA and RVpatB sit at 6 and 7, not
    only the six RVcar/RVwing/RVfryed.
  * some slots must be CLEARED. Room 212 drops the crows' RVpatA/RVpatB at
    24/25 and room 71A's hunter has two fewer sounds than the zombie it
    replaces, and those tones point past their program's tone count - the DC's
    way of saying "empty".

A slot whose 4-byte record is all zero is indistinguishable from program 0 /
tone 0, which is a real sound (slot 0's). Such a slot is UNUSED; the PC row's
NULLs are the authority for the base rooms, and for the arrange rooms a slot is
only overridden when its resolved sound differs from the base room's.

    python tools/gen_dc_arrange_sound_rows.py            # report only
    python tools/gen_dc_arrange_sound_rows.py --write    # emit the C table
"""
import argparse
import collections
import hashlib
import os
import re
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DC = os.path.join(REPO, 'assets', 'DC')
SNDTABLE = os.path.join(REPO, 'src', 'game', 'SoundTables.cpp')
OUT_C = os.path.join(REPO, 'src', 'game', 'dc', 'ArrangeSoundRows.cpp')

ARRSTAGES = os.path.join(REPO, 'src', 'game', 'dc', 'ArrangeStages.cpp')

# ArrangeStages.cpp's g_dcArrangeRooms, by base stage. check_arrange() re-reads
# that table and fails if this copy has drifted from it.
ARRANGE = {
    0: [0x01, 0x07, 0x0B, 0x0D, 0x12, 0x13, 0x1A, 0x1C],
    1: [0x01, 0x02, 0x03, 0x04, 0x12],
    2: [0x00, 0x0F],
    3: [0x00, 0x04, 0x05],
    4: [0x04, 0x05, 0x07, 0x11],
    5: [0x01, 0x04, 0x07, 0x0B, 0x0D, 0x12, 0x13, 0x1A, 0x1C],
    6: [0x01, 0x02, 0x03, 0x04, 0x12, 0x18, 0x1A],
}

# ---------------------------------------------------------------------------
# Slots whose VAG differs from the base room's but whose SOUND does not: the
# arrange RDT re-encoded the same sample. Room 507's whole bank is stored at
# half the base room's byte length (15760 -> 7904, 16384 -> 8208, ...) and room
# 505's chimera group was re-authored sample by sample while the SCD still
# spawns enemy 9 with the same group. Envelope correlation between the arrange
# VAG and the BASE room's own VAG at that slot is what settles each one; the
# figure is quoted per entry. Overriding these would replace a name with itself
# at best and with a wrong guess at worst.
#
#   (stage, room): {slot: "why"}
REENCODED = {
    (4, 0x05): {s: 'chimera group re-authored, enemy id 9 unchanged'
                for s in range(10, 19)},
    (4, 0x07): {22: 'env 0.999 vs base mv_cp',
                23: 'env 0.990 vs base mv_step',
                33: 'env 0.996 vs base key_indr (a bare WAV search says '
                    '"lockef" 0.986 - the base room\'s own sample scores higher)',
                36: 'env 0.999 vs base drw_c_op',
                37: 'env 0.999 vs base drw_c_sh',
                38: 'env 0.936 vs base key_desk'},
}

# (stage, room, group) that mine_missing() must NOT fill in, with the reason.
#
# Room 60C's group 0 has exactly two filled slots, 0 and 5, and BOTH hold the
# same VAG - the one the PC calls ft_linA/ft_linB, its linoleum footstep pair.
# The room's spawns are id 15, the monster plant, and MonsterPlant.cpp plays
# Snd_em(0..4); naming slot 0 "ft_linA" would give the plant a footstep. Either
# the id-15 records are scan noise or the DC keeps something there the PC has no
# file for. Two slots and an incoherent reading is not enough to act on.
NO_PC_SOUND_SKIP = {
    (5, 0x0C, 0): 'slots 0 and 5 are both the ft_linA/ft_linB VAG - '
                  'incoherent as the monster plant group',
}

# Slots the mined bank cannot name, with the name to use and why. One entry.
MANUAL = {
    (0, 0x01, 9): ('cer_runMX',
                   'ROOM8010 puts a 1820-sample cue here that appears in no '
                   'other RDT and matches no PC WAV (best correlation 0.24). '
                   'Cerberus.cpp plays Snd_em(9) as the footfall, and slot 9 '
                   'of the cerberus group is cer_runMX in all six base rows '
                   'that carry it, so that is what the PC has for it.'),
}

VAB_HDR, PROGATR, VAGTBL = 32, 128 * 16, 512


def check_arrange():
    """ARRANGE above is a copy of g_dcArrangeRooms. Re-read the real one."""
    with open(ARRSTAGES, encoding='utf-8') as f:
        txt = f.read()
    i = txt.index('g_dcArrangeRooms[DC_ARRANGE_ROWS][DC_ARRANGE_COLS] = {')
    body = txt[i:txt.index('};', i)]
    live = {}
    for stage, row in enumerate(re.findall(r'\{([^{}]*0x[^{}]*)\}', body)):
        live[stage] = [int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})', row)
                       if v.upper() != 'FF']
    if live != ARRANGE:
        raise SystemExit('g_dcArrangeRooms has changed - update ARRANGE:\n'
                         '  ArrangeStages.cpp: %r\n'
                         '  this script:       %r' % (live, ARRANGE))


# --------------------------------------------------------------- RDT bank
def read_bank(path):
    """48 slots of (VAG hash, note), plus the raw 4-byte records."""
    with open(path, 'rb') as f:
        d = f.read()
    if len(d) < 0x94:
        return None
    a88, a8c, a90 = (struct.unpack_from('<I', d, o)[0] for o in (0x88, 0x8C, 0x90))
    if not (0 < a88 < a8c < a90 <= len(d)) or d[a8c:a8c + 4] != b'pBAV':
        return None
    ps = struct.unpack_from('<h', d, a8c + 18)[0]
    tbl = a8c + VAB_HDR + PROGATR + ps * 16 * 32
    if tbl + VAGTBL != a90:
        return None
    sizes = struct.unpack_from('<256H', d, tbl)
    off, acc = [0] * 256, 0
    for i in range(1, 256):
        off[i] = acc
        acc += sizes[i] * 8
    body = d[a90:]

    keys, recs = [], []
    for s in range(48):
        rec = tuple(d[a88 + s * 4:a88 + s * 4 + 4])
        recs.append(rec)
        prog, tone = rec[1], rec[2]
        key = None
        # A tone past its program's tone count has an all-zero VagAtr: the DC's
        # way of leaving a slot empty when the new enemy has fewer sounds.
        if prog < ps and tone < d[a8c + VAB_HDR + prog * 16]:
            ta = a8c + VAB_HDR + PROGATR + (prog * 16 + tone) * 32
            v = struct.unpack_from('<H', d, ta + 22)[0]
            if 0 < v < 256 and sizes[v]:
                blob = body[off[v]:off[v] + sizes[v] * 8]
                if len(blob) == sizes[v] * 8:
                    key = (hashlib.sha1(blob).hexdigest()[:16], d[ta + 6])
        keys.append(key)
    return {'keys': keys, 'recs': recs}


# ------------------------------------------------------- PC name table
def read_rows(path):
    """g_RoomSndData[203][48], NULLs included - a regex over the quoted names
    alone loses the NULL runs and shifts every slot after the first gap."""
    with open(path, encoding='utf-8', errors='replace') as f:
        txt = f.read()
    i = txt.index('{', txt.index('g_RoomSndData[203][48] = {') + 24)
    tok = re.compile(r'"([^"]*)"|NULL|(\{)|(\})')
    rows, cur, depth, pos = [], None, 1, i + 1
    while pos < len(txt):
        m = tok.search(txt, pos)
        if not m:
            break
        pos = m.end()
        if m.group(2):
            depth += 1
            cur = []
        elif m.group(3):
            depth -= 1
            if depth == 1 and cur is not None:
                rows.append((cur + [None] * 48)[:48])
                cur = None
            elif depth == 0:
                break
        elif cur is not None:
            cur.append(m.group(1))
    return rows


def base_path(stage, room, ch):
    return os.path.join(DC, 'Stage%d' % (stage + 1),
                        'ROOM%d%02X%d.RDT' % (stage + 1, room, ch))


def arr_path(stage, room, ch):
    st = stage + 8
    return os.path.join(DC, 'Stage%X' % st, 'ROOM%X%02X%d.RDT' % (st, room, ch))


# ------------------------------------------------------------------ naming
def build_index(rows):
    """(VAG hash, note) -> the PC names the base rooms give it, and the same
    by hash alone, and every base room's whole bank for the donor pass."""
    exact = collections.defaultdict(collections.Counter)
    byslot = collections.defaultdict(collections.Counter)
    byhash = collections.defaultdict(collections.Counter)
    banks = []
    for st in range(1, 8):
        for room in range(29):
            for ch in (0, 1):
                p = os.path.join(DC, 'Stage%d' % st, 'ROOM%d%02X%d.RDT' % (st, room, ch))
                if not os.path.exists(p):
                    continue
                b = read_bank(p)
                if not b:
                    continue
                row = rows[(st - 1) * 29 + room]
                banks.append(('%d%02X%d' % (st, room, ch), b, row))
                for s in range(48):
                    if b['keys'][s] and row[s]:
                        exact[b['keys'][s]][row[s]] += 1
                        byslot[(b['keys'][s][0], s)][row[s]] += 1
                        byhash[b['keys'][s][0]][row[s]] += 1
    return exact, byslot, byhash, banks


def donor_names(banks, arr, slots):
    """The base room that agrees with the arrange room on the most of `slots`,
    which is what tells z_sanj from z_k03 when the two are one VAG at one note
    in two different slots. A global vote cannot; one donor room's row can."""
    best = None
    for name, b, row in banks:
        hit = [s for s in slots if arr['keys'][s] and b['keys'][s] == arr['keys'][s]]
        named = sum(1 for s in hit if row[s])
        cand = (len(hit), named, name, {s: row[s] for s in hit})
        if best is None or cand[:2] > best[:2]:
            best = cand
    return best


def name_slot(key, slot, exact, byslot, byhash, donor, twin=None):
    """Strongest evidence first. The donor room comes before the global vote
    because only one room's row can tell z_sanj from z_k03 - the two are one
    VAG at one note, told apart by which slot holds it. Then the same VAG at
    the same note anywhere; then at the same SLOT anywhere, which is what
    separates an A/B pair (ze_tomo1 at slot 5, ze_tomo3 at slot 9); then the
    VAG alone."""
    if key is None:
        return None, 'empty'
    if donor and donor[0] >= 3 and donor[3].get(slot):
        return donor[3][slot], 'donor ROOM%s' % donor[2]
    if twin:
        return twin[1], 'same tone as slot %d' % twin[0]
    c = exact.get(key)
    if c and len(c) == 1:
        return c.most_common(1)[0][0], 'exact'
    c2 = byslot.get((key[0], slot))
    if c2:
        return c2.most_common(1)[0][0], 'same slot elsewhere'
    if c:
        return c.most_common(1)[0][0], 'exact (ambiguous: %s)' % '/'.join(c)
    c = byhash.get(key[0])
    if c:
        return c.most_common(1)[0][0], 'same VAG elsewhere'
    return None, 'unknown'


def enemy_groups(path):
    """Every (enemy id, sound group) the room's scripts spawn.

    A raw scan for the 0x1B signature, not a script walk: mine_room_scd's widths
    still desync on some rooms - ROOM1160's init dies on omodel_set four
    commands in - and a desync silently LOSES the spawns after it, which is
    exactly how room 116 looked enemy-free. The record is sanity-checked
    instead (enemy id, slot nibble and byte 0x15 all have narrow ranges)."""
    with open(path, 'rb') as f:
        d = f.read()
    out = []
    for off in (0x60, 0x64, 0x68):
        rel = struct.unpack_from('<I', d, off)[0]
        if not rel or rel >= len(d):
            continue
        limit = len(d)
        for h in range(0x08, 0x94, 4):
            q = struct.unpack_from('<I', d, h)[0]
            if rel < q < limit:
                limit = q
        for o in range(rel, max(rel, limit - 22)):
            if d[o] != 0x1B:
                continue
            eid, slot, b15 = d[o + 1], d[o + 0x12] & 0xF, d[o + 0x15]
            if eid < 0x40 and slot < 8 and b15 < 0x20 and (b15 & 7) < 5:
                out.append((eid, b15 & 7))
    return out


def mine_missing(rows, exact, byslot, byhash, banks, verbose=True):
    """The OTHER way a DC enemy goes silent: no arrange RDT at all.

    Room 116 (the shotgun room) has no entry in g_dcArrangeRooms, so ADVANCED
    loads the ordinary ROOM1160 - whose init spawns three id-17 zombies behind
    `04 05 2E 00`, a bit_test on main_state_flags2 bit 17 (MSF2_DC_ADVANCED)
    with cond 0, i.e. "run this when ADVANCED". The room's own RDT bank fills
    slots 0-9 with a zombie group and the PC's g_RoomSndData row names NONE of
    them, so the zombies play nothing. The arrange-vs-base diff cannot see this
    - there is no arrange file to diff against.

    A (room, group) qualifies when the room's bank fills at least TWO slots of
    the group with a real record, the PC row names none of them, and some
    enemy_set uses that group. These entries apply in ANY DC mode, not only
    ADVANCED: it is the same RDT either way, so its bank is right either way.
    """
    out = []
    for stage in range(7):
        for room in range(29):
            p = base_path(stage, room, 0)
            if not os.path.exists(p):
                continue
            bank = read_bank(p)
            if not bank:
                continue
            row = rows[stage * 29 + room]
            # Both character files, unioned. The two share one bank (asserted
            # below) but not necessarily one script: room 605's Chris file
            # spawns into group 0 and its Jill file does not, and taking only
            # room 0 dropped the whole room.
            spawns = list(enemy_groups(p))
            p1 = base_path(stage, room, 1)
            if os.path.exists(p1):
                other = read_bank(p1)
                if other and other['keys'] != bank['keys']:
                    print('  !! %d%02X: the two character files disagree on the '
                          'bank - this table has no character column' % (stage + 1, room))
                    continue
                spawns += enemy_groups(p1)
            groups = sorted({g for _, g in spawns})
            for g in groups:
                if (stage, room, g) in NO_PC_SOUND_SKIP:
                    continue
                # An all-zero record is an UNUSED slot, so a group is only real
                # when a slot other than 0 carries one. Room 40A's "group 1"
                # was a single slot with a 1-hit donor - scan noise, not a group.
                real = [s for s in range(g * 10, min(g * 10 + 10, 48))
                        if bank['keys'][s] and any(bank['recs'][s])]
                if len(real) < 2:
                    continue
                slots = sorted(set(real) |
                               ({g * 10} if bank['keys'][g * 10] else set()))
                if any(row[s] for s in slots):
                    continue            # the port already has something here
                donor = donor_names(banks, bank, slots)
                entries = []
                for s in slots:
                    twin = next((e for e in entries
                                 if bank['recs'][e[0]] == bank['recs'][s] and e[1]), None)
                    nm, why = name_slot(bank['keys'][s], s, exact, byslot,
                                        byhash, donor, twin)
                    entries.append((s, nm, why))
                ids = sorted({e for e, gg in spawns if gg == g})
                out.append((stage, room, entries, donor, g, ids))
                if verbose:
                    print('%d%02X  %s   group %d, enemy ids %s, donor ROOM%s (%d/%d)'
                          % (stage + 1, room, os.path.basename(p), g, ids,
                             donor[2], donor[0], len(slots)))
                    for s, nm, why in entries:
                        print('    slot %2d  NULL      -> %-9s  [%s]'
                              % (s, nm or 'NULL', why))
    return out


def mine(rows, exact, byslot, byhash, banks, verbose=True):
    """-> [(stage, room, [(slot, name_or_None, why), ...]), ...]"""
    out = []
    for stage in sorted(ARRANGE):
        for room in ARRANGE[stage]:
            ap = arr_path(stage, room, 0)
            if not os.path.exists(ap):
                continue
            bp = base_path(stage, room, 0)
            arr = read_bank(ap)
            base = read_bank(bp) if os.path.exists(bp) else None
            if not arr:
                print('  !! %s: no readable bank' % os.path.basename(ap))
                continue
            row = rows[stage * 29 + room]
            skip = REENCODED.get((stage, room), {})

            changed = []
            for s in range(48):
                ak = arr['keys'][s]
                bk = base['keys'][s] if base else None
                if ak == bk or s in skip:
                    continue
                # unused in both: an all-zero record is program 0 / tone 0, not
                # a sound of its own
                if not (any(arr['recs'][s]) or s == 0 or row[s] or
                        (base and any(base['recs'][s]))):
                    continue
                changed.append(s)
            if not changed:
                continue

            donor = donor_names(banks, arr, changed)
            entries = []
            for s in changed:
                man = MANUAL.get((stage, room, s))
                if man:
                    entries.append((s, man[0], 'manual'))
                    continue
                # Two slots pointing at ONE tone are one sound, so they take
                # one name - but only where a donor room has not already named
                # the slot. ROOM2030 puts slots 8 and 9 on one tone and still
                # names them z_sanj and z_unaruB, so the donor wins; room 113's
                # arrange aims slot 23 (the bathtub event cue) at the zombie
                # group's own tone 7 and no donor names slot 23, so the twin
                # does - naming it from what other rooms happen to keep at slot
                # 23 would give "kns_tetu", a four-second metal clang that only
                # shares the source VAG.
                twin = next((e for e in entries
                             if arr['recs'][e[0]] == arr['recs'][s] and e[1]), None)
                nm, why = name_slot(arr['keys'][s], s, exact, byslot,
                                    byhash, donor, twin)
                entries.append((s, nm, why))
            out.append((stage, room, entries, donor))
            if verbose:
                print('%d%02X  %s   %s'
                      % (stage + 1, room, os.path.basename(ap),
                         'donor ROOM%s (%d/%d slots)' % (donor[2], donor[0], len(changed))
                         if donor[0] else 'no donor room shares these samples'))
                for s, nm, why in entries:
                    print('    slot %2d  %-9s -> %-9s  [%s]'
                          % (s, row[s] or 'NULL', nm or 'NULL', why))
                if skip:
                    print('    (unchanged, re-encoded: %s)'
                          % ', '.join(str(s) for s in sorted(skip)))
    return out


# ----------------------------------------------------------------- emit
def emit(mined, missing, rows):
    lines = []
    for stage, room, entries, donor in mined:
        row = rows[stage * 29 + room]
        lines.append('    // room %d%02X%s' % (stage + 1, room,
                     ' - donor ROOM%s' % donor[2] if donor[0] >= 3 else ''))
        for s, nm, why in entries:
            lines.append('    { %d, 0x%02X, %2d, DC_SND_ARRANGE, %-11s },   // was %s'
                         % (stage, room, s,
                            '"%s"' % nm if nm else '0',
                            row[s] or 'nothing'))
    lines.append('')
    lines.append('    // ------------------------------------------------------------------')
    lines.append('    // No arrange RDT: the room keeps its own file in every mode and the')
    lines.append('    // PC row simply has nothing for the enemy the DC spawns there.')
    lines.append('    // ------------------------------------------------------------------')
    for stage, room, entries, donor, g, ids in missing:
        lines.append('    // room %d%02X - group %d, enemy id%s %s%s'
                     % (stage + 1, room, g, '' if len(ids) == 1 else 's',
                        ', '.join(str(i) for i in ids),
                        ' - donor ROOM%s' % donor[2] if donor[0] >= 3 else ''))
        for s, nm, why in entries:
            lines.append('    { %d, 0x%02X, %2d, DC_SND_ANY_DC,  %-11s },   // was nothing'
                         % (stage, room, s, '"%s"' % nm if nm else '0'))
    body = '\n'.join(lines)
    return '''// ArrangeSoundRows.cpp - GENERATED by tools/gen_dc_arrange_sound_rows.py.
// Do not edit by hand; edit the generator and re-run it.
//
// Where a DC room's enemy sound bank does not match what the port loads for it,
// mined from the RDT's own embedded VAB (RDT+0x88/0x8C/0x90). Two kinds:
//
//   DC_SND_ARRANGE  the arrange RDT's bank differs from the base room's, so the
//                   entry applies only while the arrange file is loaded. A NULL
//                   name CLEARS the slot - the DC really does leave it empty,
//                   because what ADVANCED puts there has fewer sounds than what
//                   it replaces.
//   DC_SND_ANY_DC   the room has no arrange RDT at all; it keeps its own file in
//                   every mode and the PC's g_RoomSndData row simply has nothing
//                   for the enemy the DC spawns in it. Same file either way, so
//                   the entry applies whenever DC mode is on.
//
// Only room 0 of each pair is mined: every room here has the same bank in both
// character variants.
#include "ArrangeSoundRows.h"

static const DcArrangeSndSlot g_dcArrangeSndSlots[] = {
%s
};

const DcArrangeSndSlot* dc_arrange_snd_slots(int* count)
{
    if (count) {
        *count = (int)(sizeof(g_dcArrangeSndSlots) / sizeof(g_dcArrangeSndSlots[0]));
    }
    return g_dcArrangeSndSlots;
}
''' % body


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--write', action='store_true')
    args = ap.parse_args()

    check_arrange()
    rows = read_rows(SNDTABLE)
    exact, byslot, byhash, banks = build_index(rows)
    print('indexed %d base-room banks, %d distinct (hash, note) sounds\n'
          % (len(banks), len(exact)))
    print('--- arrange rooms whose bank differs from the base room ---')
    mined = mine(rows, exact, byslot, byhash, banks)
    print('\n--- rooms with no arrange RDT whose enemy has no PC sounds ---')
    missing = mine_missing(rows, exact, byslot, byhash, banks)

    unknown = [(st, rm, s) for st, rm, es, _ in mined for s, nm, why in es
               if why == 'unknown']
    unknown += [(st, rm, s) for st, rm, es, _, _, _ in missing
                for s, nm, why in es if why == 'unknown']

    # An override must never land on a slot the room's own row fills with a
    # footstep: PlayEntitySnd indexes the same bank with (footstep type + the
    # room's zone offset), and some rows put ft_* inside an enemy group's ten.
    stomp = []
    for st, rm, es, _ in mined:
        for s, nm, _w in es:
            base = rows[st * 29 + rm][s]
            if base and base.lower().startswith(('ft_', 'taore_')):
                stomp.append((st, rm, s, base, nm))

    total = sum(len(es) for _, _, es, _ in mined) + \
        sum(len(es) for _, _, es, _, _, _ in missing)
    print('\n%d arrange rooms + %d missing-sound groups, %d slot overrides, '
          '%d unnamed, %d footstep collisions'
          % (len(mined), len(missing), total, len(unknown), len(stomp)))
    for u in unknown:
        print('  UNNAMED stage %d room %#04x slot %d' % u)
    for s in stomp:
        print('  FOOTSTEP COLLISION stage %d room %#04x slot %d (%s -> %s)' % s)

    if args.write:
        with open(OUT_C, 'w', encoding='utf-8', newline='\n') as f:
            f.write(emit(mined, missing, rows))
        print('wrote %s' % os.path.relpath(OUT_C, REPO))
    return 1 if (unknown or stomp) else 0


if __name__ == '__main__':
    sys.exit(main())
