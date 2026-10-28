# Update the Android source and APK to 1.11

The current package is **version 1.11, build 47**. The source update is
cumulative from Android 1.1 and also applies over the earlier private 1.11
test builds. It includes improved mod support, background/timer fixes, Ignore
player cheat and foldable-phone fix. It contains the matching prebuilt web
engine, so installing the provided APK does not require compiling anything.

## Downloads

The prepared downloads are kept in the private RE1 repository. Sign in to the
GitHub account that has access:

- [Build 47 APK, source update and full source](https://github.com/RE4LeonMeow/-re1androidporttest/releases/tag/android-1.11-build47-docs1)

Find compatible mods in **#mods on the Discord server** and install them through
**Mods → +**. Follow the instructions supplied with each pack. Existing packs
can stay installed when updating the APK unless their instructions say otherwise.

## Update your source on Windows

1. Open Git Bash inside your **existing Android Git repository**, the folder
   containing `.git`. A downloaded full-source ZIP does not contain `.git`.
   Check the folder and remote:

   ```sh
   git status
   git remote -v
   ```

   If Git says `not a git repository`, stop and open your cloned repository
   folder instead. Save or commit your own unfinished edits before continuing.
   If your branch has an upstream, bring it up to date with `git pull --ff-only`.

2. Extract `ResidentEvil1996Android-1.11-build47-update.zip`. Open its inner
   folder, then copy **the contents** into the repository folder. Allow Windows
   to replace matching files. For example, the copied manifest should land at
   `YOUR-REPO/android/app/src/main/AndroidManifest.xml`, not inside another
   `ResidentEvil1996Android-1.11-build47-update` folder.

   `README.md` is deliberately excluded so your README edits stay intact. You
   can link to `docs/UPDATE_1_11.md` and `docs/RELEASE_1_11.md` from your README.
   No source-file deletion is needed for this update.

3. Review the source changes and commit them:

   ```sh
   git status --short
   git diff --stat
   git add .
   git commit -m "Release 1.11: improved mod support and foldable fixes"
   ```

   Keep game files, installed mod packs, saves and signing keys outside Git.
   Commit and publish only when the displayed file list is what you intend.
   Copying the source or committing it does not create a GitHub release.

The updated source is also committed in the private RE1 repository.
All prepared uploads remain in `RE4LeonMeow/-re1androidporttest`; RE2 is separate.
The complete source ZIP is also supplied if you need a fresh copy, but use the
update ZIP when preserving an existing repository and its README edits.

## Install and test the APK

Install `resident-evil-android-1.11-build47.apk` **over the existing app**.
It uses the same application ID and signing certificate as the earlier test
builds. Do not uninstall or clear app data to update. A save backup can be
exported from the launcher before installation.

For the foldable report, check that Logs identifies **1.11 (build 47)**, enter
gameplay, then fold and unfold several times. Try the same with Settings open:
the game should stay paused until you close it. Check both cover and inner
screens for correct controls and confirm no button remains held. If it still
returns to the launcher, copy the report immediately; it now records Activity
creation and display-configuration changes.

The APK compiles and passes the launcher tests. Browser tests use the actual
WASM engine and GLES 2/3 renderers to check session/memory retention, input
release, viewport framing and pause state across resizes. This is not a
physical Fold test; confirmation on the affected phone is still pending.

## When you publish 1.11

Use the text in [RELEASE_1_11.md](RELEASE_1_11.md) as the release description.
After your testing and source publication, create tag **v1.11** at your updated
commit and attach the build 47 APK. Its filename can keep the build number;
the version shown by Android is 1.11. Select a normal stable release and mark
it latest when you want the optional launcher update checker to offer it.
