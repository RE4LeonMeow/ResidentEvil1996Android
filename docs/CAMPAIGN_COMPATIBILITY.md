# Experimental campaign compatibility

Android 1.11 improves compatibility with prepared campaign mods. Wesker's
Rebirth is one example of a supported mod. This does not make arbitrary
Classic REbirth campaigns or Windows plugins compatible.

## Install a compatible pack

Find packs in **#mods on the Discord server**. Install a compatible ZIP through
**Mods → +**, then follow its author's requirements, load-order instructions
and guidance on starting a new game. Existing compatible packs can stay
installed when updating the APK unless their instructions say otherwise.

Campaign saves are separate from normal-game saves. With the campaign enabled,
Save backups imports/exports its slots. Disable it to manage normal-game slots.
Keep the same campaign ZIP when loading its saves. Do not import original-game
saves into the campaign's slots.

Test status and known limits belong in the delivered pack's README. A startup
or rendering test does not establish that the entire campaign can be completed.
There is no claim of physical Android testing unless stated separately.

## What the engine handles

The prepared ZIP contains resource replacements, converted text, room artwork
and a small data profile. It contains no Windows EXE/DLL and does not run them.
Supported profiles can describe the following changes:

- New-game character, starting room/state, spawn position and initial inventory.
- Room sound tables and background-music assignments.
- A configurable whole-minute countdown (selected by the pack).
- Camera-mask textures stored inside the campaign's edited RDT rooms.
- Bordered background windows keep the TIM storage stride, including a
  320-pixel image displayed through a 316-pixel window. Existing texture-pack
  keys remain valid.
- The TIM stride adjustment applies to campaign replacement backgrounds.
  Original GOG underground/sewer images keep their legacy bordered row layout,
  even when their TIM headers report 320 pixels. Per-camera provenance is
  retained across cached camera changes, preventing diagonal background bands.
- Countdown text stays inside the visible 16:9 viewport.

The loader still accepts existing version 1 texture/data ZIPs. Campaign hooks
remain inactive without a validated profile. Only one campaign may be enabled.
Packs can request the original GOG font; other font/UI packs may conflict.

The new-game hook follows the public SDK's `fast chris` example:
https://github.com/Gemini-Loboto3/RE1-Mod-SDK
Engine code uses validated named settings rather than executing imported
instructions or writing imported memory addresses.

## Native pack format 2

A format 2 `mod.json` has the normal `textures` and `files` arrays, plus:

```json
{
  "format": "re1-android-mod",
  "formatVersion": 2,
  "game": "re1-usa",
  "id": "my-campaign",
  "name": "My campaign",
  "runtime": {
    "campaign": "campaign.bin",
    "locale": "locale.bin",
    "originalFont": true
  },
  "textures": [],
  "files": []
}
```

`locale` is optional and uses the existing validated R1LT format. The mod helper
can package an already prepared native pack. It does not reconstruct DLL or
executable changes automatically.

`campaign.bin` uses little-endian R1CP version 1. It is bounded to 256 KiB and
must have no trailing bytes. The 32-byte header contains:

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | 4 bytes | `R1CP` |
| 4 | u32 | Version, 1 |
| 8 | u32 | Total byte count |
| 12 | u8 | Character, 0 or 1 |
| 13 | u8 | Skip character selection, 0 or 1 |
| 14, 16, 18 | i16 each | Starting X, Z, angle (angle 0–4095) |
| 20 | u16 | Initial card byte count, 1052 |
| 22 | u16 | Inventory byte count, 40 |
| 24 | u16 | Room sound row count, at most 203 |
| 26 | u16 | Music group count, 57 |
| 28 | u16 | Music room count, 224 |
| 30 | u16 | Countdown seconds: 0 uses 180, otherwise 60–3600 in whole minutes |

The body is, in order:

1. 1052-byte initial card. Only its initial-state payload at byte 512 onward
   applies to a **New Game**; the installed game's card header stays intact.
2. Forty inventory bytes: eight Jill item/count pairs, six Chris pairs and six
   Rebecca pairs.
3. 896 music-group indexes (224 rooms × four states); 255 means no group.
4. 57 × four music entries, each an ASCII basename prefixed by a u8 byte count,
   then a u8 loop boolean.
5. Room sound rows: a u16 row ID (`stage * 29 + room`, zero-based), then 48
   byte-count-prefixed ASCII basenames. Rows cannot repeat. Names may be empty,
   contain only letters, digits, `_+&.-`, have at most 63 bytes, and cannot
   contain `..`.

Campaign data cannot name save paths or executable hooks. Saves are scoped by
the validated pack ID under `/persist/CAMPAIGNS/<id>/SAVE`. Normal saves remain
under `/persist/SAVE`.

## Optional integration test

No game or mod payloads are stored in this source repository. With a privately
supplied prepared pack and your own installed game:

```sh
python tests/android/seamless_pack.py --campaign \
  --game /private/GOG/USA --pack /private/campaign.zip \
  --profile /private/campaign-test-profile --output /private/test-captures
```

The optional integration test has pack-specific expectations and exercises actual import, OPFS storage, WASM startup,
player control, inventory, quick turn, save-screen rendering, each prepared
texture's decoded pixels, and supplied movies. Adapt its expectations when testing a different pack. This is not a complete
campaign playthrough.
