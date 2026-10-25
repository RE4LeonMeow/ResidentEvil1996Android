# Building the Android edition

This repository contains the Android APK source and its WebView runtime. The
standalone browser packager/site is maintained in the separate browser repo.
The C++ engine and original upstream credits/licenses are included here.

Use Linux or WSL2 for the shell build. Install/activate Emscripten 3.1.69,
CMake, Ninja, Make, Python 3, Java 17+ and Android SDK platform 35/build-tools
35.0.0. The matching dependency sources are in dependency-sources/.

```sh
mkdir -p build/dependency-source/ffmpeg
tar -xzf dependency-sources/ffmpeg-n7.1.1-source.tar.gz --strip-components=1 -C build/dependency-source/ffmpeg
export RE1_FFMPEG_SOURCE="$PWD/build/dependency-source/ffmpeg"
export ANDROID_HOME=/absolute/path/to/android-sdk
bash tools/android/build-web.sh
bash tools/android/package.sh
```

Output: build/android/residentevil-debug.apk. The packager creates a private
build/android/debug.keystore if one does not exist. Preserve your own signing
key for future updates; keys are excluded from Git. The supplied APK retains
the established project signature. A newly generated key cannot update an
installation signed with another key; export saves before changing installs.
No game files are needed to build. Do not add game files or generated packs.

Useful checks: node --test tests/android/*.test.js and
python3 tests/check_platform_boundary.py. More build details: docs/ANDROID.md.
