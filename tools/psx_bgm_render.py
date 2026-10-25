#!/usr/bin/env python3
"""Render a PS1 BGM: PSYQ SEQ sequence played through its VAB instruments.

Data model (recovered from libsd's `SsVabOpenHeadWithMode` and `play_sfx`):

  VAB header (inside SEPxx.HSB, `pBAV`):
    0x00  VabHdr            (ver@4, ps@0x12, ts@0x14, vs@0x16)
    0x20  ProgAtr[kMax]     16 B each: [+0]=tones, [+1]=mvol
    0x820 ToneAtr           at +0x820 + prog*0x200 + tone*0x20:
                              [+2]=vol [+3]=pan [+4]=centre [+5]=fine
                              [+6]=min note [+7]=max note
                              [+16]=adsr1 [+18]=adsr2 [+22]=vag index
    0x820 + ps*0x200        VAG size table (u16 * vs, *8 bytes for ver>4)
    then                    VAG attributes (32 B each)
  body offset             = *(u32*)(size-0x0c)
  sequence offset         = *(u32*)(size-0x08)

  SEQ ("pQES"): variable-length delta then MIDI-like events (running status);
  0x9n note on, 0x8n note off, 0xBn control change, 0xCn program change,
  0xEn pitch bend, 0xFF end. Events start at 0x13 for these files.

The synth decodes each VAG to PCM at 44100 and plays it at
`2^((note - centre + fine/128)/12)` with the tone's PSX ADSR.

Tempo: the SEQ tick length is not in the header we decoded, so `--tick-rate`
(ticks per second, default 60) is exposed; calibrate against a known BGM.

Usage:
    python tools/psx_bgm_render.py                # all SEPxx -> .wav
    python tools/psx_bgm_render.py SEP00          # one bank
    python tools/psx_bgm_render.py --tick-rate 50
"""

import argparse
import glob
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import psx_audio_extract as px

SR = 44100


def parse_vab(d):
    ver = struct.unpack_from('<I', d, 4)[0]
    ps, ts, vs = struct.unpack_from('<3H', d, 0x12)
    kmax = 0x80 if ver > 4 else 0x40
    prog = {}
    for i in range(kmax):
        off = 0x20 + i * 16
        nt = d[off]
        if nt:
            prog[i] = dict(tones=nt, mvol=d[off + 1], mpan=d[off + 2])
    tone_base = 0x820
    tones = {}
    for p, pi in prog.items():
        tl = []
        for t in range(pi['tones']):
            off = tone_base + p * 0x200 + t * 0x20
            tl.append(dict(vol=d[off + 2], pan=d[off + 3], centre=d[off + 4],
                           fine=d[off + 5], mn=d[off + 6], mx=d[off + 7],
                           adsr1=struct.unpack_from('<H', d, off + 16)[0],
                           adsr2=struct.unpack_from('<H', d, off + 18)[0],
                           vag=struct.unpack_from('<H', d, off + 22)[0]))
        tones[p] = tl
    size_off = tone_base + ps * 0x200
    sizes = [struct.unpack_from('<H', d, size_off + 2 * i)[0] for i in range(vs + 1)]
    return prog, tones, sizes


def load_vag_samples(d, body_off, sizes):
    """Decode each VAG using the frame end flags (matches the VAB `vs` count)."""
    return px.split_samples(d, body_off)[:max(0, len(sizes) - 1)]


def seq_tracks(data):
    """Split a PSYQ SEQ into tracks.

    Layout: `pQES` + 4 zero bytes, then tracks. The first track has an 8-byte
    prefix; later tracks a 2-byte prefix. Each track header is 11 bytes:
    time-base (u16 BE), tempo (u24 BE, microseconds per beat), 2 bytes, event
    data size (u32 BE); events follow. Ticks per second = BPM*time_base/60.
    """
    tracks = []
    pos = 8
    first = True
    while pos + 11 <= len(data):
        if not first:
            pos += 2
        if pos + 11 > len(data):
            break
        tb = (data[pos] << 8) | data[pos + 1]
        tempo = (data[pos + 2] << 16) | (data[pos + 3] << 8) | data[pos + 4]
        size = int.from_bytes(data[pos + 7:pos + 11], 'big')
        bpm = 60000000.0 / tempo if tempo else 120.0
        events = parse_seq(data[pos + 11:pos + 11 + size], start=0)
        tracks.append((tb, bpm, events))
        pos = pos + 11 + size
        first = False
        if size == 0:
            break
    return tracks


def parse_seq(data, start=0x13):
    pos = start
    last = 0
    t = 0
    out = []
    while pos < len(data):
        delta = 0
        while True:
            b = data[pos]; pos += 1
            delta = (delta << 7) | (b & 0x7F)
            if not (b & 0x80):
                break
        t += delta
        s = data[pos]
        if s < 0x80:
            s = last
        else:
            pos += 1
        last = s
        hi, ch = s & 0xF0, s & 0x0F
        if hi in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
            d1, d2 = data[pos], data[pos + 1]; pos += 2
            out.append((t, s, ch, d1, d2))
        elif hi in (0xC0, 0xD0):
            out.append((t, s, ch, data[pos], 0)); pos += 1
        elif s == 0xFF:
            break
        elif s == 0xF1:
            pos += 1
        elif s == 0xF2:
            pos += 2
        else:
            break
    return out


def pick_tone(tones, note):
    best = None
    for t in tones:
        if t['mn'] <= note <= t['mx']:
            best = t
            break
    if best is None:
        best = min(tones, key=lambda t: abs((t['mn'] + t['mx']) / 2.0 - note))
    return best


