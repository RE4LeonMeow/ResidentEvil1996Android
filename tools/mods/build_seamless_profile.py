#!/usr/bin/env python3
"""Build a hash-only SHDP adapter from the authors' low-resolution reference ZIP.

Reference and finished images are read locally, never added to source or APK.
Requires Python Pillow and a C++ compiler. The Android importer needs only the
resulting metadata and the user's separately downloaded official SHDP ZIP.
"""
import argparse
import ctypes
import io
import json
import pathlib
import subprocess
import tempfile
import zipfile

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--reference', type=pathlib.Path, required=True)
parser.add_argument('--pack', type=pathlib.Path, required=True)
parser.add_argument('--output', type=pathlib.Path, required=True)
args = parser.parse_args()
records = {}
matched = 0
skipped = []
with tempfile.TemporaryDirectory(prefix='re1-shdp-profile-') as temp:
    module = pathlib.Path(temp) / 'profile-key.so'
    subprocess.run(['c++', '-O2', '-std=c++17', '-shared', '-fPIC', '-I', str(ROOT / 'src'),
                    str(ROOT / 'tools/mods/profile_key.cpp'), '-o', str(module)], check=True)
    key = ctypes.CDLL(str(module)).re1_mod_profile_key
    key.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
    key.restype = None
    with zipfile.ZipFile(args.reference) as reference, zipfile.ZipFile(args.pack) as pack:
        available = {n.lower(): n for n in pack.namelist()}
        # Later groups have priority when the original pixels are identical.
        for group in ['misc', 'skin', 'skin2', 'mask', 'bgd']:
            for name in sorted(reference.namelist()):
                if not name.startswith('dump/' + group + '/') or not name.lower().endswith('.webp'):
                    continue
                # TeamX groups its low-resolution misc references by purpose
                # (fonts/effects/UI); the finished pack flattens that folder.
                target = available.get(('hires/' + group + '/' + pathlib.PurePosixPath(name).name).lower())
                if not target:
                    continue
                original = Image.open(io.BytesIO(reference.read(name))).convert('RGBA')
                finished = Image.open(io.BytesIO(pack.read(target)))
                w, h = original.size
                ow, oh = finished.size
                if min(w, h) < 1 or w > ow or h > oh or max(ow, oh) > 4096 or ow * oh > 16777216 or ow * h != oh * w:
                    skipped.append({'file': target, 'original': [w, h], 'replacement': [ow, oh]})
                    continue
                # Reference dumps expand RGB555 channels by bit replication.
                # GOG's renderer instead uses floor(value * 255 / 31). Preserve
                # a direct-byte variant too for 24/32-bit UI surfaces. Finished
                # HD pixels are never quantized by this compatibility step.
                lut = [(v >> 3) * 255 // 31 for v in range(256)]
                quantized = original.point(lut * 3 + list(range(256)))
                # TeamX uses literal RGB(1,0,0) for opaque near-black texels;
                # GOG expands the corresponding nonzero RGB555 value to 8.
                sentinel = bytearray(quantized.tobytes())
                reference_pixels = original.tobytes()
                for at in range(0, len(sentinel), 4):
                    if reference_pixels[at:at + 3] == b'\x01\x00\x00':
                        sentinel[at] = 8
                sentinel = Image.frombytes('RGBA', (w, h), bytes(sentinel))
                # TeamX expands narrow GOG texture pages to 256x256 by
                # repeating texels. Recover only demonstrably lossless page
                # layouts; retain every pixel of the finished HD texture.
                layouts = [(1, 1, None)]
                if group != 'bgd':
                    for sx in [1, 2, 4]:
                        for sy in [1, 2, 4]:
                            if (sx, sy) == (1, 1) or w % sx or h % sy:
                                continue
                            small = original.resize((w // sx, h // sy), Image.Resampling.NEAREST)
                            if small.resize((w, h), Image.Resampling.NEAREST).tobytes() == original.tobytes():
                                layouts.append((sx, sy, None))
                if group == 'mask':
                    bounds = original.convert('RGB').getbbox()
                    if bounds:
                        for cw in [64, 128, 256]:
                            for ch in [64, 128, 256]:
                                if cw <= w and ch <= h and (cw, ch) != (w, h) and bounds[2] <= cw and bounds[3] <= ch:
                                    layouts.append((1, 1, (0, 0, cw, ch)))
                if name.startswith('dump/misc/icons/') and (w, h) == (64, 64):
                    # TeamX dumps each 40x30 inventory icon in a padded
                    # 64x64 tile. The PC port composites those icons into
                    # changing VRAM pages, so match the sprite region itself.
                    layouts.append((1, 1, (0, 0, 40, 30)))
                unique = set()
                for sx, sy, crop in layouts:
                  lw, lh = (crop[2], crop[3]) if crop else (w // sx, h // sy)
                  for parent in [original, quantized, sentinel]:
                    image = parent.crop(crop) if crop else parent.resize((lw, lh), Image.Resampling.NEAREST)
                    base = image.tobytes()
                    black = bytearray(base)
                    opaque = bytearray(base)
                    for at in range(0, len(base), 4):
                        opaque[at + 3] = 255
                        black[at + 3] = 255 if base[at] or base[at + 1] or base[at + 2] else 0
                    for pixels, alpha in [(base, 'image'), (bytes(black), 'black'), (bytes(opaque), 'opaque')]:
                        result = ctypes.create_string_buffer(64)
                        key(pixels, lw, lh, result)
                        digest = result.value.decode('ascii')
                        if digest in unique:
                            continue
                        unique.add(digest)
                        policy = 'opaque' if group == 'bgd' else 'image' if group == 'mask' else alpha
                        if group in ['skin', 'skin2'] and policy == 'image':
                            policy = 'opaque' if all(base[n] == 255 for n in range(3, len(base), 4)) else 'image'
                        record = {'key': digest, 'file': target, 'alpha': policy}
                        if crop:
                            record['crop'] = [*crop, w, h]
                        elif ow * lh != oh * lw:
                            record['layout'] = 'page'
                        records[digest] = record
                matched += 1
        credits_path = next((n for n in pack.namelist() if n.lower() == 'mod_shdp/description.txt'), None)
        credits = pack.read(credits_path).decode('utf-8-sig') if credits_path else 'Seamless HD Project: MoArtis, Kayael, Doze_D, FrankWesker and contributors.'
        # The TeamX save background contains its grey menu panel; GOG's
        # TYPE00.TIM is the same scene before that panel is composited. These
        # two keys are its transparent-black and opaque upload variants.
        # Verified against the local USA GOG file: all pixels outside the
        # panel (x=10..309, y=45..200) match the RGB555-normalized reference.
        typewriter = available.get('hires/bgd/d1d144a8.webp')
        if typewriter:
            for digest in ['8f3f021ceb0a101061ea315c7bf57ff2_320x240', 'c0b97c19e54c09af26bd080ff29777e7_320x240']:
                records[digest] = {'key': digest, 'file': typewriter, 'alpha': 'opaque', 'path': 'Data/TYPE00.TIM'}
        # USA hall masks differ from TeamX's reference by ONE palette texel
        # each, besides the known near-black sentinel. Verified pixel by pixel:
        # OSP00061: (103,80); OSP00065: (196,5). Use exact source hashes, never
        # fuzzy matching or modified HD pixels. Reuse an already installed,
        # identically sized/uncropped payload so build-33 users need no reimport.
        for filename, reuse, source, digests in [
            ('60C0C99A', '28ba97ca77e0ce14922d9f5cb7c9af58_256x256', 'Objspr/OSP00061.pak',
             ['33f7fae10cb8ca3f9e696b6738ba8c4c_256x256', '8c744cf1862b6e7fe06391afd4ba40a9_256x256']),
            ('C7D4615A', '189ac79f3812612bd1ef95bf6149fc80_256x256', 'Objspr/OSP00065.pak',
             ['73db04c377d78847e848c4b3267cd519_256x256', 'e4adf31ae789e8787b7250a04d3d18e9_256x256'])
        ]:
            target = available.get('hires/mask/' + filename.lower() + '.webp')
            if target and reuse in records:
                for digest in digests:
                    records[digest] = {'key': digest, 'file': target, 'alpha': 'image', 'path': source, 'reuse': reuse}
        parents = {}
        for record in records.values():
            if record['key'].endswith('_64x64') and not record.get('crop'):
                parents.setdefault((record['file'], record['alpha']), record['key'])
        for record in records.values():
            if record.get('crop') == [0, 0, 40, 30, 64, 64]:
                parent = parents.get((record['file'], record['alpha']))
                if parent:
                    record['reuse'] = parent
                    record['path'] = 'Data/ITEM_ALL.PIX'
profile = {'format': 're1-seamless-adapter', 'version': 1, 'revision': 4,
           'officialPack': 'RE_SHDP_1.1.zip',
           'project': 'https://www.moddb.com/mods/resident-evil-seamless-hd-project',
           'reference': 'TeamX mask generator v20220731 original texture reference',
           'credits': credits, 'records': sorted(records.values(), key=lambda r: r['key'])}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(profile, separators=(',', ':')), encoding='utf-8')
print(f'Wrote {len(records)} texture aliases for {matched} reference images; no texture pixels included')
print('Unsupported image dimensions:', json.dumps(skipped))
