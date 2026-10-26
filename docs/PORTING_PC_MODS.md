# Bring a PC mod to Android, step by step

Hey! Start with one small mod. You do not need to build an APK to install
compatible replacement files. This guide helps you find the right files,
make the ZIP and check whether it actually works.

**Mod Helper checks and packages supported files. It is not a universal mod
converter.** A ZIP can change how files are stored; it cannot change a model's
format or add the Windows features a mod depends on. Some formats have adapters
in the Android launcher, and those adapters have limits.

## 1. Get the files out of the download

1. Download the mod from its author or a confirmed modder in **#mods** on the
   [Discord server](https://discord.gg/5PVsXAgvM2). Keep the author's README.
2. Extract the download into a new folder. On Windows, 7-Zip can extract ZIP,
   RAR and 7z archives. On Android, an archive app such as ZArchiver can do it.
3. If you find another archive inside, extract it too. For `.zip.001`, `.002`,
   etc., keep all parts together and open `.001` with a supporting archive app.
4. Keep the original download and make changes in a separate working folder.

On Windows, turn on **File Explorer → View → Show → File name extensions**
(Windows 10: **View → File name extensions**). This helps you spot a renamed
RAR or an accidental `mod-info.txt.txt` filename.

For example, this is ready to inspect:

```text
Extracted mod/
  Enemy/
    CHAR11.EMD
  README.txt
```

This still needs extracting:

```text
Extracted mod/
  mod_character.7z
```

Renaming `.rar` to `.zip`, or putting a RAR inside a ZIP, will not make its
contents importable.

## 2. Choose the route that matches the contents

| What you find | Go here |
| --- | --- |
| `Enemy`, `Players`, `Sound` or `Voice` with replacement game files | [Model and audio replacements](#3-model-and-audio-replacements) |
| `mod.json` with texture files made for this port | Import the author's ZIP directly through Mods → +. |
| Loose PNGs, a character texture sheet or a `hires` folder | [Texture packs](#4-texture-packs) |
| `manifest.txt`, `encoding.xml` and translated XML | [Translations](#5-classic-rebirth-translations) |
| A Windows installer, DLL/ASI plugin or EXE patch | [Mods that need code changes](#6-mods-that-need-code-changes) |

A download can contain more than one type. For example, a DLL may be bundled
beside usable model files. The launcher can use compatible replacement files;
it does not run that DLL. If the model depends on the DLL's behavior, copying
the model alone will not reproduce it.

## 3. Model and audio replacements

These often only need packaging. Mod Helper's translation command is not
needed for compatible file replacements.

### Example: a compatible Jill model

1. Look inside your selected USA GOG game folder. Jill's standard model is
   `Enemy/CHAR11.EMD`.
2. Check the mod author's installation instructions. If their compatible
   replacement is also `Enemy/CHAR11.EMD`, keep that path and filename.
3. Create a separate working folder like this:

   ```text
   My Jill mod/
     Enemy/
       CHAR11.EMD
     mod-info.txt
   ```

4. `mod-info.txt` is optional. To give the mod a name, save this as UTF-8:

   ```text
   Name: My Jill replacement
   Description: Replaces Jill's standard model.
   Author: Original author's name
   Version: 1.0
   ```

5. Select `Enemy` and `mod-info.txt`, then compress those selected contents as
   **ZIP**. On Windows, use **Compress to ZIP file** or **Send to → Compressed
   (zipped) folder**. A single outer wrapper folder is also supported.
6. Copy that ZIP to your phone, select your original USA GOG game folder,
   and install it with **Mods → +**. If you omitted metadata, enter its name
   and description in the launcher.

Do not rename an unrelated model to `CHAR11.EMD` and assume it is compatible.
The mesh, skeleton, animations and resource format must suit the target. The
helper does not turn a Blender/FBX/OBJ model into a working RE1 model. For a
simple reskin instead, use the [Jill texture walkthrough](ANDROID_MODS.md#example-edit-jills-textures).

### Example: replace one sound

Keep the original game's folder and basename:

```text
My sound mod/
  Sound/
    gun01.wav
```

ZIP the `Sound` folder and import it. Use a compatible PCM WAV. An editor such
as Audacity can export a **16-bit PCM WAV**; changing a file's extension does
not change its audio format. Voice replacements use the actual filenames in
the game's `Voice` folder. A compatible OGG Vorbis sibling can also replace a
matching original WAV; if both are installed, the engine prefers OGG.

For movies, use a compatible H.264/AAC MP4 with the appropriate GOG movie
basename in `Movie`. A movie that relies on a PC-specific decoder or different
name/timing still needs checking. The helper does not transcode audio/video.

## 4. Texture packs

**Already made for this port?** Import the ZIP. Its `mod.json` tells the
launcher which original images to replace.

**Just a PNG or a PC hires folder?** The launcher needs to know which original
image each picture replaces. A PC pack's hashed filenames may not describe
that mapping. Do not rename all its images at random or expect the helper to
work out every match. A pack with a dedicated adapter can be supported; the full
official RE1 SHDP 1.1 ZIP is one example, not a rule for all `hires` packs.

For a texture you can identify, use the [first texture mod guide](ANDROID_MODS.md#make-your-first-texture-mod):

1. Export the target image from your own GOG files. The tool creates its
   `mod.json` entry for you.
2. Edit that PNG or fit your permitted replacement artwork to the same layout.
   Keep its proportions and transparent areas. A character sheet's face,
   clothes and hair must stay in their original positions.
3. Build the ZIP and test it. Start with one obvious change so you can see
   whether it matches the intended image.

Texture painting changes the surface artwork. Changing body/hair shapes needs
a compatible model editor and model-file replacement.

## 5. Classic REbirth translations

Read the [translation requirements](CLASSIC_REBIRTH_MODS.md#check-the-files-before-trying-a-translation)
before running anything. You need the matching font as well as the XML:

```text
Extracted translation/
  manifest.txt
  encoding.xml
  misc.xml
  Data/
    PBFONT0.tim
  text/
    ROOM_106.xml
```

The font must use the supported layout; a file having the right name is not
enough. If it is missing, ask the author whether the PC pack uses Classic
REbirth's own font. Borrowing another translation's font can scramble letters.

1. Extract **RE1-Mod-Helper.zip** from the Android repo's Releases.
2. Install [Node.js LTS](https://nodejs.org/), version 20 or newer. Keep the
   helper's complete folder structure.
3. Drag your extracted translation folder onto **Mod-Helper.cmd**. In the
   source download, that CMD is under `tools/mods` instead.
4. If the checks pass, read the generated `.preparation.txt` report beside
   the new `-android.zip`.
5. Import that ZIP with **Mods → +**, read its Compatibility report, and test.

The helper checks supported XML commands, font dimensions and document sizes,
packages selected files, retains credits and handles older description
encodings. The launcher adapts supported text/font data during import.
Neither step repairs arbitrary translation formats. If checking fails, use
the table below; a failed run has not created an installable ZIP.

For terminal commands, separate Movie folders, supported XML editing and
manual phone packaging, use the [translation guide](CLASSIC_REBIRTH_MODS.md).
An HD language add-on may also require its base translation. Installing the
HD add-on alone will not supply that missing base.

## 6. Mods that need code changes

Windows DLL/ASI plugins, EXE patches, memory hooks, new engine behavior and
incompatible room scripts need a developer to implement the feature in this
port. The helper cannot turn them into data mods. Ask the author/developer
which parts are replacement assets and which depend on PC code.

Useful information includes the exact mod/version, its required PC game
version, the author's installation instructions, the required files and the
first helper/import error. A mod page saying "Classic REbirth required" alone
does not answer those questions.

## Common errors

| Message or symptom | What to do |
| --- | --- |
| ZIP END header not found | Make sure it is an ordinary ZIP, not a renamed RAR/7z, incomplete download or one part of a split archive. Extract first, then ZIP the contents. |
| Node is not recognized | Install Node.js LTS and open a new terminal. Check `node --version`. |
| Nested archive / no manifest | Extract the inner mod archive. Select the extracted translation folder, not its download ZIP or your whole game folder. |
| More than one translation found | Put each translation in its own working folder and prepare it separately. |
| Missing `Data/PBFONT0.tim` | Obtain the author's matching supported font. Packaging cannot generate it. |
| Font layout unsupported | The current adapter cannot read that layout. Renaming or resizing the atlas is not a reliable fix. |
| Old `description.txt must be UTF-8` error | Use the current helper. It handles common older descriptions and preserves their original credits. Check accents; see the guide for other encodings. |
| Game XML must be UTF-8 | Follow the Notepad steps below on a copy. Do not change `encoding.xml`'s glyph map to solve a file-encoding error. |
| Invalid XML / control characters | Check the named file with the author. Stray binary/control bytes are not ordinary translated text; do not delete unknown commands blindly. |
| Text follows an end marker / unterminated section | The entry uses markup the checker rejects. Compare the author's original; see the translation guide. It may need a deliberate text fix or adapter work. |
| Output already exists | Choose another `--out` filename, or use `--overwrite` to replace the earlier generated ZIP/report. |
| Output must be outside input | Save the result beside the input folder, not inside it. |
| No matching resources / no visible change | Check the target USA GOG paths, mod priority and whether that exact character/room is loaded. A PC resource may be incompatible. |
| Missing font character uses `?` | Check whether the affected character is actually used. Fixing it needs a matching glyph and character mapping. |

To save a known, readable XML file as UTF-8 on Windows:

1. Work on a copy and open the file in Notepad. Check that its letters and
   accents already display correctly. If they are scrambled, stop and find
   its original encoding first; saving scrambled text will keep it scrambled.
2. Choose **File → Save as**, select **All files**, keep the exact `.xml`
   filename and choose **UTF-8** in the Encoding box.
3. Save the working copy and run the helper again. This only changes how the
   text is stored; it does not repair unsupported commands or add missing glyphs.

## 7. Test before sharing

1. Use an unchanged USA GOG folder. Disabling a mod returns to those files;
   it cannot undo replacements you copied into the GOG folder yourself.
2. Test one enabled mod first. Check its report, then launch a new game or the
   relevant save. A successful install does not prove every scene works.
3. For a model, check movement, aiming and animations. For textures, check
   both sides of a character, seams, transparency and reused images. For audio,
   check the actual sound, speech timing and movie playback.
4. For translations, check dialogue choices, item use, inventory, readable
   documents and save/load screens. Use a separate save slot while testing.
5. Add other packs one at a time. Put translations above packs replacing the
   same fonts/UI. The top enabled mod wins a conflict. Relaunch after changes.
6. When reporting a problem, include the mod/version, Android port version,
   exact scene and **Settings → Logs → Copy report**, plus the helper's error
   or report if relevant. Report port bugs on the
   [Android GitHub Issues page](https://github.com/RE4LeonMeow/ResidentEvil1996Android/issues).

For sharing, keep the author's credits and follow their permissions. Include
the supported port version, installation steps, required base mods and known
issues with your ZIP. Community packs go in **#mods** through confirmed modders.
Players supply their own game files; do not bundle an entire GOG installation.
