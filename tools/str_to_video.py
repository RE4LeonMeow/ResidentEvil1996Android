#!/usr/bin/env python3
"""str_to_video.py - convert PS1 (Capcom) .STR movies to PC video.

Produces, for each movie:
  * <name>.avi - Cinepak AVI, the layout the shipped PC movies use (the
    Windows port plays it through MCI, the Linux port through ffmpeg), so it
    stays drop-in compatible with the existing FMV table; and
  * <name>.mp4 - H.264 + AAC, the modern format the port prefers when present.

Both carry every decoded MDEC frame and the CD-XA audio. The default timing is
the STR's own CD-sector timing: the source is read at the PS1's 150 sectors per
second through the last valid video sector, so trailing XA/padding does not
shorten the displayed movie. Use --timing pc to reproduce the shipped PC
movie's slower wall-clock timing.

Two sources are supported:

  --bin <image>   a raw 2352-byte BIN/CUE image. The movie's sectors are read
                  whole, so the Form 2 audio sectors (2324 bytes = 18 sound
                  groups) are complete. This is the correct source.
  <str files>     a 2048-byte-per-sector extract (the usual "content copy").
                  Video is complete (Form 1), but every Form 2 audio sector was
                  truncated to 2048, losing sound groups 16-17, so the audio
                  glitches once per sector. Use --bin for audio.

Container (docs/PS1_VIDEO_FORMAT.txt): 32-byte chunk header + 2016 bytes of
MDEC bitstream per video sector; audio sectors are CD-XA ADPCM at 37800 Hz,
4-bit stereo. The XA decode is the ffmpeg xa_decode algorithm (cdrom XA).

Usage:
    python tools/str_to_video.py --bin "game.bin" --out assets/DC/Movie
    python tools/str_to_video.py "PSX\\MOVIE" --out out [--format both]
                                 [--timing pc|ps1] [--fps auto|N]
                                 [--pc-movies DIR] [--no-audio] [--dry-run]
"""
import argparse
import glob
import mmap
import os
import struct
import subprocess
import sys
import tempfile
import wave

import numpy as np

import mdec

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SECTOR = 0x800
# PS1 FMV streaming uses CdlModeSpeed; see docs/PS1_VIDEO_FORMAT.txt.
CD_SECTOR_RATE = 150.0
MAGIC = b'\x60\x01\x01\x80'


def source_frame_rate(frames, sectors):
    if not frames or not sectors:
        return 0.0
    raw = frames * CD_SECTOR_RATE / float(sectors)
    for rate in (7.5, 10.0, 15.0, 20.0, 30.0):
        if abs(raw - rate) < 0.25:
            return 20000.0 / 1001.0 if rate == 20.0 else rate
    return raw


# ffmpeg xa_adpcm_table (cdrom XA predictor coefficients, filter 0..4).
XA_TABLE = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))


# ---------------------------------------------------------------------------
# Demultiplexing
# ---------------------------------------------------------------------------
class Movie(object):
    def __init__(self, name):
        self.name = name
        self.width = 0
        self.height = 0
        self.version = 0
        self.sector_count = 0
        self.video_sector_end = 0
        self.frames = []        # bytearray per frame, MDEC bitstream
        self.audio = bytearray()  # concatenated 128-byte XA sound groups
        self.truncated_audio = False


def _is_valid_video(data):
    if len(data) < 0x20 or data[:4] != MAGIC:
        return False
    magic32 = struct.unpack_from('<I', data, 0)[0]
    magic16 = struct.unpack_from('<H', data, 22)[0]
    return magic32 == 0x80010160 and magic16 == 0x3800


def demux(sectors):
    """sectors: iterable of (is_video, data). Video data is one 2048-byte
    chunk (32-byte header + 2016 payload); audio data is the sound-group
    payload (2304 bytes from a Form 2 sector, or 2048 from a truncated copy).
    """
    mov = Movie('')
    by_frame = {}
    order = []
    for is_video, data in sectors:
        if not is_video:
            mov.audio += data
            if len(data) < 2304:
                mov.truncated_audio = True
            continue
        if not _is_valid_video(data):
            continue
        _magic32, _chunk, _chunks, frame_no, _used, w, h, _n, _magic16, qscale, \
            version = struct.unpack_from('<IHHIIHHHHHH', data, 0)
        if frame_no not in by_frame:
            by_frame[frame_no] = bytearray()
            order.append(frame_no)
        by_frame[frame_no] += data[0x20:SECTOR]
        mov.width, mov.height, mov.version = w, h, version
    mov.frames = [by_frame[f] for f in order]
    return mov


