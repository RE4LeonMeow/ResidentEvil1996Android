#!/usr/bin/env bash
# Build the Java shell without downloading Gradle/Maven dependencies.
set -euo pipefail
cd "$(dirname "$0")/../.."
: "${ANDROID_HOME:?Set ANDROID_HOME to an Android SDK with platform 35 and build-tools 35.0.0}"
bt="$ANDROID_HOME/build-tools/35.0.0"
jar="$ANDROID_HOME/platforms/android-35/android.jar"
out="$PWD/build/android"
web="$PWD/android/app/src/main/assets/web"
for name in residentevil.js residentevil.wasm hd-resize.js hd-resize.wasm; do
  test -s "$web/$name" || { echo "Missing $name; run tools/android/build-web.sh first" >&2; exit 1; }
done
# Rebuild generated classes/Dex; retain the signing key for in-place updates.
python3 - "$out" <<'PYCLEAN'
import pathlib,shutil,sys
out=pathlib.Path(sys.argv[1])
for name in ('classes','dex'):
    path=out/name
    if path.exists():shutil.rmtree(path)
    path.mkdir(parents=True)
PYCLEAN
# Gradle supplies namespace; aapt2 invoked directly needs it in its input manifest.
python3 - "$out/AndroidManifest.xml" <<'PYMANIFEST'
import pathlib,sys
source=pathlib.Path('android/app/src/main/AndroidManifest.xml').read_text()
source=source.replace('<manifest ', '<manifest package="org.re1.launcher" ',1)
pathlib.Path(sys.argv[1]).write_text(source)
PYMANIFEST
"$bt/aapt2" compile --dir android/app/src/main/res -o "$out/resources.zip"
"$bt/aapt2" link -o "$out/shell.apk" -I "$jar" --manifest "$out/AndroidManifest.xml" -A android/app/src/main/assets "$out/resources.zip" --min-sdk-version 28 --target-sdk-version 35 --version-code 47 --version-name 1.11 -0 wasm
javac -source 17 -target 17 -classpath "$jar" -d "$out/classes" android/app/src/main/java/org/re1/launcher/*.java
find "$out/classes" -name '*.class' -print > "$out/classes.list"
"$bt/d8" --lib "$jar" --min-api 28 --output "$out/dex" @"$out/classes.list"
python3 - "$out" <<'PY'
import sys,zipfile,pathlib
out=pathlib.Path(sys.argv[1])
with zipfile.ZipFile(out/'shell.apk','a') as z:
    for p in sorted((out/'dex').glob('*.dex')): z.write(p,p.name)
PY
"$bt/zipalign" -f -P 16 4 "$out/shell.apk" "$out/aligned.apk"
if [ ! -f "$out/debug.keystore" ]; then
  keytool -genkeypair -keystore "$out/debug.keystore" -storepass android -keypass android -alias androiddebugkey -dname 'CN=Android Debug,O=Android,C=US' -keyalg RSA -keysize 2048 -validity 10000 -noprompt
fi
"$bt/apksigner" sign --ks "$out/debug.keystore" --ks-pass pass:android --key-pass pass:android --out "$out/residentevil-debug.apk" "$out/aligned.apk"
"$bt/apksigner" verify --verbose "$out/residentevil-debug.apk"
echo "Debug APK: $out/residentevil-debug.apk"