def adsr_envelope(tone, nframes, note_off):
    """PSX-ish ADSR: short attack, decay to sustain, hold, then release.

    The exact SPU rate tables are not reproduced; times are fixed but the
    sustain level and curve modes come from adsr1/adsr2.
    """
    a_mode = (tone['adsr1'] >> 15) & 1
    d_mode = (tone['adsr1'] >> 7) & 1
    s_level = ((tone['adsr2'] >> 8) & 0x7F) / 127.0
    r_mode = (tone['adsr2'] >> 5) & 1
    a = max(1, int(0.004 * SR))
    d = max(1, int(0.25 * SR))
    r = max(1, int(0.35 * SR))
    env = [0.0] * nframes
    for i in range(min(a, nframes)):
        x = i / float(a)
        env[i] = (x ** 1.5) if a_mode else x
    for i in range(a, min(a + d, nframes)):
        x = (i - a) / float(d)
        curve = (1 - (1 - x) ** 3) if d_mode else x
        env[i] = 1.0 - (1.0 - s_level) * curve
    for i in range(a + d, min(note_off, nframes)):
        env[i] = s_level
    for i in range(note_off, nframes):
        x = min(1.0, (i - note_off) / float(r))
        env[i] = s_level * ((1 - x) ** 2 if r_mode else 1 - x)
    return env


def render_voice(sample, tone, note, vel, hold, start, out):
    ratio = 2.0 ** ((note - tone['centre'] + tone['fine'] / 128.0) / 12.0)
    vol = (vel / 127.0) * (tone['vol'] / 127.0)
    if ratio <= 0 or not sample:
        return 0
    r = max(1, int(0.35 * SR))
    length = hold + r
    env = adsr_envelope(tone, length, hold)
    src_n = len(sample)
    end = min(length, len(out) - start)
    for i in range(end):
        p = i * ratio
        j = int(p)
        if j + 1 >= src_n:
            break
        frac = p - j
        s = sample[j] + (sample[j + 1] - sample[j]) * frac
        out[start + i] += s * env[i] * vol
    return end


def render_bank(path, tick_rate, out_dir):
    d = open(path, 'rb').read()
    prog, tones, sizes = parse_vab(d)
    body_off = struct.unpack_from('<I', d, len(d) - 0x0C)[0]
    seq_off = struct.unpack_from('<I', d, len(d) - 0x08)[0]
    vags = load_vag_samples(d, body_off, sizes)
    valid = [t for p in tones for t in tones[p]
             if 0 < t['vag'] <= len(vags) and t['mn'] <= t['mx']]
    tracks = seq_tracks(d[seq_off:body_off])
    base = os.path.splitext(os.path.basename(path))[0]
    for ti, (tb, bpm, events) in enumerate(tracks):
        if not events:
            continue
        tick_s = 60.0 / (bpm * tb) if (tb and bpm) else 1.0 / tick_rate
        ch_prog, ch_vol, ch_pan = {}, {}, {}
        sounding = {}
        voices = []
        last_tick = events[-1][0]
        for t, status, ch, d1, d2 in events:
            hi = status & 0xF0
            if hi == 0xC0:
                ch_prog[ch] = d1
            elif hi == 0xB0:
                if d1 == 7:
                    ch_vol[ch] = d2
                elif d1 == 10:
                    ch_pan[ch] = d2
            elif hi == 0x90 and d2 > 0:
                p = ch_prog.get(ch, 0)
                tone = None
                if p in tones:
                    tone = pick_tone(tones[p], d1)
                    if tone['vag'] <= 0 or tone['vag'] > len(vags):
                        tone = None
                if tone is None and valid:
                    tone = pick_tone(valid, d1)
                if tone is None:
                    continue
                sounding[(ch, d1)] = (t, d2 * ch_vol.get(ch, 127) / 127.0, tone, d1)
            elif hi == 0x80 or (hi == 0x90 and d2 == 0):
                v = sounding.pop((ch, d1), None)
                if v:
                    voices.append((v[0], t, v[1], v[2], v[3]))
        for key, v in sounding.items():
            voices.append((v[0], last_tick + 2, v[1], v[2], v[3]))
        if not voices:
            continue
        frames = int(math.ceil(last_tick * tick_s * SR)) + int(1.5 * SR)
        mix = [0.0] * frames
        for start_tick, end_tick, vel, tone, note in voices:
            start = int(start_tick * tick_s * SR)
            dur = max(1, int((end_tick - start_tick) * tick_s * SR))
            render_voice(vags[tone['vag'] - 1], tone, note, vel, dur, start, mix)
        peak = max(1.0, max(abs(x) for x in mix))
        pcm = [int(max(-32768, min(32767, x / peak * 32000))) for x in mix]
        name = base if len(tracks) == 1 else '%s_t%d' % (base, ti)
        px.write_wav(os.path.join(out_dir, name + '.wav'), pcm, SR)
        print('%-8s t%d tb=%d bpm=%.0f %d events, %d voices, %.1fs -> %s.wav' %
              (base, ti, tb, bpm, len(events), len(voices),
               len(pcm) / float(SR), name))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('bank', nargs='?', default=None)
    ap.add_argument('--tick-rate', type=float, default=60.0,
                    help='SEQ ticks per second (default 60)')
    ap.add_argument('-o', '--out', default='assets/PSX/SOUND')
    args = ap.parse_args()
    if args.bank:
        files = [os.path.join('assets', 'PSX', 'SOUND', args.bank + '.HSB')]
    else:
        files = sorted(glob.glob(os.path.join('assets', 'PSX', 'SOUND', 'SEP*.HSB')))
    for f in files:
        render_bank(f, args.tick_rate, args.out)


if __name__ == '__main__':
    main()
