# Update the Android repository from 1.01 to 1.1

This source update is for **ResidentEvil1996Android**, not the browser repo.
It includes everything since the public 1.01 controller fix: the mod loader,
SHDP and supported translation adapters, authoring tools, controller quick turn,
Android 9 compatibility work and the optional update checker.

## Replace the source on Windows

1. Open Git Bash **inside your existing Android repository folder**. Check
   the remote before doing anything:

   ```sh
   git remote -v
   git switch main
   git pull --ff-only origin main
   git status --short
   ```

   The remote should be `RE4LeonMeow/ResidentEvil1996Android`. If `git status`
   shows your own unfinished edits, commit or save them before replacing files.

2. Extract `ResidentEvil1996Android-1.1-update.zip`. Open its inner folder
   `ResidentEvil1996Android-1.1-update` and copy **its contents** into the
   Android repository folder. Let Windows replace matching files. For example,
   the copied README must become `YOUR-REPO/README.md`, not
   `YOUR-REPO/ResidentEvil1996Android-1.1-update/README.md`.

3. Review and push from Git Bash:

   ```sh
   git status --short
   git diff --stat
   rm -f ANDROID9-TEST.md
   git add .
   git commit -m "Release 1.1: Android mods and optional update checker"
   git push origin main
   ```

   Do not copy your game folder, installed mods, save data or signing keystore
   into the repository. The supplied update ZIP contains source, documentation
   and compatibility metadata; no commercial game/mod assets or private keys.

The obsolete `ANDROID9-TEST.md` from earlier previews has been removed. The
`rm -f` command above removes an existing local copy too; it
is safe when your 1.01 checkout never had that file.

## Create the public release yourself

1. Open the Android repo on GitHub, then **Releases → Draft a new release**.
2. Create tag **`v1.1`**, targeting your updated `main`, with title
   **Resident Evil Android 1.1**.
3. Add the release notes and attach `resident-evil-android-1.1.apk`.
   You can also attach the full source ZIP and standalone preparation tool.
4. Publish it as a normal stable release when you are ready. Do not mark it
   as a prerelease if you want stable update checks to include it. Leave
   **Set as the latest release** selected.

No release is created automatically by copying or pushing the source update.
GitHub also provides its own source archives after you publish the tag.

## Install the APK

Install the new APK over the existing app. It uses the same app ID and signing
certificate as the previous APK, so saves, settings, folder access and installed
packs stay in place. Export a save backup before updating. **Do not uninstall
or clear app data to update.** The browser edition does not change.

## How future update notifications work

Version 1.1 checks the latest stable release from the public Android repo.
It compares the release's **tag**, not source commits, file names or the build
number. Use tags such as **`v1.1.1`**, **`v1.2`** or **`v1.2.0`** for later
versions and also update the app's version before building that release.
The older public `v1.00`/`v1.01` labels are recognized as earlier versions.

The status text retries the check immediately. Automatic checks cache a
successful result for up to six hours to reduce GitHub requests. The Update
button appears only for a newer stable release and opens the public releases
page. It never downloads, installs or forces an update. Offline mode still lets
people launch their local game. Automatic
checks can be turned off in Controls & settings.

The old 1.01 APK has no update checker, so its users need this first upgrade
manually. It will not show a new update badge by itself.
