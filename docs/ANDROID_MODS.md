# Make mods for the Android port

Hey! You can change a background, character texture or item without rebuilding
the APK. Start with one image, get it working, then add more. The launcher keeps
mods separate from the original game folder, so you can disable your pack later.

**Mods → ? → Mod-making guide** opens the public Android README, which links to this guide.
Download the Android source ZIP on your PC to get the authoring tool.

This guide is for the Android edition. Find community packs in **#mods** on
[Discord](https://discord.gg/5PVsXAgvM2), on the Resident Evil 1996 Porting server.
Use community packs from confirmed modders.

For a quick list of direct imports, packs that need preparation and code mods
that cannot run as imported ZIPs, see the [README compatibility guide](../README.md#mods-and-making-texture-packs).

## Install a pack

1. Select your own USA GOG game folder in the launcher.
2. Open **Mods**, tap **+** in the top right and choose the ZIP.
3. Wait for installation to finish, then launch. Keep several GB free for a
   large pack. A failed or cancelled reinstall keeps the previous pack.
4. Use the checkbox to enable/disable a mod. The top mod has priority when
   packs replace the same texture or data file. Changes apply next launch.

Packs can replace one image or many resources. Examples include character
reskins, item textures, room backgrounds and compatible sound/model swaps.
Install packs separately so you can test and toggle each one.

One larger example is the **full RE_SHDP_1.1.zip**, which can be imported
directly. Its patch-only and GitHub source-code ZIPs are not the full texture
pack. See the [pack-specific coverage notes](SHDP_COMPATIBILITY.md) for that
example; the steps in this guide apply to your own mods too.

## Prepare a Classic REbirth mod

Compatible model/sound replacements and Android texture packs already import as
described above. For a supported **Classic REbirth XML translation**, extract
its archives first, including any inner `mod_something.7z`, then run:

```sh
node tools/mods/pack.js rebirth "C:/Mods/Extracted translation" --out "C:/Mods/Translation-Android.zip"
```

On Windows, you can instead drag the extracted folder onto
`tools/mods/Prepare-Rebirth-Mod.cmd` after installing Node.js LTS. The tool
creates an Android ZIP and a plain-text preparation report, retaining the
translation files and original author credits. It does not rewrite the mod or
turn Windows code patches into portable game behaviour.

The [step-by-step preparation guide](CLASSIC_REBIRTH_MODS.md#prepare-a-translation-with-the-tool)
shows nested archives, separate Movie folders, the Windows helper and a manual
phone method. The tool needs no APK rebuild, npm packages or GOG installation.

## What you need to make a texture pack

- Your own USA GOG game files.
- The source code for this Android port, extracted into a folder.
- [Node.js](https://nodejs.org/) 20 or newer. Install the LTS version; no npm
  packages are needed. Open a new terminal after installing it.
- An image editor, such as GIMP, Krita or Paint.NET.

These authoring tools run on your PC. Open Git Bash or PowerShell **inside the Android source folder**. Check Node:

```sh
node --version
```

The commands below work in both terminals. Change `C:/Games/Resident Evil/USA`
to your own folder containing `Data`, `Stage1`, `Sound` and the other game
folders. Use quotes around paths with spaces.

## Make your first texture mod

A texture can be a background, character skin, clothing, item or menu image.
This first example uses one background. The Jill example below uses the same
workflow for a character reskin.

Create a working folder **beside** the source folder:

```sh
node tools/mods/pack.js new ../my-first-pack --name "My first pack" --author "Your name"
```

Export a background from your own game:

```sh
node tools/mods/pack.js texture ../my-first-pack --game "C:/Games/Resident Evil/USA" --source "Stage1/RC1000.pak"
```

The tool creates an ordinary PNG and adds its entry to `mod.json`:

```text
my-first-pack/
  mod.json
  textures/
    Stage1/
      RC1000.pak.png
```

Open that PNG in your editor, change it and save over the same file. **Keep the
filename and proportions.** You can paint at its original resolution or make
it larger. For a 320×240 background, 1280×960 and 1440×1080 both work. A
1920×1080 image has different proportions and will be rejected. The game's
16:9 option handles framing; do not crop your source artwork to 16:9.

Build the finished ZIP:

```sh
node tools/mods/pack.js build ../my-first-pack
```

This creates `my-first-pack.zip` beside the working folder. Copy the ZIP to
your phone and install it through **Mods → +**. You do not need Android Studio,
Emscripten or a new APK. Edit the PNG and run `build` again to update your mod.
Importing a new ZIP with the same `id` replaces that installed mod after the
new installation succeeds.

The exporter refuses to overwrite an existing PNG, so it cannot accidentally
replace your artwork with the original. Use `--overwrite` only when you want
to start that texture over.

## Example: edit Jill's textures

Jill's standard model is `Enemy/CHAR11.EMD`. This exports her texture sheet,
not an editable 3D mesh. Use a new working folder for a separate character mod:

```sh
node tools/mods/pack.js new ../jill-reskin --name "My Jill reskin" --author "Your name"
node tools/mods/pack.js texture ../jill-reskin --game "C:/Games/Resident Evil/USA" --source "Enemy/CHAR11.EMD"
node tools/mods/pack.js texture ../jill-reskin --game "C:/Games/Resident Evil/USA" --source "Enemy/CHAR11.EMD" --palette 1 --rule index
```

That creates two **256×256 PNGs** in `jill-reskin/textures/Enemy`:

```text
CHAR11.EMD.png
CHAR11.EMD.i0.p1.png
```

They show the same layout using two different colour palettes. Palette 0
shows the skin correctly but makes some clothes pink. Palette 1 shows the
blue clothing but makes the face look strange. **That is expected**, not a
broken export: the game uses the appropriate palette for different parts.
Edit both exports to change the full appearance. Editing only one can leave
parts unchanged.

Keep the face, hair, clothing and other pieces in exactly their original
positions. Do not rotate, move or rearrange them. Keep transparent pixels
transparent. Staying at 256×256 is a simple first test; larger square PNGs are
also supported within the limits below. Keep each filename, and leave the
exporter's `image`, `palette`, `rule` and `originalSize` entries in `mod.json`
as written.

```sh
node tools/mods/pack.js build ../jill-reskin
```

Install `jill-reskin.zip` through **Mods → +**, enable it, and place it above
other packs replacing the same character textures. Relaunch and check the
face, front/back of the clothes and seams in game. For changes, edit the
working PNGs, build again and reinstall the ZIP. Your working PNGs should
stay outside the game installation.

This changes the model's surface artwork. Changing the actual hair/body shape
needs a compatible model editor and model-file replacement; the texture tool
does not change geometry, skeletons or animations.

## Compatible PC model/file mods

You can import a ZIP of compatible PC resource replacements directly, without
a `mod.json`. Keep their game folders and filenames, for example:

```text
My model mod/
  USA/
    Enemy/
      EM1000.EMD
    Players/
      W01.EMW
```

The launcher matches these paths to your selected GOG folder. A wrapper folder
and `USA` folder are fine. Install through **Mods → +**, then use its checkbox
to turn it on or off before launching. The top mod wins file conflicts.
Windows DLL/ASI/EXE files are ignored; executable patches cannot run here.
Files from other RE games or incompatible PC versions are not converted.

Start with an unchanged GOG folder. If you already overwrote a model in that
folder, disabling the mod returns to that modified file, not to a backup the
launcher never had. Restore your original PC files first to toggle between
the original model and its replacement.

## RAR / 7z downloads

The launcher imports ZIP archives. If a mod download is `.rar` or `.7z`, open
it in an archive app (for example ZArchiver on Android), extract it, then
compress the extracted game folders as **ZIP**. Changing the filename's
extension does not change the archive format. Do not ZIP the RAR itself.

For a Jill model replacement, the resulting ZIP can look like:

```text
Claire.zip
  Enemy/
    CHAR11.EMD
```

Keep `CHAR11.EMD` as its original resource name. The launcher's display name
is separate from that filename. Import with **Mods → +** and enable the mod
before launching. A compatible model format is still required.

## Name and description

PC resource ZIPs can include an optional UTF-8 file called `mod-info.txt`:

```text
Name: Claire Redfield
Description: Replaces Jill's standard model.
You can continue the description on another line.
Author: Original mod author
Version: 1.0
```

Put this file alongside the game folders, either at the ZIP root or inside its
single wrapper folder. Keep only one `mod-info.txt`. `Name`, `Description`,
`Author` and `Version` are optional fields, matched without regard to case;
use `:` or `=` after a field name. Description continuation lines are allowed.
Save as UTF-8, with a maximum of 16 KiB. Names can contain up to 120 characters,
descriptions 2000, author names 160 and versions 40.

If there is no name metadata, the launcher shows a **Mod details** screen
before installation. Enter a name and optional description, then tap Install.
Cancel backs out without replacing an existing mod. Formal Android `mod.json`
metadata and supported packs' built-in details are already recognized.

Use **Edit details** on an installed mod to change its displayed name and
description. These edits are stored in the launcher and do not rename the
model, change its files or affect enable/disable settings. Reimporting the same
PC ZIP filename suggests your previously saved details when metadata is absent.

## Add more images

Run `texture` again with another source file. Examples:

```sh
node tools/mods/pack.js texture ../my-first-pack --game "C:/Games/Resident Evil/USA" --source "Data/TYPE00.TIM" --rule opaque
node tools/mods/pack.js texture ../my-first-pack --game "C:/Games/Resident Evil/USA" --source "Item_m1/KAI03.DOR"
```

`TYPE00.TIM` is the save/load background. `KAI03.DOR` contains one of the stair
transition textures. Check the exported PNG before editing: model and door
textures are sheets containing multiple surfaces, not a finished scene.

The exporter can find TIM images inside `.tim`, unpacked `.pak`, `.dor`,
`.emd`, `.emw`, `.ivm` and other compatible containers. Some contain several
images or palette rows. Use `--image 1` for the second image or `--palette 1`
for the second palette. Both indices start at zero. `--rule opaque` is useful
for a fully opaque background; the default follows the normal texture rules.

Texture replacements match the image's original pixels. If the game reuses
those exact pixels in several places, your change appears in those places too.
An edit to your GOG installation can change that match; export and reinstall
the pack against the game files you actually use.

## Names, description and credits

Open `my-first-pack/mod.json` in a text editor. The important fields are:

```json
{
  "format": "re1-android-mod",
  "formatVersion": 1,
  "game": "re1-usa",
  "id": "my-first-pack",
  "name": "My first pack",
  "author": "Your name",
  "version": "1.0",
  "description": "My description",
  "credits": "Artwork by Your name",
  "textures": [
    {
      "source": "Stage1/RC1000.pak",
      "file": "textures/Stage1/RC1000.pak.png",
      "image": 0,
      "palette": 0,
      "rule": "opaque",
      "originalSize": [320, 240]
    }
  ]
}
```

Keep `id` the same for later updates. It uses lowercase letters, numbers,
dots, underscores or hyphens. Use a different `id` for a separate mod.
`originalSize` helps the build tool catch a wrong image shape; leave it as the
exporter wrote it. Credits appear in the mod's expandable Credits section.

PNG, WebP and JPEG images are supported. Each image must be at least the
original size, preserve its proportions, be no larger than 4096 pixels per
side and be smaller than 64 MiB. Use PNG/WebP with alpha for transparent
textures. JPEG has no transparency. By default, a fully opaque replacement
inherits the original texture's alpha mask. Add `"alpha": "image"` to its
entry if you want the imported image's alpha to replace that mask completely.

## Sounds, movies and game-data mods

Classic REbirth XML translations have a separate experimental adapter. See
[Classic REbirth compatibility](CLASSIC_REBIRTH_MODS.md) for font layouts,
document pages, ZIP preparation and the per-mod compatibility report. Model
mods keep their existing import paths.

For compatible raw replacements, create a `files` folder inside your pack:

```text
my-first-pack/
  mod.json
  files/
    Sound/
      gun01.wav
    Movie/
      pu.mp4
```

Those become `Sound/gun01.wav` and `Movie/pu.mp4` in the game's virtual folder.
The original files on your phone stay intact. `Movie/pu.mp4` is the modern
replacement for the GOG intro `pu.avi`; the engine prefers the MP4 sibling.
Use a standard H.264/AAC MP4. Imported Android MP4s play through WebView's media
decoder, so audio and video share one playback clock. Installed movies are
prepared as a disk-backed local video file, avoiding intercepted network media
requests and keeping the entire movie out of RAM. The temporary cache keeps
one completed movie plus a new movie while it is being prepared.

The tool's generated manifest omits `files`, so that folder is discovered
automatically. If you add an explicit `files` array, list each replacement:

```json
"files": [
  {"target": "Sound/gun01.wav", "file": "files/Sound/gun01.wav"}
]
```

Compatible TIM/PIX/PAK images, model resources, RDT room data, DOR door/stair
data and supported audio/video files can be overlaid. Changing a model or
room file requires understanding its original format; the launcher does not
convert incompatible resource formats or fix malformed game resources. Windows DLL/ASI
plugins, executable patches, save files and configuration files cannot be
installed as game-data replacements.

## ZIP it manually, if you prefer

The ZIP must contain one `mod.json`, either at its root or inside one wrapper
folder. Keep the referenced paths beside that manifest. If you omit the
`textures` array, the launcher also recognizes:

```text
textures/Stage1/RC1000.pak.png
```

That replaces the first image/palette in `Stage1/RC1000.pak`. For multiple
images, palettes or explicit alpha rules, use manifest entries instead.

The provided `build` command validates the manifest and includes only its
referenced assets, plus optional `README.txt` and `LICENSE.txt`. It does not
copy the whole GOG folder into your ZIP.

## If it does not work

- **No visual change:** enable the mod, restart the game and check whether
  another mod above it replaces the same texture. Test the exact room/camera
  whose PNG you edited.
- **Different game folder:** select the GOG folder used when installing it,
  or reinstall your ZIP for the newly selected folder.
- **Wrong proportions:** compare the edited PNG with `originalSize`. Export
  a clean copy into another working folder if needed.
- **Black boxes/edges:** keep transparency in PNG/WebP. Check whether you
  need the original alpha mask or `"alpha": "image"`.
- **ZIP rejected:** check for one `mod.json`, matching filenames, valid JSON
  commas/quotes and a supported USA game/format. Do not ZIP a Windows installer.
- **Only part of a character changes:** check its other palette exports.
  The Jill example above needs both palette 0 and palette 1 for a full reskin.
- **An adapted PC pack is missing textures:** check its compatibility report
  and coverage notes; arbitrary layouts may need a separate adapter.

Share your finished ZIP and screenshots in #mods, including a description,
version and credits. Include only artwork/data you have permission to share;
players provide their own game installation. Keep your working PNGs so the
next update is easy.
