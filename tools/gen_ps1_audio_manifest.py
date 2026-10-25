#!/usr/bin/env python3
"""gen_ps1_audio_manifest.py - name every PS1 sound after the PC .wav it replaces.

The asset migrator can replace the PC release's Sound/ and Voice/ WAVs with the
PS1 originals (the PC files are 22050 Hz, mostly 8-bit re-renders of them). The
migrator only DECODES; which PS1 sample becomes which PC file is decided here,
once, and baked into tools/asset_migrator/src/core/Ps1AudioManifest.h.

WHERE EACH NAME COMES FROM - the PC's own tables, not a guess
------------------------------------------------------------
Every PC sound is loaded through a table whose PS1 counterpart is on the disc,
slot for slot:

  g_SoundBanksTable[b][k]      SOUND/<WP001..WP00A, BIO, EVIL, SELECT, ENDING>.HED
  g_charactersSfxTable[c][k]   SOUND/CHAR00..05.HED
  g_roomSoundEffectsTable[i][k] SOUND/DOOR*.HED
  g_RoomSndData[s*29+r][k]     the room's own bank inside ROOM<s+1><rr><c>.RDT
  g_StageVoiceNamesTable[s][i] VOICE<s+1>.XAS, clip i of g_StageVoiceOffsetTable
  g_BgmNameTable[g][t]         SOUND/SEP<gg>.HSB, sequence track t

A .HED is a Capcom slot table (one 4-byte record per id: byte 1 = VAB program,
byte 2 = tone) followed by a stock VAB header; its .VB is the VAB body. The RDT
carries the same three pieces at +0x88/+0x8C/+0x90. An ALL-ZERO record is
program 0 / tone 0, a real sound - WP002 slot 7 reads as "empty" but is tone 0,
which is the PC's Gun01 at g_st2[7].

The slot tables are the candidates; the audio decides. Each candidate is
rendered at its PS1 playback rate, 44100 * 2^((note - centre + fine/128) / 12)
(the +fine sign is the one that correlates - the other sign never wins), and
scored against the PC file by waveform NCC and by a log-RMS envelope
correlation. The envelope is what matters for noise-like sounds (shots, doors,
footsteps), whose waveform decorrelates under any rate difference. A blind
all-against-all search is NOT good enough: it paired z_k03 with a weapon bank.
Restricted to its own table's candidates every name resolves.

  VOICES. VOICE<n>.XAS is 16 CD-XA channels interleaved sector by sector
  (37.8 kHz mono 4-bit). The offset table's values are in 16-sector cycles, and
  every bit-15 entry closes a group: the clip after it is on the NEXT channel
  (VoicePlayClip (0x800152a4) in SLUS_005.51 adds that count to the sector). Reading the file
  as one stream - what a 2048-byte extract forces - is why an earlier attempt
  never matched a PC voice. Clip i of stage s is g_StageVoiceNamesTable[s][i].
  A name gets the PS1 clip even where the PC file under it is different audio
  (the PS1's timing depends on it - see resolve_voices), and an id whose clip
  no name holds becomes a PS1-only P<row>_<id> file.

Usage:
    python tools/gen_ps1_audio_manifest.py <PS1 .cue/.bin/.img> [--write]

The image must be RAW (2352-byte sectors) for the voices. Both the 1996 USA
disc and the Director's Cut carry byte-identical audio (docs/PSX_DC_AUDIO.md).
"""
import argparse
import hashlib
import os
import re
import struct
import sys
import wave

import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'tools'))
from gen_dc_arrange_sound_rows import read_rows  # noqa: E402

SOUNDTABLES = os.path.join(REPO, 'src', 'game', 'SoundTables.cpp')
SOUNDAPI = os.path.join(REPO, 'src', 'game', 'SoundApi.cpp')
SOUNDSYS = os.path.join(REPO, 'src', 'game', 'SoundSystem.cpp')
PC_SOUND = os.path.join(REPO, 'assets', 'USA', 'Sound')
PC_VOICE = os.path.join(REPO, 'assets', 'USA', 'Voice')
OUT_H = os.path.join(REPO, 'tools', 'asset_migrator', 'src', 'core',
                     'Ps1AudioManifest.h')

# g_SoundBanksTable bank id -> the PS1 bank. 15 (Win95_mg, A_mcn03) is PC-only.
SFX_BANK_FILES = ['WP001', 'WP001', 'WP002', 'WP003', 'WP004', 'WP005', 'WP006',
                  'WP007', 'WP008', 'WP009', 'WP00A', 'BIO', 'EVIL', 'SELECT',
                  'ENDING']
