# Update Mod Helper and the guides

This updates the Android repository's documentation and PC helper. It also
includes the earlier description-encoding hotfix, so you only need this one
source update ZIP. The released **Android 1.1 APK stays the same**.

## 1. Open your Git repository

Open Git Bash in your existing Android clone. If you followed the earlier
instructions, it is `Documents/re1-android-publish`:

```sh
cd ~/Documents/re1-android-publish
git status --short
git remote -v
git switch main
git pull --ff-only origin main
```

The remote should point to `RE4LeonMeow/ResidentEvil1996Android`. If you have
unfinished edits, commit or save them before copying replacements.

**"Not a git repository"?** You opened an extracted source ZIP. That download
does not have Git history. Use your real clone, or make a fresh one:

```sh
cd ~/Documents
git clone https://github.com/RE4LeonMeow/ResidentEvil1996Android.git re1-android-helper-update
cd re1-android-helper-update
```

## 2. Copy the source update

Extract **ResidentEvil1996Android-1.1-helper-update.zip**. Open its inner folder,
then copy **the contents** into your Android Git clone and replace matching
files. Keep the clone's existing `.git` folder.

For example, the guide belongs at `YOUR-REPO/docs/PORTING_PC_MODS.md`, not
`YOUR-REPO/ResidentEvil1996Android-1.1-helper-update/docs/PORTING_PC_MODS.md`.
Do not copy your game folder, installed mods or saves into the repository.

## 3. Push the changes

In Git Bash, inside that same clone:

```sh
git status --short
git diff --stat
git add README.md CHANGELOG.md docs/ tools/mods/ tests/android/mod_tools.py
git commit -m "Clarify mod support and add beginner Mod Helper guides"
git push origin main
```

This updates the public README and the guides linked from the launcher's
Mod-making guide button. The browser repository and APK are unchanged.

## 4. Update the release downloads

1. Open your Android repo's existing **v1.1** release and choose **Edit**.
2. Upload **RE1-Mod-Helper.zip** as the new standalone helper download.
3. Remove the older preparation-tool ZIP from that release to avoid confusing
   people. Keep its existing APK asset.
4. Optionally attach **ResidentEvil1996Android-1.1-helper-update.zip** too.
5. In the release description, call it **Mod Helper — checks and packages
   supported files; does not automatically convert incompatible PC mods**.
6. Save the release changes.

GitHub's automatic source ZIP/TAR for an existing tag still represents the
commit that tag points to. Pushing `main` does not update that tagged archive.
For the latest helper/docs source, use `main` → **Code → Download ZIP**, or the
attached helper-update ZIP. You do not need to move the published tag or
create an APK version just for these helper/documentation changes.
