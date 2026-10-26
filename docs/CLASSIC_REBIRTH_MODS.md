# Classic REbirth mods on Android

This is experimental **data compatibility**, not the Windows Classic REbirth
patch. Compatible resource mods and certain translation formats can run in
this port. A Classic REbirth requirement on a download page does not, by
itself, establish compatibility.

## Installing a translation

1. Download the translation from its author.
2. Extract it. If there is a `mod_something.7z`, extract that archive too.
3. Create one ZIP containing its `manifest.txt`, XML files, `Data`, `text`,
   `file` and any `Movie` folder. Keep the files together; do not put a 7z
   archive inside a ZIP. One outer wrapper folder is fine.
4. Select your USA GOG game folder, then open **Mods → +** and select that ZIP.
5. Read the mod's **Compatibility report**. It lists files used, skipped
   files and known limitations. Keep the translation above any
   other mod replacing the same font or UI textures.
6. Launch. Disable the translation in Mods and relaunch to return to the
   original language. The GOG folder and saves are not overwritten.

For example, the ZIP contents can look like this:

```text
manifest.txt
encoding.xml
misc.xml
desc.xml
item_simple.xml
save.xml
ascii.xml
text/
  ROOM_106.xml
  Room_20D_0.xml
Data/
  PBFONT0.tim
  STATUS.tim
file/
  FILEM_L00.png
  FILEM_L01.png
Movie/
  pu.mp4
```

The importer needs an `[MOD]` manifest and an `encoding.xml`. The current
custom-font adapter expects the 256×256 `Data/PBFONT0.tim` Latin atlas used by
the supplied Polish translation. Other atlas layouts need a separate adapter;
changing the filenames does not convert them.

## Prepare a translation with the tool