# g_charactersSfxTable id -> candidate CHARxx banks. The order of CHAR00..05 is
# not documented anywhere, so every CHAR bank is a candidate for every row.
CHAR_FILES = ['CHAR00', 'CHAR01', 'CHAR02', 'CHAR03', 'CHAR04', 'CHAR05']
# g_roomSoundEffectsTable index -> the DOOR bank, by the name both use.
DOOR_FILES = ['DOORWOD', 'DOORMTL', 'DOORBRK', 'DOORREB', 'DOORSTJ', 'DOORSTW',
              'DOORELV', 'DOORNEW', 'DOORGAT', 'DOORSMT', 'DOOROLD', 'DOORLAD',
              'DOORAIR', 'DOORLAB', 'DOORFTN']

XA_K = [(0, 0), (60, 0), (115, -52), (98, -55), (122, -60)]
XA_RATE = 37800


# ---------------------------------------------------------------- disc image
class Disc:
    def __init__(self, path):
        if path.lower().endswith('.cue'):
            with open(path, encoding='latin1') as f:
                m = re.search(r'FILE\s+"([^"]+)"', f.read())
            path = os.path.join(os.path.dirname(path), m.group(1))
        self.f = open(path, 'rb')
        self.f.seek(0, 2)
        self.raw = self.f.tell() % 2352 == 0 and self._probe_raw()
        self.files = {}
        pvd = self.user(16)
        root = pvd[156:190]
        self._walk(struct.unpack_from('<I', root, 2)[0],
                   struct.unpack_from('<I', root, 10)[0], '')

    def _probe_raw(self):
        self.f.seek(0)
        return self.f.read(12) == b'\x00' + b'\xff' * 10 + b'\x00'

    def sector(self, lba):
        self.f.seek(lba * 2352)
        return self.f.read(2352)

    def user(self, lba):
        if self.raw:
            return self.sector(lba)[24:24 + 2048]
        self.f.seek(lba * 2048)
        return self.f.read(2048)

    def _walk(self, lba, size, prefix):
        data = b''.join(self.user(lba + i) for i in range((size + 2047) // 2048))
        p = 0
        while p < len(data):
            n = data[p]
            if n == 0:
                p = (p // 2048 + 1) * 2048
                continue
            elba, esz = struct.unpack_from('<I', data, p + 2)[0], struct.unpack_from('<I', data, p + 10)[0]
            nl = data[p + 32]
            name = data[p + 33:p + 33 + nl].decode('latin1').split(';')[0]
            if name not in ('\x00', '\x01'):
                full = (prefix + '/' + name).upper()
                if data[p + 25] & 2:
                    self._walk(elba, esz, full)
                else:
                    self.files[full] = (elba, esz)
            p += n

    def find(self, folder, name):
        """`<anything>/<folder>/<name>` - the discs nest PSX/ differently."""
        tail = ('/' + folder + '/' + name).upper()
        for k, v in self.files.items():
            if k.endswith(tail):
                return v
        return None

    def read(self, folder, name):
        e = self.find(folder, name)
        if e is None:
            return None
        lba, size = e
        return b''.join(self.user(lba + i) for i in range((size + 2047) // 2048))[:size]


# ---------------------------------------------------------------- SPU ADPCM
def decode_vag(d):
    out = []
    h1 = h2 = 0
    for p in range(0, len(d) - 15, 16):
        sf, fl = d[p], d[p + 1]
        sh = 12 - (sf & 15)
        f = sf >> 4
        k0, k1 = XA_K[f if f <= 4 else 0]
        for i in range(14):
            b = d[p + 2 + i]
            for n in (b & 15, b >> 4):
                n = n - 16 if n > 7 else n
                s = (n << sh if sh >= 0 else n >> -sh) + ((h1 * k0 + h2 * k1 + 32) >> 6)
                s = -32768 if s < -32768 else (32767 if s > 32767 else s)
                h2, h1 = h1, s
                out.append(s)
        if fl & 1:
            break
    return np.array(out, np.float64)


class Bank:
    """A slot table + VAB (header, body) - a .HED/.VB pair or an RDT."""

    def __init__(self, recs, hdr, vo, body):
        self.recs, self.hdr, self.vo, self.body = recs, hdr, vo, body
        self.ps = struct.unpack_from('<h', hdr, vo + 18)[0]
        tbl = vo + 32 + 2048 + self.ps * 16 * 32
        self.sizes = struct.unpack_from('<256H', hdr, tbl)
        self.off, acc = [0] * 256, 0
        for i in range(1, 256):
            self.off[i] = acc
            acc += self.sizes[i] * 8

    def tone(self, slot):
        """(tone attrs, VAG bytes) for a slot, or None when it is empty."""
        if slot >= len(self.recs):
            return None
        prog, tone = self.recs[slot][1], self.recs[slot][2]
        if prog >= self.ps or tone >= self.hdr[self.vo + 32 + prog * 16]:
            return None
        o = self.vo + 32 + 2048 + (prog * 16 + tone) * 32
        h = self.hdr
        t = dict(prog=prog, tone=tone, centre=h[o + 4], fine=h[o + 5], note=h[o + 6],
                 vag=struct.unpack_from('<H', h, o + 22)[0])
        v = t['vag']
        if not (0 < v < 256 and self.sizes[v]):
            return None
        blob = self.body[self.off[v]:self.off[v] + self.sizes[v] * 8]
        if len(blob) != self.sizes[v] * 8:
            return None
        return t, blob


def hed_bank(disc, name):
    hed = disc.read('SOUND', name + '.HED')
    vb = disc.read('SOUND', name + '.VB')
    if hed is None or vb is None:
        return None
    vo = struct.unpack_from('<I', hed, len(hed) - 8)[0]
    return Bank([hed[i:i + 4] for i in range(0, vo, 4)], hed, vo, vb)


def rdt_bank(disc, folder, name):
    d = disc.read(folder, name)
    if d is None or len(d) < 0x94:
        return None
    a88, a8c, a90 = (struct.unpack_from('<I', d, o)[0] for o in (0x88, 0x8C, 0x90))
    if not (0 < a88 < a8c < a90 <= len(d)) or d[a8c:a8c + 4] != b'pBAV':
        return None
    return Bank([d[a88 + i * 4:a88 + i * 4 + 4] for i in range(48)], d, a8c, d[a90:])


def play_rate(t):
    return 44100.0 * 2 ** ((t['note'] - t['centre'] + t['fine'] / 128.0) / 12.0)


# ---------------------------------------------------------------- CD-XA
def decode_xa(sectors):
    out = []
    h1 = h2 = 0
    for r in sectors:
        d = r[24:24 + 2304]
        for g in range(18):
            grp = d[g * 128:(g + 1) * 128]
            for u in range(8):
                prm = grp[4 + u]
                sh = 12 - (prm & 15)
                f = prm >> 4
                k0, k1 = XA_K[f if f <= 4 else 0]
                for j in range(28):
                    b = grp[16 + (u >> 1) + j * 4]
                    n = (b >> 4) if (u & 1) else (b & 15)
                    n = n - 16 if n > 7 else n
                    s = (n << sh if sh >= 0 else n >> -sh) + ((h1 * k0 + h2 * k1 + 32) >> 6)
                    s = -32768 if s < -32768 else (32767 if s > 32767 else s)
                    h2, h1 = h1, s
                    out.append(s)
    return np.array(out, np.float64)


# ---------------------------------------------------------------- scoring
def load_wav(path):
    w = wave.open(path)
    raw = w.readframes(w.getnframes())
    if w.getsampwidth() == 1:
        a = (np.frombuffer(raw, np.uint8).astype(np.float64) - 128) * 256
    else:
        a = np.frombuffer(raw, np.int16).astype(np.float64)
    return a[::w.getnchannels()], w.getframerate()


def envelope(a, rate, ms=10):
    n = max(1, int(rate * ms / 1000))
    k = len(a) // n
    if k < 1:
        return np.zeros(1)
    return np.log(np.sqrt((a[:k * n].reshape(k, n) ** 2).mean(1) + 1))


def env_corr(x, y, maxlag=20):
    best = -1.0
    for lag in range(-maxlag, maxlag + 1):
        a, b = x[max(0, lag):], y[max(0, -lag):]
        m = min(len(a), len(b))
        if m < 5:
            continue
        a, b = a[:m] - a[:m].mean(), b[:m] - b[:m].mean()
        den = np.linalg.norm(a) * np.linalg.norm(b)
        if den > 0:
            best = max(best, float(np.dot(a, b) / den))
    return best


def wave_ncc(x, y):
    n = len(x) + len(y)
    size = 1 << (n - 1).bit_length()
    c = np.fft.irfft(np.fft.rfft(x, size) * np.conj(np.fft.rfft(y, size)), size)
    den = np.linalg.norm(x) * np.linalg.norm(y)
    return float(np.max(np.abs(c)) / den) if den > 0 else 0.0


def resample(x, src, dst):
    n = max(1, int(len(x) * dst / src))
    return np.interp(np.arange(n) * src / dst, np.arange(len(x)), x)


def score(pcm, rate, ref, ref_rate):
    """(waveform NCC, envelope correlation, length ratio) of a PS1 render vs a
    PC file."""
    x = resample(pcm, rate, ref_rate)
    ratio = len(x) / max(1, len(ref))
    if not 0.5 <= ratio <= 2.0:
        return 0.0, 0.0, ratio
    return wave_ncc(x, ref), env_corr(envelope(pcm, rate), envelope(ref, ref_rate)), ratio


def accepted(w, e, ratio, exact):
    """`exact`: the PS1 slot is the one the PC table names. The PC rows are
    not always slot-exact (room 116 has gatan/sw_trap at 23/24, the PS1 bank
    at 24/23), so any slot of the same bank may answer - but a neighbour has
    to clear a higher bar than the slot the table points at."""
    if exact:
        return w >= 0.8 or e >= 0.95
    return w >= 0.9 or (e >= 0.97 and 0.8 <= ratio <= 1.25)


# ---------------------------------------------------------------- PC tables
def c_string_tables(path, pattern):
    """{table name: [str|None, ...]} for `static const char* NAME[N] = {...};`."""
    with open(path, encoding='utf-8', errors='replace') as f:
        txt = f.read()
    out = {}
    for m in re.finditer(pattern, txt):
        body = txt[m.end():txt.index('};', m.end())]
        body = re.sub(r'//[^\n]*', '', body)
        out[m.group(1)] = [None if null else s
                           for s, null in re.findall(r'"([^"]*)"|(NULL)', body)]
    return out


def sfx_contexts():
    """[(pc name, [(bank key, slot), ...])] from the four slot tables."""
    ctx = {}

    def add(name, cands):
        if name:
            ctx.setdefault(name.upper(), []).extend(cands)

    st = c_string_tables(SOUNDAPI, r'static const char\* (g_st\d+)\[SFX_SUBTABLE_SIZE\] = \{')
    with open(SOUNDAPI, encoding='utf-8') as f:
        txt = f.read()
    order = re.findall(r'g_st\d+', txt[txt.index('g_SoundBanksTable[SFX_SUBTABLE_SIZE] = {'):])[:16]
    for bank, tname in enumerate(order[:len(SFX_BANK_FILES)]):
        for k, name in enumerate(st[tname]):
            # play_sfx folds ids > 15 by 16, so the 32-record banks (BIO, EVIL,
            # ENDING) may hold the PC's slot k at k or at k + 16.
            add(name, [(('SOUND', SFX_BANK_FILES[bank]), k),
                       (('SOUND', SFX_BANK_FILES[bank]), k + 16)])

    ch = c_string_tables(SOUNDSYS, r'static const char\* (g_charSfx_\d+)\[16\] = \{')
    for tname, names in ch.items():
        for k, name in enumerate(names):
            add(name, [(('SOUND', f), k) for f in CHAR_FILES])

    rs = c_string_tables(SOUNDSYS, r'static const char\* (g_roomSfx_\d+)\[2\] = \{')
    for tname, names in rs.items():
        i = int(tname.rsplit('_', 1)[1])
        for k, name in enumerate(names):
            add(name, [(('SOUND', DOOR_FILES[i]), k)])

    for row, names in enumerate(read_rows(SOUNDTABLES)):
        stage, room = divmod(row, 29)
        for k, name in enumerate(names):
            add(name, [(('STAGE%d' % (stage + 1), 'ROOM%d%02X%d.RDT' % (stage + 1, room, c)), k)
                       for c in (0, 1)])
    return ctx


def voice_tables():
    """Per stage: [(channel, start cycle, end cycle)] and the names, by id."""
    with open(SOUNDTABLES, encoding='utf-8') as f:
        txt = f.read()
    out = []
    for s in range(5):
        m = re.search(r'g_VoiceOffsetData_Stage%d\[\d+\] = \{[^\n]*\n(.*?)\};' % s, txt, re.S)
        vals = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{4})', m.group(1))]
        m = re.search(r'g_VoiceNameData_Stage%d\[\]\[9\] = \{(.*?)\};' % s, txt, re.S)
        names = re.findall(r'"(\w*)"', re.sub(r'//[^\n]*', '', m.group(1)))
        clips, p, ch = [], 0, 0
        # VoicePlayClip (0x800152a4) (SLUS_005.51): clip i starts at entry p and ends at
        # entry p+1; a bit-15 entry is its group's last clip and is followed by
        # that clip's end, so the walk skips one more and moves to channel+1.
        while p + 1 < len(vals) and len(clips) < len(names):
            e = vals[p]
            clips.append((ch, e & 0x7FFF, vals[p + 1] & 0x7FFF))
            if e & 0x8000:
                ch += 1
                p += 2
            else:
                p += 1
        out.append((clips, names))
    return out


def bgm_table():
    with open(SOUNDTABLES, encoding='utf-8') as f:
        txt = f.read()
    i = txt.index('g_BgmNameTable[57][4] = {')
    body = txt[i:txt.index('};', i)]
    rows = re.findall(r'\{([^{}]*)\}', body)
    out = []
    for g, row in enumerate(rows):
        for t, tok in enumerate(re.findall(r'"(\w+)"|NULL', row)):
            if tok:
                out.append((tok.upper(), g, t))
    return out


def seq_tracks(hsb):
    """[(notes, seconds of one pass)] per SEP track of a SEPxx.HSB.

    The trailer's last words hold the sequence offset (size-8) and the VAB
    body offset (size-12). The SEP is 'pQES' + 4 bytes, then per track a
    2-byte id (not on the first), time base (u16 BE), tempo (u24 BE, us per
    beat), 2 bytes, event size (u32 BE) and the events. One pass is up to the
    loop end (CC99 = 30) when the track loops, else to its end."""
    seq = struct.unpack_from('<I', hsb, len(hsb) - 8)[0]
    body = struct.unpack_from('<I', hsb, len(hsb) - 12)[0]
    d = hsb[seq:body]
    out, pos, first = [], 8, True
    while pos + 11 <= len(d):
        if not first:
            pos += 2
        tb = (d[pos] << 8) | d[pos + 1]
        tempo = int.from_bytes(d[pos + 2:pos + 5], 'big')
        size = int.from_bytes(d[pos + 7:pos + 11], 'big')
        ev = d[pos + 11:pos + 11 + size]
        t = p = last = notes = 0
        loop_end = None
        # Tempo changes (FF 51) are rare (5 in 57 banks) and all at tick 0 of
        # one-shot tracks; the renderer honours them, this estimate need not.
        while p < len(ev):
            delta = 0
            while True:
                b = ev[p]
                p += 1
                delta = (delta << 7) | (b & 0x7F)
                if not b & 0x80:
                    break
            t += delta
            s = ev[p]
            if s < 0x80:
                s = last
            else:
                p += 1
            last = s
            hi = s & 0xF0
            if hi in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                if hi == 0x90 and ev[p + 1]:
                    notes += 1
                if hi == 0xB0 and ev[p] == 99 and ev[p + 1] == 30 and loop_end is None:
                    loop_end = t
                p += 2
            elif hi in (0xC0, 0xD0):
                p += 1
            elif s == 0xFF:
                if ev[p] == 0x51:
                    p += 4
                    continue
                break
            else:
                break
        ticks = loop_end if loop_end is not None else t
        tps = (60e6 / tempo) * tb / 60.0 if tempo and tb else 60.0
        out.append((notes, ticks / tps))
        pos += 11 + size
        first = False
        if size == 0:
            break
    return out


def resolve_bgm(disc, log):
    """g_BgmNameTable, checked against the sequences.

    A group's slots are its SEP's tracks in order - except where they are not:
    group 0x0A lists {Bgm_40, Bgm_0c} but its track 0 is the 30 s one the PC
    ships as Bgm_0c and track 1 the 6 s Bgm_40. A group is re-ordered only
    when the swap is decisively better on length (cost below a quarter of the
    table order's); the PC's own renders are sometimes the loop body alone
    (Bgm_04, Se_39lp) or several loop passes (Bgm_3d), so length alone must not
    override the table on a close call. A track with no notes (chain1, the
    slot V110_00 fills - that dialogue is not a sequence) is left to the PC."""
    import itertools
    groups = {}
    for name, g, t in bgm_table():
        groups.setdefault(g, []).append((name, t))
    out, kept = {}, []
    for g in sorted(groups):
        hsb = disc.read('SOUND', 'SEP%02X.HSB' % g)
        if hsb is None:
            continue
        tr = seq_tracks(hsb)
        slots = groups[g]
        pcdur, pcrms = [], []
        for name, _ in slots:
            p = pc_file(PC_SOUND, name)
            if p:
                ref, rate = load_wav(p)
                pcdur.append(len(ref) / rate)
                pcrms.append(float(np.sqrt((ref ** 2).mean())))
            else:
                pcdur.append(None)
                pcrms.append(0.0)

        def cost(order):
            c = 0.0
            for (name, _), tk, pd in zip(slots, order, pcdur):
                if tk >= len(tr) or pd is None or tr[tk][1] <= 0:
                    c += 5.0
                else:
                    c += abs(np.log(tr[tk][1] / pd))
            return c

        ident = [t for _, t in slots]
        order = ident
        if len(slots) > 1:
            best = min(itertools.permutations(range(len(tr)), len(slots)), key=cost)
            if list(best) != ident and cost(best) < 0.25 * cost(ident):
                order = list(best)
                log('bgm: group %02X re-ordered %s -> tracks %s' % (
                    g, [n for n, _ in slots], order))
        for (name, _), tk, pd, rms in zip(slots, order, pcdur, pcrms):
            if tk >= len(tr) or tr[tk][0] == 0:
                kept.append((name, g, tk))
                continue
            err = abs(np.log(tr[tk][1] / pd)) if pd else 0.0
            if name not in out or err < out[name][3]:
                out[name] = (name, g, tk, err, tr[tk][1], pd, rms)
    # A name can come from several groups (Bgm_05 is SEP00 track 2 and an
    # empty SEP18 track 2); it is only left to the PC when no group has it.
    kept = [k for k in kept if k[0] not in out]
    log('bgm: %d channels, %d left to the PC file' % (len(out), len(kept)))
    for name, g, tk in kept:
        log('   keep PC %-8s SEP%02X track %d has no notes' % (name, g, tk))
    return [out[k] for k in sorted(out)]


# ---------------------------------------------------------------- main
def pc_file(folder, name):
    for f in os.listdir(folder):
        if f.upper() == name.upper() + '.WAV':
            return os.path.join(folder, f)
    return None


def resolve_sfx(disc, log):
    banks = {}

    def bank(key):
        if key not in banks:
            folder, name = key
            banks[key] = hed_bank(disc, name) if folder == 'SOUND' else rdt_bank(disc, folder, name)
        return banks[key]

    rendered = {}
    out, rejected = [], []
    index = {}

    def everywhere():
        """(VAG bytes, note, centre, fine) -> [(bank key, slot)] over EVERY
        bank on the disc, for fallbacks the PC tables do not point at."""
        if not index:
            keys = [('SOUND', p.rsplit('/', 1)[1][:-4]) for p in disc.files
                    if p.endswith('.HED') and '/SOUND/' in p]
            keys += [(p.rsplit('/', 2)[1], p.rsplit('/', 1)[1]) for p in disc.files
                     if p.endswith('.RDT') and '/STAGE' in p]
            for key in sorted(keys):
                b = bank(key)
                if b is None:
                    continue
                for s in range(len(b.recs)):
                    got = b.tone(s)
                    if got:
                        t, blob = got
                        index.setdefault((blob, t['note'], t['centre'], t['fine']),
                                         []).append((key, s))
            index[None] = []
        return index

    for name, cands in sorted(sfx_contexts().items()):
        path = pc_file(PC_SOUND, name)
        if path is None:
            continue
        ref, ref_rate = load_wav(path)
        named = dict.fromkeys(cands)
        trials = list(named)
        for key in dict.fromkeys(k for k, _ in named):
            b = bank(key)
            if b is not None:
                trials += [(key, s) for s in range(len(b.recs)) if (key, s) not in named]
        best = None
        best_any = None
        found = []  # (ident, key, slot) of every resolvable candidate
        for key, slot in trials:
            b = bank(key)
            if b is None:
                continue
            got = b.tone(slot)
            if got is None:
                continue
            t, blob = got
            ident = (blob, t['note'], t['centre'], t['fine'])
            if ident not in rendered:
                rendered[ident] = decode_vag(blob)
            found.append((ident, key, slot))
            w, e, ratio = score(rendered[ident], play_rate(t), ref, ref_rate)
            exact = (key, slot) in named
            cand = (max(w, e), w, e, key, slot, t, exact, ident)
            if best_any is None or cand[0] > best_any[0]:
                best_any = cand
            if accepted(w, e, ratio, exact) and (best is None or cand[0] > best[0]):
                best = cand
        if best is None:
            if best_any is None:
                rejected.append((name, 'no PS1 candidate'))
            else:
                _, w, e, key, slot, t, exact, _ = best_any
                rejected.append((name, 'best %s/%s slot %d%s wave %.3f env %.3f' % (
                    key[0], key[1], slot, '' if exact else ' (other slot)', w, e)))
            continue
        _, w, e, key, slot, t, exact, ident = best
        # Byte-identical copies of the chosen sample in OTHER files, as
        # fallbacks: the Director's Cut re-encoded some rooms' banks at half
        # the rate (ROOM50A0's drw_opmt 24750 -> 12375 Hz, ROOM40A0's
        # slide_bk 32184 -> 9031 Hz), and the migrator skips a source whose
        # VAG is not the size recorded here.
        alts = [(key, slot)]
        for k2, s2 in [(k, s) for i2, k, s in found if i2 == ident] + everywhere().get(ident, []):
            if k2 not in [k for k, _ in alts] and len(alts) < 6:
                alts.append((k2, s2))
        out.append((name, alts, t, w, e, len(ident[0])))
    log('sfx: %d named, %d left to the PC file' % (len(out), len(rejected)))
    for name, why in rejected:
        log('   keep PC %-10s %s' % (name, why))
    return out


def resolve_voices(disc, log):
    if not disc.raw:
        log('voices: 2048-byte image, the XA channels cannot be read - skipped')
        return []
    # What the PS1 plays at an id is the clip at that id, whatever the PC's file
    # of the same name holds. Most names match (502 at envelope >= 0.9), but not
    # all, and the difference is audible in the timing: the DC's room 513 waits
    # on voice 0x51 ("VB00_11") with an F6 F7-style MSF_VOICE_PLAYING poll that
    # the 1996 room does not have. The PS1 clip is 10.0 s - exactly the scene's
    # 140 + 160 frame walk - while the PC's VB00_11.wav is 17.4 s, so on the PC
    # the scene stalls for seven seconds.
    #
    # So, per name, the ids are grouped by decoded clip. One group: the name IS
    # that clip. Several (V007_0d is ids 99 and 116 of stage 0, two different
    # lines): the name takes the group closest to the PC file and every other id
    # gets a PS1-only file P<row>_<id> that the game's voice loader looks for
    # first. Ids with an EMPTY name record (stage 4 ids 181/182) get one too: the
    # PC cannot play them at all.
    decoded = {}

    def clip(xas, ch, s, e):
        key = (xas, ch, s, e)
        if key not in decoded:
            lba = disc.find('VOICE', 'VOICE%d.XAS' % xas)[0]
            decoded[key] = decode_xa([disc.sector(lba + c * 16 + ch) for c in range(s, e)])
        return decoded[key]

    occ, unnamed = {}, []
    for row, (clips, names) in enumerate(voice_tables()):
        for i, (ch, s, e) in enumerate(clips):
            if s >= e:
                continue
            entry = (row, i, row + 1, ch, s, e)
            if names[i]:
                occ.setdefault(names[i].upper(), []).append(entry)
            else:
                unnamed.append(entry)
    out, extra, differ = [], [], []
    for name in sorted(occ):
        groups = {}
        for ent in occ[name]:
            pcm = clip(*ent[2:])
            groups.setdefault(hashlib.sha1(pcm.astype(np.int16).tobytes()).hexdigest(),
                              []).append(ent)
        path = pc_file(PC_VOICE, name)
        ref = load_wav(path) if path else None
        scored = []
        for h, ents in groups.items():
            env = -1.0
            if ref is not None:
                env = env_corr(envelope(clip(*ents[0][2:]), XA_RATE),
                               envelope(ref[0], ref[1]))
            scored.append((env, h, ents))
        scored.sort(key=lambda x: -x[0])
        env, _, ents = scored[0]
        out.append((name,) + ents[0][2:] + (env,))
        if ref is not None and env < 0.9:
            differ.append((name, env))
        for env2, _, ents2 in scored[1:]:
            for row, i, xas, ch, s, e in ents2:
                extra.append(('P%d_%03X' % (row, i), xas, ch, s, e, -2.0))
    for row, i, xas, ch, s, e in unnamed:
        extra.append(('P%d_%03X' % (row, i), xas, ch, s, e, -3.0))
    log('voices: %d names, %d PS1-only id clips' % (len(out), len(extra)))
    for name, env in differ:
        log('   %-8s the PS1 clip differs from the PC file (env %.3f) - PS1 wins' % (name, env))
    for name, xas, ch, s, e, why in extra:
        log('   %-8s VOICE%d ch %d %s' % (name, xas, ch,
                                          'id shares a name' if why == -2.0 else 'no PC name'))
    return out + extra


def emit(sfx, voices, bgm, path):
    L = []
    L.append('#pragma once')
    L.append('// GENERATED by tools/gen_ps1_audio_manifest.py - do not edit by hand.')
    L.append('//')
    L.append('// Which PS1 sound becomes which PC .wav. See the generator for how each')
    L.append('// name was resolved (the PC slot tables give the candidates, the audio')
    L.append('// confirms them) and for the XA channel layout of the voices.')
    L.append('')
    L.append('#include <cstddef>')
    L.append('#include <cstdint>')
    L.append('')
    L.append('namespace re1 {')
    L.append('')
    L.append('// A sample in a VAB bank: `file` is a SOUND/<name>.HED (+ .VB) or a')
    L.append('// STAGEn/ROOMsrrc.RDT on the disc; `slot` indexes its slot table.')
    L.append('// A name has one row per SOURCE, best first; the later rows are')
    L.append('// byte-identical copies of the same sample in other files. The migrator')
    L.append('// uses the first whose VAG is `vagBytes` long - the Director\'s Cut')
    L.append('// re-encoded some rooms\' banks at half the rate.')
    L.append('struct Ps1SfxEntry {')
    L.append('    const char* pcName;  // Sound/<pcName>.wav')
    L.append('    const char* folder;  // disc folder: SOUND, STAGE1..STAGE7')
    L.append('    const char* file;    // HED base name, or RDT file name')
    L.append('    uint8_t slot;')
    L.append('    uint32_t vagBytes;   // the VAG as the 1996 disc has it')
    L.append('};')
    L.append('')
    L.append('inline constexpr Ps1SfxEntry kPs1Sfx[] = {')
    for name, alts, t, w, e, nbytes in sfx:
        for i, (key, slot) in enumerate(alts):
            note = ('prog %d tone %d, %.0f Hz; wave %.3f env %.3f' % (
                t['prog'], t['tone'], play_rate(t), w, e)) if i == 0 else 'same sample'
            L.append('    {"%s", "%s", "%s", %d, %d},  // %s' % (
                name, key[0], key[1], slot, nbytes, note))
    L.append('};')
    L.append('inline constexpr size_t kPs1SfxCount = sizeof(kPs1Sfx) / sizeof(kPs1Sfx[0]);')
    L.append('')
    L.append('// A voice clip: VOICE<xas>.XAS channel `channel` (of 16), sectors')
    L.append('// channel + 16*c for c in [start, end). 37.8 kHz mono 4-bit CD-XA.')
    L.append('// A P<row>_<id> name is a PS1-only clip for one voice id (voice table')
    L.append('// row, id in hex), which voice_load_and_play looks for before the name.')
    L.append('struct Ps1VoiceEntry {')
    L.append('    const char* pcName;  // Voice/<pcName>.wav')
    L.append('    uint8_t xas;         // 1..5')
    L.append('    uint8_t channel;')
    L.append('    uint16_t start;')
    L.append('    uint16_t end;')
    L.append('};')
    L.append('')
    L.append('inline constexpr Ps1VoiceEntry kPs1Voices[] = {')
    for name, xas, ch, s, e, env in voices:
        note = {-1.0: 'no PC file', -2.0: 'PS1-only: this id is not the named clip',
                -3.0: 'PS1-only: no PC name for this id'}.get(env, 'env %.3f vs the PC file' % env)
        L.append('    {"%s", %d, %d, %d, %d},  // %s' % (name, xas, ch, s, e, note))
    L.append('};')
    L.append('inline constexpr size_t kPs1VoiceCount = sizeof(kPs1Voices) / sizeof(kPs1Voices[0]);')
    L.append('')
    L.append('// A BGM channel: SOUND/SEP<group>.HSB, sequence track `track`')
    L.append("// (g_BgmNameTable[group][track]). `pcRms` is the PC file's RMS in")
    L.append('// 16-bit units: the PC normalised every BGM on its own (0.66..3.5x the')
    L.append("// PS1 render's level) and its engine volumes assume those levels, so the")
    L.append('// render is levelled to it. 0 = no PC file.')
    L.append('struct Ps1BgmEntry {')
    L.append('    const char* pcName;  // Sound/<pcName>.wav')
    L.append('    uint8_t group;')
    L.append('    uint8_t track;')
    L.append('    uint16_t pcRms;')
    L.append('};')
    L.append('')
    L.append('inline constexpr Ps1BgmEntry kPs1Bgm[] = {')
    for name, g, t, err, dur, pd, rms in bgm:
        pc = ('PC %.1f s' % pd) if pd else 'no PC file'
        L.append('    {"%s", 0x%02X, %d, %d},  // one pass %.1f s, %s' % (
            name, g, t, int(round(rms)), dur, pc))
    L.append('};')
    L.append('inline constexpr size_t kPs1BgmCount = sizeof(kPs1Bgm) / sizeof(kPs1Bgm[0]);')
    L.append('')
    L.append('}  // namespace re1')
    L.append('')
    with open(path, 'w', newline='\n', encoding='utf-8') as f:
        f.write('\n'.join(L))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('image', help='raw PS1 disc image (.cue/.bin/.img)')
    ap.add_argument('--write', action='store_true', help='write ' + os.path.relpath(OUT_H, REPO))
    args = ap.parse_args()
    disc = Disc(args.image)
    log = print
    log('image: %s (%s)' % (args.image, 'raw' if disc.raw else '2048-byte'))
    sfx = resolve_sfx(disc, log)
    voices = resolve_voices(disc, log)
    bgm = resolve_bgm(disc, log)
    clash = {s[0] for s in sfx} & {b[0] for b in bgm}
    if clash:
        raise SystemExit('names claimed by both a sample and a sequence: %s' % sorted(clash))
    if args.write:
        emit(sfx, voices, bgm, OUT_H)
        log('wrote ' + os.path.relpath(OUT_H, REPO))


if __name__ == '__main__':
    main()
