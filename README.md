## **Resident Evil (1996) Android Port**

**Current version: 1.01**

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
- Bluetooth and USB controller support with button remapping.
- Optional 16:9 mode, including rearranged inventory, item box and pickup
  screens that keep text and items in proportion.
- Quick turn, a togglable FPS counter and optional loading filenames.
- Experimental **More FPS** mode. It adds extra rendered frames without
  speeding up the game, but it does not guarantee 60 FPS.
- An optional locally built 1080p HD pack.
- An experimental Cheats tab with infinite health/ammo, healing, ammo refill,
  item box access and saving without an ink ribbon.

## What you need

- **Android 10 or newer**, a recent Android System WebView and working
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

**Mod support is in the works.** For now, use the original USA GOG files.
The built-in HD pack is available separately in the launcher.

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

Game files are read locally. The Android app has no Internet permission;
social links open through another app. Saves and settings live in the app's
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