Use the source download's `tools/mods/pack.js`, or the standalone preparation
tool download. The standalone tool keeps the same folder structure and includes
the scripts it needs. Install **Node.js LTS, version 20 or newer**, from
[nodejs.org](https://nodejs.org/). No npm install, Android SDK or APK rebuild is
needed. Use an archive app such as 7-Zip to extract downloaded archives.

### Easy Windows method

1. Extract the translation download into its own folder. If it is split into
   `.zip.001`, `.002`, etc., keep all parts together and open `.001` with an
   archive program that supports split files.
2. Extract any inner `mod_something.7z`, RAR or ZIP too. The tool takes an
   **extracted folder**, not an archive file.
3. Keep the extracted translation and the author's Movie folder together. For
   the Polish download, this layout works:

   ```text
   Polish download/
     mod_spolszczenie/
       manifest.txt
       encoding.xml
       misc.xml
       Data/
       file/
       text/
     Movie/
       pu.mp4
       eu4.mp4
       eu5.mp4
   ```

4. Drag **Polish download** onto `Prepare-Rebirth-Mod.cmd`. In the source ZIP,
   that helper is in `tools/mods`; in the standalone download it is at the top.
5. Read the result in the console. The tool creates
   `Polish download-android.zip` beside the input folder, plus
   `Polish download-android.preparation.txt`.
6. Copy the generated ZIP to your phone and install it using **Mods → +**.
   Keep the translation above other font/UI packs and check its report too.

The tool finds one extracted translation and a Movie folder beside it. Multiple
translations in one input are rejected: prepare each mod separately. Choose
the extracted mod download, not your installed GOG game folder.

### Git Bash or PowerShell

Open a terminal inside the Android source/standalone tool folder:

```sh
node tools/mods/pack.js rebirth "C:/Mods/Polish download" --out "C:/Mods/Polish-Android.zip"
```

If the movies are stored somewhere else, point to that Movie folder:

```sh
node tools/mods/pack.js rebirth "C:/Mods/mod_spolszczenie" --movies "C:/Mods/Translation download/Movie" --out "C:/Mods/Polish-Android.zip"
```

Keep quotes around paths with spaces. To replace an earlier generated ZIP and
report, rerun the command with `--overwrite`. The tool otherwise refuses to
overwrite them. Store outputs outside the input folders.

### What preparation does

- Finds the manifest and removes unnecessary outer wrapper folders from ZIP
  paths. It can merge the author's Movie folder into the finished ZIP.
- Checks UTF-8 XML, the supported text commands, fixed save-text cell limits,
  the supported font layout and document PNG dimensions.
- Copies supported file types without changing the text, image or media bytes.
  Original author credits and notices are kept.
- Leaves out Windows DLL/ASI/executable/launch-script files, backups and known
  unused desktop UI tables. The preparation report lists these exclusions.
- Reports missing font characters. Those characters would display as `?` if
  the game encounters them; a warning about `ä`, `è`, `é` or `à` does not mean
  Polish letters are missing.

The tool prepares the packaging; the Android importer does the actual native
text/font/document adaptation when installing. It does not emulate Classic
REbirth, convert executable patches, transcode movies/audio, add document pages
or automatically repair different room-script formats. Filenames, game version,
movie codecs, document mappings and full gameplay still need testing against
your GOG installation. A ZIP passing preparation is not a full compatibility
certification.
The generated ZIP may be larger than the original 7z download: files are
stored without re-encoding their images or media.

### Only have a phone?

The Node tool is optional. An archive app can do the same packaging manually:
extract the outer download and inner mod archive, then create one ordinary ZIP
with `manifest.txt`, `encoding.xml`, the other mod files and `Movie` at its root.
Import through Mods. You do not need to edit the translated dialogue.

### Other Classic REbirth mod types

| Download contains | What to do |
| --- | --- |
| Compatible Enemy/Players model files or Sound/Voice replacements | ZIP the original game folders and import directly; no translation tool needed |
| Full official RE1 SHDP 1.1 pack | Import the original full ZIP directly |
| Supported XML translation and font | Use the preparation tool or manual ZIP method above |
| Windows DLL/ASI/code patches or a different font/script format | Requires an engine feature or a dedicated adapter; repacking alone cannot make it work |

## What the translation adapter reads

| Source | Android use |
| --- | --- |
| `manifest.txt` / `description.txt` | Display name and original credits |
| `misc.xml` | USA global messages; entries outside the USA table are unused |
| `desc.xml` | Item examination descriptions |
| `item_simple.xml` | Item names and unexamined-item generic names |
| `text/ROOM_SRR.xml` | Room messages, with the original message indices |
| `text/ROOM_SRR_0.xml` / `_1.xml` | Chris/Jill-specific room messages |
| `encoding.xml` + `Data/PBFONT0.tim` | Character mapping and supported Latin glyphs |
| `file/FILEM_L00.png`, etc. | Existing GOG document pages, including page zero |
| `save.xml` | Labels in the existing GOG save screen; fixed reveal cells remain fixed |
| `ascii.xml` | Yes/No labels |
| Compatible `Data/*.TIM`, `Item_m2/*.IVM`, etc. | Existing game-file overlay |
| Compatible `Movie/*.mp4` | Existing local movie playback |

`SRR` means the one-based stage digit plus the two-digit hexadecimal room.
Keep the author’s indices and scenario suffixes. The reader compiles the
usual color, item-name, reveal, page, timing and choice markup during import.
It preserves GOG prompt actions instead of applying another region's action
bytes to the USA engine. If a translated prompt cannot be matched safely, it
keeps that original prompt and records a `[MOD TEXT]` message in Logs.

This does not convert arbitrary Japanese room scripts into USA scripts.
Extra messages belonging to another engine/version are not new game content.
Room numbering, message order, cutscene timing and full story progression
still need testing with each translation.

Document PNGs must match an existing GOG page and fit its original texture
surface. They are padded without rescaling or modifying their supplied pixels.
This adapter does not add pages or rebuild the document navigation tables.
Native-size translated font and document images keep the normal framebuffer;
enabling a larger texture pack still selects its existing HD output resolution.

Classic REbirth's `save2.xml` alternate UI, its `config.xml` desktop controls,
and the desktop-only portions of `ascii.xml` are not replacements for this
launcher's settings. The compatibility report calls these out. The current
font adapter keeps the USA 8-pixel text cells; packs depending on wider or
variable-width glyphs need additional adaptation. Missing font characters are
reported and use `?`.

## Voice and sound packs

Compatible PCM WAV replacements can use the original game folders and names:

```text
Sound/
  gun01.wav
Voice/
  original-dialogue-name.wav
```

Use the actual names from the selected GOG installation. OGG Vorbis siblings
can also be imported when a matching original WAV exists. The engine already
prefers that OGG sibling when loading the WAV resource. Installing both a WAV
and its OGG sibling makes the OGG take precedence.

An AI dub or another language dub still needs its own test. Different voice
filenames, packed audio banks, unsupported codecs or DLL-dependent behaviour
are not automatically converted. Voice timing can also differ from the
original room/cutscene timing.

The Brazilian Portuguese AI dub at Nexus Mods `/residentevil1996/mods/112`
has not yet been verified from its archive; the public listing alone is not
a compatibility test.

## Combining translations with other mods

Compatible model-file replacements use their existing game paths. Texture,
audio and translation packs can be enabled together; priority is shown in Mods.
A translation can change the hashes of UI art, so a separate texture pack may
not recognize those translated images. They can retain their original
resolution while other backgrounds/textures still use that texture pack.
Keep the translation above any pack replacing the same font/UI art.

## Unsupported mod types

Windows `ddraw.dll`, ASI plugins, executables, launch scripts and binary code
patches are not executed. Mods that add engine behaviour need a portable
implementation and separate testing. RAR/7z archives must be extracted and
repackaged as ZIP; renaming the extension is not conversion.

Keep the original authors' credits and follow their redistribution terms.
Use your own game files for testing. The port's source and APK do not include
translation artwork, replacement videos, models or original game data.