def parse_str(path, name):
    """2048-byte-per-sector extract."""
    data = open(path, 'rb').read()
    sectors = []
    video_sector_end = 0
    for s in range(0, len(data), SECTOR):
        chunk = data[s:s + SECTOR]
        is_video = chunk[:4] == MAGIC
        sectors.append((is_video, chunk))
        if _is_valid_video(chunk):
            video_sector_end = s // SECTOR + 1
    mov = demux(sectors)
    mov.sector_count = len(data) // SECTOR
    mov.video_sector_end = video_sector_end
    mov.name = name
    return mov


# ---------------------------------------------------------------------------
# Raw BIN/CUE (ISO9660 + CD-XA)
# ---------------------------------------------------------------------------
def _iso_sector(img, lba, n=2048):
    return img[lba * 2352 + 24:lba * 2352 + 24 + n]


def _iso_dir(img, lba, length):
    data = bytearray()
    for s in range((length + 2047) // 2048):
        data += _iso_sector(img, lba + s)
    out = []
    i = 0
    while i < length:
        n = data[i]
        if n == 0:
            i = (i // 2048 + 1) * 2048
            continue
        rec = data[i:i + n]
        out.append((rec[33:33 + rec[32]].decode('latin1'),
                    struct.unpack_from('<I', rec, 2)[0],
                    struct.unpack_from('<I', rec, 10)[0], rec[25]))
        i += n
    return out


def iso_files(img):
    """Every path in the ISO9660 tree -> (lba, size)."""
    pvd = _iso_sector(img, 16)
    root = pvd[156:190]
    files = {}

    def rec(lba, length, prefix):
        for name, ext, size, flags in _iso_dir(img, lba, length):
            if name in ('\x00', '\x01'):
                continue
            path = prefix + '/' + name
            if flags & 2:
                rec(ext, size, path)
            else:
                files[path.upper()] = (ext, size)
    rec(struct.unpack_from('<I', root, 2)[0],
        struct.unpack_from('<I', root, 10)[0], '')
    return files


def parse_bin(img, lba, size, name):
    """Raw sectors: classify by submode, honour Form 1 vs Form 2 sizing."""
    nsec = size // 2048
    sectors = []
    video_sector_end = 0
    for s in range(nsec):
        raw = img[(lba + s) * 2352:(lba + s) * 2352 + 2352]
        submode = raw[18]
        if submode & 0x04:                     # AUDIO
            sectors.append((False, raw[24:24 + 2304]))
        elif submode & 0x02:                   # VIDEO
            chunk = raw[24:24 + 2048]
            sectors.append((True, chunk))
            if _is_valid_video(chunk):
                video_sector_end = s + 1
    mov = demux(sectors)
    mov.sector_count = nsec
    mov.video_sector_end = video_sector_end
    mov.name = name
    return mov


# ---------------------------------------------------------------------------
# CD-XA ADPCM (ffmpeg xa_decode)
# ---------------------------------------------------------------------------
def _sign4(v):
    return v - 16 if v > 7 else v


def _clamp16(v):
    return 32767 if v > 32767 else (-32768 if v < -32768 else v)


def _xa_params(param):
    shift = 12 - (param & 0x0F)
    filt = param >> 4
    if filt > 4 or shift < 0:
        filt, shift = 0, 12
    return XA_TABLE[filt], shift


def _xa_28(g, i, high, hist, out):
    (f0, f1), shift = _xa_params(g[(5 if high else 4) + i * 2])
    s1, s2 = hist
    for j in range(28):
        d = g[16 + i + j * 4]
        t = _sign4((d >> 4) & 0x0F if high else d & 0x0F)
        s = t * (1 << shift) + ((s1 * f0 + s2 * f1 + 32) >> 6)
        s2 = s1
        s1 = _clamp16(s)
        out.append(s1)
    return [s1, s2]


def decode_xa(groups, stereo):
    """Decode 128-byte CD-XA sound groups to interleaved s16.

    Exactly ffmpeg's xa_decode: unit i (0..3) uses header bytes 4+i*2 / 5+i*2
    for its two channels; its 28 sample pairs live at data offset 16+i+j*4
    (the four units' data are byte-interleaved); the low nibble is the first
    channel and the high nibble the second; shift = 12 - (param & 15).
    """
    left = [0, 0]
    right = [0, 0]
    a = []
    b = []
    for off in range(0, len(groups) - 127, 128):
        g = groups[off:off + 128]
        for i in range(4):
            left = _xa_28(g, i, False, left, a)
            if stereo:
                right = _xa_28(g, i, True, right, b)
            else:
                left = _xa_28(g, i, True, left, b)

    if stereo:
        n = min(len(a), len(b))
        out = np.empty(n * 2, dtype=np.int16)
        out[0::2] = a[:n]
        out[1::2] = b[:n]
        return out, n
    # ffmpeg's mono layout puts each unit's 56 samples as low-half then
    # high-half, so interleave a/b in 28-sample units.
    out = []
    for u in range(len(a) // 28):
        out += a[u * 28:(u + 1) * 28]
        out += b[u * 28:(u + 1) * 28]
    return np.array(out, dtype=np.int16), len(out)


# ---------------------------------------------------------------------------
# Encoding
# ---------------------------------------------------------------------------
def encode(mov, out_dir, fmt, fps, wav_path, dry, asetrate=None):
    outs = []
    for kind in fmt:
        out_path = os.path.join(out_dir, mov.name +
                                ('.mp4' if kind == 'mp4' else '.avi'))
        cmd = ['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y',
               '-f', 'rawvideo', '-pix_fmt', 'rgb24',
               '-s', '%dx%d' % (mov.width, mov.height),
               '-r', '%g' % fps, '-i', '-']
        if wav_path:
            cmd += ['-i', wav_path]
            # The shipped PC AVIs play the PS1 audio slower, i.e. pitched down
            # (their spectral centroid is ~0.79 of the PS1's). Reproduce that
            # with a rate change, not a pitch-preserving time stretch.
            if asetrate:
                cmd += ['-filter:a', 'asetrate=%d,aresample=22050'
                        % round(asetrate)]
        if kind == 'avi':
            cmd += ['-c:v', 'cinepak', '-pix_fmt', 'rgb24']
        else:
            cmd += ['-c:v', 'libx264', '-preset', 'slow', '-crf', '16',
                    '-pix_fmt', 'yuv420p']
        if wav_path:
            cmd += (['-c:a', 'aac', '-b:a', '192k', '-ar', '22050', '-ac', '2']
                    if kind == 'mp4'
                    else ['-c:a', 'pcm_s16le', '-ar', '22050', '-ac', '2'])
        cmd.append(out_path)
        outs.append((out_path, cmd))

    if dry:
        for out_path, _ in outs:
            print('  would encode %s' % os.path.relpath(out_path, REPO))
        return

    for out_path, cmd in outs:
        os.makedirs(out_dir, exist_ok=True)
        proc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
        for frame in mov.frames:
            rgb = mdec.decode_frame(frame, 0, mov.width, mov.height)
            proc.stdin.write(rgb.tobytes())
        proc.stdin.close()
        if proc.wait() != 0:
            raise SystemExit('ffmpeg failed for %s' % out_path)
        print('  wrote %s' % os.path.relpath(out_path, REPO))


def write_wav(path, samples, rate):
    with wave.open(path, 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(samples.astype('<i2').tobytes())


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------
def collect(paths):
    out = []
    for p in paths:
        if os.path.isdir(p):
            out += sorted(glob.glob(os.path.join(p, '*.STR'))) + \
                   sorted(glob.glob(os.path.join(p, '*.str')))
        else:
            out += sorted(glob.glob(p))
    seen, uniq = set(), []
    for p in out:
        if p.lower() not in seen:
            seen.add(p.lower())
            uniq.append(p)
    return uniq


# The PC release names some of the same movies differently from the PS1 disc
# (the JPN PC table keeps OJ/PJ, but the USA PC AVIs are ou/pu; the endings are
# eu4/eu5 on PC against ed4/ed5 on the PS1; the staff rolls carry an _r/_b
# suffix). Alias so the retiming can still find them.
PC_ALIASES = {
    'oj': 'ou', 'pj': 'pu',
    'ed4': 'eu4', 'ed5': 'eu5',
    'stfc': 'stfc_r', 'stfj': 'stfj_r',
    'stfz': 'stfz_r', 'staf': 'staf_r',
}


def pc_duration(name, dirs, cache={}):
    """Duration of the shipped PC AVI for this movie, or None."""
    key = name.lower()
    if key in cache:
        return cache[key]
    cache[key] = None
    wanted = {key, PC_ALIASES.get(key, key)}
    for d in dirs:
        if not os.path.isdir(d):
            continue
        for f in os.listdir(d):
            if os.path.splitext(f)[0].lower() not in wanted or \
                    os.path.splitext(f)[1].lower() != '.avi':
                continue
            out = subprocess.run(
                ['ffprobe', '-v', 'error', '-show_entries',
                 'format=duration', '-of', 'csv=p=0',
                 os.path.join(d, f)], capture_output=True, text=True)
            try:
                cache[key] = float(out.stdout.strip())
            except ValueError:
                pass
            return cache[key]
    return None


def convert(mov, args, fmt, pc_dirs):
    print('%s: %d frames, %dx%d, v%d, %d audio group(s)%s'
          % (mov.name, len(mov.frames), mov.width, mov.height, mov.version,
             len(mov.audio) // 128,
             '  [TRUNCATED 2048-B source]' if mov.truncated_audio else ''))
    if mov.truncated_audio:
        print('  note: 2048-byte audio sectors dropped Form 2 groups 16-17; '
              'convert from a raw BIN/CUE (--bin) for clean audio')

    wav_path = None
    fps = None if args.fps == 'auto' else float(args.fps)
    duration_sectors = mov.video_sector_end or mov.sector_count
    source_dur = (duration_sectors / CD_SECTOR_RATE
                  if duration_sectors else None)
    source_fps = source_frame_rate(len(mov.frames), duration_sectors)
    audio_dur = None
    nframes = 0
    target = None
    asetrate = None

    if mov.audio and not args.no_audio:
        pcm, nframes = decode_xa(bytes(mov.audio), True)
        audio_dur = nframes / float(args.xa_rate)
        print('  audio: %.2f s @ %d Hz stereo' % (audio_dur, args.xa_rate))
        if not args.dry_run:
            fd, wav_path = tempfile.mkstemp(suffix='.wav')
            os.close(fd)
            write_wav(wav_path, pcm, args.xa_rate)

    if args.timing == 'pc':
        pc = pc_duration(mov.name, pc_dirs)
        if pc:
            target = pc
            if audio_dur:
                asetrate = nframes / pc
            print('  pc movie: %.2f s (retiming from %.2f s, audio pitch %+.1f%%)'
                  % (pc, audio_dur if audio_dur else 0,
                     (asetrate / args.xa_rate - 1.0) * 100 if asetrate else 0))
        else:
            print('  pc movie: none for this name, keeping PS1 timing')

    if source_dur:
        print('  source timing: %.2f s (%d/%d sectors, %.3f fps)'
              % (source_dur, duration_sectors, mov.sector_count, source_fps))

    if args.fps == 'auto':
        if target:
            fps = len(mov.frames) / target
        elif source_dur and source_fps > 0:
            fps = source_fps
        else:
            fps = len(mov.frames) / audio_dur if audio_dur else 15.0

    print('  fps: %.3f  duration: %.2f s' % (fps, len(mov.frames) / fps))
    try:
        encode(mov, args.out, fmt, fps, wav_path, args.dry_run, asetrate)
    finally:
        if wav_path and os.path.exists(wav_path):
            os.remove(wav_path)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('input', nargs='*', help='.STR files, globs or a folder')
    ap.add_argument('--bin', help='raw 2352-byte BIN image to read movies from')
    ap.add_argument('--filter', default='MOVIE',
                    help='substring an --bin path must contain (default MOVIE)')
    ap.add_argument('--out', required=True, help='folder for the .avi/.mp4')
    ap.add_argument('--format', default='both',
                    choices=['avi', 'mp4', 'both'])
    ap.add_argument('--timing', default='ps1', choices=['pc', 'ps1'],
                    help='"ps1" (default) keeps the STR sector timing; "pc" '
                         'retimes each movie to the duration of the shipped PC '
                         'AVI of the same name')
    ap.add_argument('--pc-movies', action='append',
                    help='folder holding the shipped PC Movie AVIs '
                         '(default assets/USA/Movie then assets/JPN/Movie)')
    ap.add_argument('--fps', default='auto',
                    help='"auto" (frames/duration) or a number')
    ap.add_argument('--xa-rate', type=int, default=37800)
    ap.add_argument('--no-audio', action='store_true')
    ap.add_argument('--dry-run', action='store_true')
    args = ap.parse_args()

    fmt = ('avi', 'mp4') if args.format == 'both' else (args.format,)

    movies = []
    if args.bin:
        fh = open(args.bin, 'rb')
        img = mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ)
        for path, (lba, size) in sorted(iso_files(img).items()):
            base = os.path.basename(path).split(';')[0]
            if '.STR' not in base.upper() or args.filter.upper() not in path:
                continue
            if size % 2048:
                continue
            movies.append(parse_bin(img, lba, size,
                                    os.path.splitext(base)[0]))
    else:
        files = collect(args.input)
        if not files:
            raise SystemExit('no .STR files matched')
        for path in files:
            movies.append(parse_str(path,
                                    os.path.splitext(os.path.basename(path))[0]))

    pc_dirs = args.pc_movies or [
        os.path.join(REPO, 'assets', 'USA', 'Movie'),
        os.path.join(REPO, 'assets', 'JPN', 'Movie'),
    ]
    for mov in movies:
        convert(mov, args, fmt, pc_dirs)


if __name__ == '__main__':
    main()
