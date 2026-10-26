RE1 Mod Helper

Hey! Start with docs/PORTING_PC_MODS.md. It explains how to bring an existing
PC mod to Android and helps you choose the right steps for its files.
For your first original texture mod, open docs/ANDROID_MODS.md instead.

This is a helper for checking and packaging supported files, not a universal
mod converter. It does not create missing fonts, repair game XML, change
model formats or implement Windows DLL/ASI behaviour. The Android launcher
has adapters for certain supported formats; not every PC pack matches them.

QUICK START
1. Extract this entire download into a new folder. Keep its files together.
2. Install Node.js LTS, version 20 or newer, from https://nodejs.org/.
3. Extract your mod download, including any inner 7z/RAR/ZIP archive.
4. Identify the mod using the guide. Compatible model/sound ZIPs import
   directly and do not need the translation command.
5. For a supported XML translation, check that it has manifest.txt,
   encoding.xml and its matching 256x256 Data/PBFONT0.tim, then drag its
   extracted folder onto Mod-Helper.cmd. Prepare-Rebirth-Mod.cmd is an
   older shortcut to the same helper.
6. If checks pass, read the generated .preparation.txt report beside the
   -android.zip, then install the ZIP through Mods > +. Test it in game.
   If checks fail, use the error table in the guide; the helper has not
   produced an installable ZIP from that failed run.

COMMANDS
Open Git Bash or PowerShell in this extracted folder, where tools/docs are.
The commands also work from the Android source root.

Create a texture mod:
node tools/mods/pack.js new ../my-first-pack --name "My first pack" --author "Your name"
node tools/mods/pack.js texture ../my-first-pack --game "C:/Games/Resident Evil/USA" --source "Stage1/RC1000.pak"
Edit the exported PNG, then:
node tools/mods/pack.js build ../my-first-pack

Check and package a supported translation:
node tools/mods/pack.js rebirth "C:/Mods/Extracted translation"
For separate movies, add --movies "C:/Mods/Separate Movie folder".
For another output name, add --out "C:/Mods/Translation-Android.zip".
Use --overwrite to replace a previous generated ZIP/report, not source files.
Keep outputs outside the input folder. Keep quotes around paths with spaces.

OLDER DESCRIPTION FILES
UTF-8 descriptions are unchanged. UTF-16 with a BOM is detected. Older
non-UTF-8 descriptions default to Windows-1252; check accents in Credits.
The ZIP gets a UTF-8 copy, and exact original credits remain in notices/.
If the description uses another known encoding, specify it, for example:
node tools/mods/pack.js rebirth "C:/Mods/Polish pack" --description-encoding windows-1250
This changes only description metadata. Game XML still needs valid UTF-8.

No APK rebuild, Android SDK or npm packages are needed. Checking a translation
does not need GOG files; exporting textures needs your own USA GOG installation.
The existing Android 1.1 APK works with this helper. No game/mod assets or
Node.js installer are included. Follow original mod authors' sharing terms.
