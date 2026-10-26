## **Resident Evil (1996) Android Port**

**Current version: 1.1**

**Browser edition:** https://github.com/RE4LeonMeow/ResidentEvil1996Browser

Hey! This is my unofficial Android port of Resident Evil 1, based on
[ecruells/resident-evil-pc-decomp](https://github.com/ecruells/resident-evil-pc-decomp).
Huge thanks to ecruells and everyone who worked on the decompilation. This
port uses their engine and adds the Android launcher, touch controls and
other features below.

You bring your own GOG PC files, select the folder and play. **No game files
are included in the APK or this repo.**

## What it has

- Touch controls with arrows or a joystick, automatic arrows in menus,
  movable/resizable buttons, opacity settings and Hold/Toggle options.
- Bluetooth and USB controller support with button remapping and a configurable R3 quick turn.
- Optional 16:9 mode, including rearranged inventory, item box and pickup
  screens that keep text and items in proportion.
- Quick turn, a togglable FPS counter and optional loading filenames.
- Experimental **More FPS** mode. It adds extra rendered frames without
  speeding up the game, but it does not guarantee 60 FPS.
- An optional locally built 1080p HD pack.
- Optional update checks with a highlighted Update button. Offline play still works.
- Android ZIP mod support for compatible textures, models, audio, movies and
  translations. Enable/disable packs, edit their details and change priority.
- An experimental Cheats tab with infinite health/ammo, healing, ammo refill,
  item box access and saving without an ink ribbon.

## Mods and making texture packs

Hey! Find community packs in **#mods** on the **Resident Evil 1996 Porting**
[Discord server](https://discord.gg/5PVsXAgvM2). Use packs from confirmed modders
and read the installation notes. You can also find mods on **Nexus Mods**,
**Mod DB** and the original authors' sites. **Mods from these sites can work
with this port**, including compatible model swaps, character reskins, texture
replacements and sounds. Compatibility depends on the pack's file formats,
game version and required engine features, wherever you download it.

Select your own USA GOG game folder, open **Mods**, tap **+** and choose a ZIP.
The **?** button has the same quick compatibility guide. Mods are Android-only.

For Seamless HD, follow [Install SHDP in a few steps](#install-shdp-in-a-few-steps).

**New to modding?** Start with [Bring a PC mod to Android, step by step](docs/PORTING_PC_MODS.md).
It shows which files to use, how to ZIP them and when a mod needs more work.
To paint your own textures instead, use [Make your first texture mod](docs/ANDROID_MODS.md#make-your-first-texture-mod).

### Which mods can I install directly?

| Pack | What to do |
| --- | --- |
| A mod ZIP made for this Android port | Import it directly. |
| Compatible PC model swaps, character reskins, native texture-file replacements and sounds | Import the ZIP with its original game folders and filenames, such as `Enemy`, `Players`, `Data`, `Stage1`, `Sound` or `Voice`. The resources must match the USA GOG game and supported formats. Loose PNGs or arbitrary `hires` packs need texture mappings; see the [texture guide](docs/PORTING_PC_MODS.md#4-texture-packs). |
| Full official **RE_SHDP_1.1.zip** | Import directly. Use the full release pack, not its small patch or GitHub source-code ZIP. Compatible textures and MP4 movies are mapped by the importer. |

Compatible PC replacement ZIPs and the supported SHDP release do not need you
to create a `mod.json`. PC ZIPs can include `mod-info.txt` for
name and description; otherwise the launcher lets you enter them. Use
**Edit details** to change them later. RAR, 7z and split downloads need to be
extracted first, then ZIP the actual mod folders together. Do not put the RAR
or 7z file inside another ZIP.

### Install SHDP in a few steps

SHDP means **Resident Evil Seamless HD Project**. Use its full **RE1 PC pack**
with this Android port:

1. Open the [official RE1 Seamless HD download page](https://www.reshdp.com/re1/#downloads)
   and follow its PC/Mod DB download link. Get the full **`RE_SHDP_1.1.zip`**.
   The small update/patch and GitHub **Code → Download ZIP** source archive
   are not the full texture/movie pack. Packs for RE2, RE3 or emulators are
   different downloads.
2. Put **`RE_SHDP_1.1.zip` on your phone** and keep it as downloaded. Do not
   extract it, rename another archive to ZIP or remove its movie folders.
   It is a large download; keep several GB free for installation and temporary
   files.
3. Open **Resident Evil Launcher** and **Select game folder** for your own
   original **USA GOG RE1** files. If the game is running, return to the
   launcher first.
4. Open **Mods**, tap **+** in the top-right corner and select
   **`RE_SHDP_1.1.zip`**. Keep the app open and wait until it says
   **Mod installed and enabled**. Importing the full pack can take a while.
5. Check that Seamless HD is enabled in the Mods list. For your first test,
   leave other mods disabled so you can check SHDP by itself.
6. Close Mods and **Launch game**. Relaunch after changing enabled mods or
   their priority. With a supported translation added later, keep that
   translation above packs replacing the same font/UI.

**You do not need Mod Helper or Classic REbirth's Windows DLL for SHDP.**
Do not copy its files over your original GOG installation. The launcher uses
the supported parts directly from the imported mod.

**Install HD pack is a separate feature.** That button generates an upscaled
pack from your game files; it does not install SHDP. SHDP uses **Mods → +** and
works independently of the generated pack's **Use HD pack** checkbox.

If it does not look right:

- **No HD changes:** confirm the SHDP checkbox is on, relaunch, and read its
  Compatibility report. Check that you imported the full PC pack and selected
  the original USA GOG game folder.
- **Videos are skipped or missing:** turn off **Skip movies** in settings and
  check that you imported the complete ZIP, including its movie folders.
  A **Loading HD video** message can appear while a clip is prepared; let it
  finish.
- **Only a few textures remain original:** some USA GOG assets differ from the
  PC version SHDP was made for. See the [SHDP coverage notes](docs/SHDP_COMPATIBILITY.md)
  for known differences; this alone does not mean installation failed.
- **Import fails:** note the exact error and include **Settings → Logs → Copy
  report** when reporting it. Check free storage and that the download finished.

### What needs preparation or additional support?

- **Supported Classic REbirth XML translations:** put the extracted
  `manifest.txt`, `encoding.xml`, `Data/PBFONT0.tim`, XML/resource folders and
  optional movies in one ZIP. The launcher adapts supported text, fonts and
  document pages during installation; you do not need Classic REbirth on the
  phone. The supplied Polish translation was tested. Other translations need
  the same supported formats and font layout.
- The optional **Mod Helper** checks and packages translations that already
  meet the [supported format requirements](docs/CLASSIC_REBIRTH_MODS.md).
  Download `RE1-Mod-Helper.zip` from [Releases](https://github.com/RE4LeonMeow/ResidentEvil1996Android/releases),
  or use `tools/mods` in the source. Install Node.js LTS and drag the extracted
  translation folder onto **Mod-Helper.cmd**. The old
  `Prepare-Rebirth-Mod.cmd` name still works. **This is a checking and ZIP
  packaging helper, not a mod converter.** It cannot supply missing fonts,
  repair unsupported text commands, change model formats or run Windows code.
  A successful check still needs in-game testing. There are manual phone
  steps in the guide too.
- **Translated HD/hires add-ons:** install their required base translation
  first. Arbitrary `hires` layouts and hashes need a dedicated mapping, so the
  tool cannot guarantee them. An HD add-on is not necessarily a standalone translation pack.
- Different font atlases, text formats, skeletons or resources for another
  release need investigation and an adapter. Renaming files does not convert
  them. A voice pack using compatible replacement audio may work, but each
  untested pack still needs checking.

### What cannot run as an imported Classic REbirth mod?

Windows **DLL/ASI plugins, EXE patches, memory hooks and engine changes** do
not run in this Android engine. Mods relying on Classic REbirth-specific code,
Windows installers or another game's engine need their behavior reimplemented
in the port; the ZIP tool cannot do that. Different game modes and incompatible
room/script formats also need engine work. A Classic REbirth requirement on a
mod page alone does not tell you whether it only replaces data or needs code.

### Enable, disable and combine packs

Use each mod's checkbox before launching. The top mod wins when packs replace
the same resource. **Keep translations above other packs replacing the same fonts or UI.**
Changes apply on the next launch; removing a mod keeps your original files and
saves. Check **Compatibility report** after importing: it lists used/skipped
files and limitations. Skipped backups or desktop-only menus do not necessarily
mean a translation failed. A missing glyph can show `?` if that character is used.

Want to make your own? Start with the [step-by-step mod guide](docs/ANDROID_MODS.md).
It exports editable PNGs from your own files and builds installable ZIPs, with
no APK rebuild or npm packages. See [Classic REbirth compatibility](docs/CLASSIC_REBIRTH_MODS.md)
for supported files, common errors and limits. The [SHDP example](docs/SHDP_COMPATIBILITY.md)
shows how one larger PC texture pack is adapted. Keep original authors' credits and respect permissions
when sharing a prepared mod.

## What you need

- **Android 9 or newer** (Android 9 support is experimental), an updated compatible Android System WebView and working
  WebGL/OpenGL ES support.
- Your own installed/extracted **USA GOG PC version of Resident Evil 1**.
- Enough local storage for your game files, plus extra space if you build
  the optional HD pack.

## How to install

1. Download the APK from this repo's **Releases** page and install it. Android
   might ask you to allow installation from the app opening the APK.
2. Copy your complete installed GOG game folder to your phone. Keep `Data`,
   `Stage1` and the other stage folders, `Sound`, `Voice`, `Movie` and the
   remaining game folders together.
3. Open **Resident Evil Launcher**, tap **Select game folder** and select
   your installation or its `USA` folder.
4. Tap **Launch game** and enjoy!

Select the extracted game folder, not a ZIP, installer `.exe` or disc image.
The port does not run the Windows executable.

To update, install the newer APK over the existing app. Updates need the same
signing key. **Don't uninstall or clear app data if you want to keep your
saves and settings.** Use **Save backups -> Export saves** before uninstalling
or clearing data.

## Optional update checker

The launcher's top-left corner checks the latest **public Android GitHub
release**. If a newer stable version exists, it says **Update available** and
shows and lights up **Update**. The button stays hidden when your version is
current, while checking, or when a check fails. Tap Update to open the GitHub
releases page, where you can choose whether to download and install it.
Updates are never forced or installed by
the launcher, and checking does not block **Launch game**.

Without a connection it shows **Offline mode**. Tap the status text to retry
when you are online. Failed checks say **Couldn't check for updates**, rather
than pretending there is no update. Successful results are cached for up to
six hours; tapping the status does a fresh check.

Turn off **Controls & settings → Check for updates on launch** if you prefer.
You can still tap the status to check manually. This optional check requests
public release metadata from GitHub; it does not send game files, saves or
mods. Gameplay and mod installation use your local files.

## Save backups

Save in the game first, then return to the launcher and open **Save backups**.
**Export saves** lets you choose where to keep a `.saves.zip` file.
**Import saves** opens one of those backups and asks before replacing matching
save slots. Slots outside the backup, settings, game files and HD packs stay
as they are. Both editions use the same backup format, so you can move saves
between Android and browser. This backs up existing save slots, not an unsaved
moment in the game.

For your original USA GOG saves, put `savedat1.dat` through `savedat8.dat`
into a ZIP, then use **Import saves**. The ZIP can contain the files directly
or inside a `SAVE` folder. To use an exported ZIP on PC, extract its `.dat`
files into your GOG game's `SAVE` folder, backing up existing files first.

## Controls

Open the gear button or press Android Back for **Controls & settings**.

Controller quick turn defaults to **R3** (press the right stick). Change or
disable its shortcut under **Controller bindings → Quick turn shortcut**.
It turns once per press during player control.

| Button | What it does |
| --- | --- |
| Arrows / joystick | Move using the original tank controls |
| ACTION | Interact or confirm |
| RUN | Run or cancel |
| AIM | Ready your weapon; use ACTION while aiming to fire |
| ITEMS | Open inventory |
| PAUSE | Open in-game options |
| TURN | Quick 180-degree turn during normal movement |

In-game remapping can change these actions. For Yes/No prompts, use the
arrows to choose and ACTION to confirm.

Use **Edit touch layout** to move controls and change each one's size.
**Touch button behavior** lets you choose Hold or Toggle per button. Toggles
glow amber and release when you open settings, switch menus or leave the app.
Pair your controller in Android settings, then adjust its bindings in the
launcher if needed.

### AIM opens inventory, or PAUSE does nothing after loading a save

Original PC saves can carry a different controller layout. Load the affected
save, then open **Controls & settings → Fix game action bindings → Restore
game actions**. This restores the game's default action bindings without
resetting your touch layout, button sizes, Hold/Toggle choices or launcher
controller mappings. Save in-game again to keep the recovered bindings in that
slot. Loading another old slot can restore that slot's own layout, so repeat
if needed. The recovery button does not rewrite your save files by itself.

## Optional HD pack

Select your game folder, then tap **Install HD pack** in the launcher. It
builds replacements locally using the bundled FSRCNN upscaler. Backgrounds
become **1440x1080**, keeping their original proportions. Videos stay original.

Keep the app open while it builds. There is a progress bar, elapsed timer and
an estimated time remaining once enough work has been measured. Building can
take a long time and several GB of storage.

Choose Natural, Crisp or Sharp before building. Use **Reinstall HD pack** to
apply a different sharpness later. **Use HD pack** changes the next launch;
**Remove HD pack** deletes the generated pack. Your original game files stay
intact. The pack uses more memory and GPU power during gameplay.

## FAQ

### Does this include the game?

No, you need your own legally obtained game files. The APK contains the
launcher and engine.

### Is this a Windows emulator? Why does it use WebView?

The engine is compiled to WebAssembly and runs inside Android WebView. This
keeps the original 32-bit engine compatible with modern phones. It loads the
game data directly instead of running the PC executable.

### Can I use mods?

Yes. The Android launcher installs compatible texture and game-data ZIPs
through **Mods → +**. This includes texture packs, compatible character/model
replacements, audio mods and supported translations. See the
[mod-making guide](docs/ANDROID_MODS.md) to make your own. Windows DLL/ASI
plugins require their original Windows engine and cannot run here.

### Why isn't everything 16:9?

Gameplay and supported menus have widescreen layouts. Movies, scripted
cutscenes, save screens and game options keep their complete original image.

### Does More FPS make the game run faster?

No, game speed and timers stay the same. It adds interpolated render frames
in supported scenes. It can increase heat and GPU load, so try it and see how
your phone handles it.

### I get a black screen or the game won't launch. What should I try?

Check that you selected the full game folder and update Android System
WebView. Try **Test graphics & sound**, then **OpenGL ES 2 - compatibility**
in settings and relaunch. **Skip movies** is also available for troubleshooting.
There is no Vulkan renderer.

If it still fails, open **Logs -> Copy report** and include it with your phone
model, Android version and what you were doing when the problem happened.

### Are my files uploaded? Where are my saves?

Game files are read locally. The optional update checker contacts GitHub for
public release metadata; it does not upload your files. Social and update
links open through another app. Saves and settings live in the app's
private storage, separate from your original game folder. Clearing app data
or uninstalling removes them.

### Are cheats safe to use?

They are experimental and can break progression or saves. Use a separate save
slot. Turning cheats off does not undo changes you already saved.

## Credits

Playtesters:

- Kali Kyle
- Vhmed_hosny
- khoilinux

thank you to everyone who tested the port and helped find bugs <3

## Links

- [Discord](https://discord.gg/5PVsXAgvM2)
- [X / Twitter](https://x.com/4wu4_)
- [GitHub](https://github.com/RE4LeonMeow)

## Source and licensing

The original engine and decompilation are by
[ecruells and contributors](https://github.com/ecruells/resident-evil-pc-decomp).
Widescreen code adapts work from
[ResidentEvilClassicTrilogy.FusionFix](https://github.com/Fusion-Fix/ResidentEvilClassicTrilogy.FusionFix).
The HD upscaler uses the pretrained model from
[Saafke/FSRCNN_Tensorflow](https://github.com/Saafke/FSRCNN_Tensorflow).

Code is distributed under [GPL-3.0](LICENSE), with GPL-3.0-or-later notices
for the adapted FusionFix code. Emscripten, SDL2, FFmpeg, zlib, the upscaler
and social icons have their own bundled notices. Keep the copyright and
license notices when sharing builds or source. Build instructions are in
`docs/ANDROID.md` in the source tree.

Resident Evil belongs to Capcom. Game assets, launcher artwork and trademarks
are not covered by the code license. This is an unofficial fan project,
unaffiliated with Capcom or GOG. Original game data and generated image packs
are not included in this repo or the downloads.
