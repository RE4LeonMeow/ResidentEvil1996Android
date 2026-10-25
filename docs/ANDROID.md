# Android launcher (experimental)

Target requested: Realme 10, Android 14. Minimum supported Android API: 29
(Android 10), with an up-to-date Android System WebView and WebGL 1 or 2.

This build runs the shared C++ engine as **wasm32 inside Android WebView**.
It is not a native ARM64 rewrite. WebView runs on the phone's supported CPU
architecture; no 32-bit Android runtime or x86 emulator is needed. wasm32
preserves the game's packed 32-bit pointer layouts and original resource
formats. Rendering uses OpenGL ES 3 (WebGL 2) or OpenGL ES 2 compatibility mode
(WebGL 1), audio uses SDL/WebAudio, and the
original AVI movies are decoded by a small FFmpeg build.

## Optional HD pack

The launcher **Install HD pack** scans local PAK, TIM and embedded image
containers in a worker, runs the bundled **FSRCNN x4** neural model, and writes
losslessly compressed replacement RGBA records to an immutable OPFS archive.
Backgrounds target **1440×1080**; texture sheets scale proportionally, bounded
to 2048 per side and 4 million pixels. Logical sizes, UVs and alpha coverage
stay original. The model reconstructs luminance; alpha-safe Lanczos colour and
final-size resampling preserve palette colour and cutouts. Model inference is
tiled with the full receptive-field overlap to prevent seams and bound memory.
This compact model improves edges; it is not a full texture remaster.
**HD sharpness** offers Natural (previous output), Crisp (default) and Sharp.
Crisp/Sharp add luminance sharpening after neural reconstruction, with a noise
threshold, local extrema limits and unchanged alpha. Model weights are not
retrained. Reinstall to change an existing pack; no sharpening runs in-game.

The installer inventories/deduplicates images first, then decodes files again
as needed instead of retaining all RGBA in RAM. The visible clock shows elapsed
time during scanning and an approximate remaining time after sufficient
upscaling samples. Its workload weights use actual image sizes and neural tile
overlap rather than file counts. The ETA adjusts to observed throughput; phone
heat, storage stalls and background throttling can change it. Completed build
duration is stored with the pack.

`tools/hd/import_fsrcnn.py` reproduces `fsrcnn_weights.h` from a pinned,
SHA-256-checked upstream pretrained graph. The Apache-2.0 license is bundled at
`assets/licenses/FSRCNN.txt`. The model is compiled into a small worker-only
SIMD WebAssembly module; no external service, downloaded image pack, model
runtime or Android Internet permission is required. Android fetches the worker
code and wasm in the parent WebView, then starts a Blob worker. No worker DNS
interception is needed. Modern WebAssembly SIMD, OPFS and Web Locks are needed
for generation. Installation may take a long time and several GB; progress,
cancellation, transactional reinstall, enable/disable and removal are exposed.

After installation **Reinstall HD pack** and **Remove HD pack** appear outside
the collapsible details. Old version-1 720p packs remain removable, but require
reinstallation for version-2 neural replacements. The private originals are
never moved or overwritten. Their fixed-size TIM/palette/UV layouts remain the
game's authoritative input; moving those files would not remove GPU upload
costs and overwriting them with larger images would break those layouts.

Activation uses an IndexedDB transaction after all writes finish. Web Locks
protect live packs; cancellation/failure retains the previous complete pack.
Pixel-content keys bind replacements to the actual original RGBA data. Headers,
dimensions, byte limits and DEFLATE checksums are validated. At runtime, the
worker performs all image reads/decompression and prefetches adjacent cameras
in a room. The main thread only uploads ready pixels, at most one queued
texture per frame; a camera can briefly show the original before HD arrives.
Immutable textures share resident GPU storage with reference counting and a
96 MiB soft budget (live textures may exceed it); decoded worker replies are
bounded to 32 MiB. Unmatched runtime-composed atlases retain original pixels.
No runtime resizing, neural inference, synchronous HD reads or per-frame
upscaling pass occurs. Streaming FMV bypasses the replacement cache.

The HD render target is 1440×1080, versus the normal 640×480. More GPU/memory
use is expected; device FPS and heat require real-device testing. Disabling HD
does not rebuild or touch the source tree. Clearing app data removes the pack.
Game originals, generated images and private test captures must never enter
GitHub, APKs, browser/source ZIPs or public test fixtures.

## Enemy-scene rendering

The web backend batches adjacent draws with identical render state across up to
four textures, using a per-vertex page selector and complete fallback samplers
in GLES 2/3. Primitive order, alpha blending, depth rules and texture sizes stay
unchanged. Interpolation captures keep original texture identities and use the
same batching during replay. An orphaned 512 KiB vertex stream replaces
per-draw VBO allocations and advances through non-overlapping regions, wrapping
only by orphaning storage. Texture mutation/readback/capacity/state changes
still flush pending work. This targets high draw counts in enemy/corpse views,
with HD on or off; actual Realme 10 FPS still requires device validation.

## Update 1.00: experimental cheats, branding and browser package

This 1.00 test-build revision adds per-button **Hold / Toggle** preferences and
an optional **Experimental · More FPS** geometry interpolation mode. Toggles
release on settings/menu/lifecycle changes, and only preferences are persisted.
Keyboard/gamepad behavior remains held. The More FPS option leaves the shared
simulation limiter unchanged, matches stable model/primitive identities and
replays an extra midpoint only when the scene, textures and available time
permit it. It uses original frames for doors, movies, menus, camera changes,
large geometry changes and slow frames. It starts off; resolution, textures and
filtering are preserved. The render capture buffers are bounded below 8 MiB.
Slow frames suspend interpolation attempts for about two seconds.
Additional GPU work can increase heat/latency. The FPS overlay includes generated
render frames; diagnostics list their count separately.


Install this update over the previous APK. It uses version code 15 and the same
debug signing key; keep the app installed to preserve saves and settings.

- The launcher credit now reads **Made by 4wu4_**. The APK and browser
  package are version **1.00**, retaining 0.9.1 rendering, image quality and icon.
- A separate static browser ZIP supports local folder import, optional browser
  retention of game files, keyboard shortcuts on PC, touch layouts on phones,
  browser gamepads, fullscreen, diagnostics and the existing save system.
  The separate browser repository describes its use and limits. The ZIP is downloadable;
  packaging does not deploy a website or copy selected game data into it.
- **Mod support is in the works.** A general mod loader is not part of 1.00.
- **Controls & settings → Cheats** first shows an experimental-risk agreement.
  After accepting, enable infinite health/ammo, heal or refill carried weapons,
  open the item box, or open the normal save-slot screen without ink ribbons.
  Actions require normal player control; movies, scripted scenes, menus, room
  transitions and death block them. Save/item-box requests are checked again on
  resume and processed by the game task. All toggles start off and last for the
  current runtime only. Disabling cheats does not undo changes already saved.
  Infinite health maintains full HP; scripted instant deaths can still happen.
  Use a separate save slot. Cheat toggles and agreement are not stored in saves;
  changed health, ammo or inventory are saved normally.
- Browser remembered files are copied into browser-owned Blobs so moving the
  original selected folder does not invalidate the local remembered library.

## Features introduced in 0.9.1: rendering overhead, scheduling and APK icon

- The Android renderer combines adjacent compatible draws, preserving their
  original order, texture, sampler, blend and depth state. The vertex queue is
  bounded to 12,288 vertices and flushed before texture mutation/deletion,
  clear, resize, capture and presentation.
- The backend retains shader, vertex-layout, texture-filter and render-state
  bindings until they change. Item-view framing is read once per rendered frame
  rather than once per 3D draw.
- The WebAssembly loop awaits a display callback or the next tick deadline
  during foreground play. The original limiter decides when ticks are due,
  without catch-up simulation. Paused sessions wait 100 ms between checks.
  See build 25 below for the foreground scheduling change.
- **Image quality is preserved:** existing render resolution (640×480 by
  default), textures, filtering, depth/transparency and widescreen behavior.
  No upscaler or reduced-quality mode was added.
- The APK uses the supplied zombie ICO artwork, decoding its largest 256×256
  frame without resampling. An adaptive icon supplies a black background and
  the Android launcher's mask. The artwork is separately credited in NOTICE.

Nine asset-free render captures are byte-for-byte identical to 0.1.7 in both
GLES 2 and GLES 3, including transparency order, texture mutation, filtering,
large batches and depth. Private USA-data samples reduced about 133 gameplay
draws per frame to 88, and about 114 pickup-screen draws to 9. Steady-state
vertex-layout/filter setup calls were eliminated. These are desktop Chromium
call counts, not measured phone FPS or temperature improvements. Version 0.1.6
was also sampled and had about 134 gameplay draws per frame.

## Features introduced in 0.1.7: widescreen interactions and Android documentation

Build 17 of 1.00 replaces the pickup-model crop below with a complete,
rearranged widescreen inventory/pickup layout. The preview frame expands,
the inventory column moves outward, and the item model stays centered.
Text, icons and models retain their proportions; decorative frame strips
extend into the added space. The drawable widens at the existing vertical
resolution (853×480, or 1920×1080 with HD enabled). Original 4:3 output,
maps/documents, save screens, options and story scenes keep their full layouts.
Item boxes use a separate wide arrangement, and their lid animations hold the
preceding world crop.
The following bullets describe the earlier 0.1.7 behavior.

- In optional 16:9 mode, door animations use a centered crop rather than
  following the player position from the destination room. Door approaches
  retain gameplay framing, and menu/room fades hold the preceding framing.
- World messages and item-model pickup prompts use a bottom-aligned crop so
  their text, Yes/No choices and cursor remain visible. With menu arrows enabled,
  prompts use arrows even though their picture remains wide. Doors retain the
  gameplay movement layout.
- The Android GL adapter uniformly reduces and lowers only the pickup viewer's
  3D model to fit its remaining display area. This keeps the model visible above
  the text; both opaque and translucent passes receive the same transform.
  Original 4:3 rendering and the shared engine's item camera are preserved.
- Full inventory and item-box content, maps, file/key lists, options, movies and scripted
  story scenes retain the complete 4:3 picture. The policy reads existing engine
  state and does not change scripts, camera matrices or packed game structures.
- The main [README](../README.md) now explains the Android adaptation, upstream
  credit, installation, controls, limits and substantial OpenAI Codex assistance.
  Inherited desktop instructions are preserved in
  [UPSTREAM_README.md](UPSTREAM_README.md).

## Features introduced in 0.1.6: grab framing and touch layouts

- Widescreen stays active during enemy grabs and bite reactions. These use
  animation windows also used by scripts; the presentation policy now checks
  their combat flag so the aspect ratio stays steady. Movies, dialogue, menus
  and scripted camera locks still preserve the complete 4:3 picture in 0.1.6.
  Version 0.1.7 adds the interaction exceptions described above.
- **Use arrows in menus** is enabled by default. With **Movement → Joystick**,
  gameplay uses the joystick and title, character selection, inventory and
  options screens use arrows. Turn the option off to keep the joystick in
  menus. Control style is independent of aspect ratio, so a bite or cinematic
  does not count as a menu. Switching style clears held touch input.
- **Edit touch layout** now supports individual sizes. Tap a control or pick it
  from **Control**, drag to move it, then adjust **Size** (50–200%). Selecting an
  arrow or the joystick previews that movement layout without changing the
  gameplay preference. **Save layout** remembers all sizes and positions.
  Android Back also finishes editing and returns to settings.
- **Overall touch size** scales the complete layout; individual sizes multiply
  that setting. Existing layouts retain their sizes until edited. **Reset
  controls** clears individual sizes/positions and restores menu arrows.

## Framing changes introduced in 0.1.5

The mansion introduction could alternate between centered widescreen, full 4:3
for subtitles, and a player-following crop during scripted idle poses. Movies
and scripted scenes now retain the complete, centered 4:3 picture throughout.
16:9 player tracking resumes when directional control is enabled and the
scripted camera is unlocked. In 0.1.5, menus and message boxes also kept their
complete picture; 0.1.7 adds wide world prompts and item-model views.
Ordinary aiming and combat damage keep gameplay framing.

The policy now checks the same directional-input flag that `game_loop` uses,
plus camera locks and scripted animation states, rather than treating actor
updates as proof of player control. This change only reads engine state; it
does not alter game scripts, camera matrices, or packed structures.

## Features introduced in 0.1.4: widescreen and settings layout

- **Controls & settings → Aspect ratio** selects original 4:3 (default) or
  optional 16:9. Widescreen uses the pan-and-scan and torso projection approach
  adapted from FusionFix v1.1.0. It crops the completed frame vertically and
  smoothly follows the player; camera changes snap to the new framing. It
  keeps one uniform transform for backgrounds, masks and characters. Menus,
  inventory and message boxes retain the full 4:3 image. Version 0.1.5 also
  preserves full movies and scripted scenes. This does not reveal extra scenery
  or stretch the picture. See 0.1.7 above for wide doors and interaction prompts.
- **Launcher** and **Logs** are inside settings. The opaque native bottom bar
  and its reserved screen space have been removed. **Launcher** uses the normal
  save/flush/exit path. **Logs** opens native diagnostics; Android Back has a
  native recovery fallback if JavaScript cannot open settings.
- Loading filenames appear as small text with a transparent background at the
  bottom, disappearing after 1.5 seconds without another file read. **Show
  loading filenames** toggles this text and remembers the choice across app
  restarts. Logs remain available when the overlay is hidden.
- TURN checks active gameplay/menu tasks rather than historical menu entries
  and request flags. Stale load/menu/fade state and non-damage flag bits no
  longer keep it disabled after returning to normal movement. The real movie,
  transition, action, message, damage and death guards remain. The regression
  harness verifies recovery after a menu; the intermittent phone issue still
  needs device confirmation.
- The launcher uses a composition of the user-supplied logo and background.
  See the bundled notices for artwork and FusionFix attribution. The uploaded
  GOG gameplay files remain private test data and are never bundled.

## Controls introduced in 0.1.3

- **Controls & settings → Movement** switches between D-pad arrows and an
  eight-direction joystick. Both use the original tank controls. The joystick
  has a radial dead zone, supports simultaneous action touches, and can be
  repositioned with **Edit touch layout**. Size/opacity/hide settings apply
  to both styles, and the choice is remembered.
- The draggable **TURN** button performs an instant 180° turn during normal
  player movement. It is ignored in menus, movies, scripted actions, damage,
  death, aiming and transitions. This is an Android port feature; it preserves
  the existing game state machine and packed structures.
- **Show FPS counter** toggles a small overlay counting frames actually
  presented by the game renderer. It updates twice per second and displays
  `paused` in settings. It does not count browser animation callbacks or infer
  frame rate from the display refresh rate.
- Touch buttons act on pointer-down; short taps between engine input polls
  are retained until one poll consumes them. Pause/cancel clears input. This
  addresses missed quick menu presses while retaining the game's tick timing.
  The web loop also avoids a redundant timer wait, controller updates reuse
  their objects, and closed diagnostic panels defer log rendering.
- Browser tap highlights, text selection and context menus are suppressed on
  the touch controls. Holding a button still shows the intentional held state.

The user reports successful gameplay on Realme 10 / Android 14 and an
intermittent TURN failure before 0.1.4. The user subsequently confirmed that
0.1.4 widescreen gameplay works well, but reported unstable intro/cutscene
framing. Version 0.1.5 addresses that presentation issue and needs phone testing.
No measured phone performance gain is claimed.

## Android file reads and launcher recovery (introduced in 0.1.2)

- Game bytes now use a direct Android storage bridge with bounded 256 KiB
  chunks and exact-length checks. This bypasses the synchronous WebView XHR
  path that failed reading the final chunk of `Movie/capcom.avi` on the phone.
  Only the current chunk is transferred; the existing 16 MiB cache remains.
- The shared Java reader handles partial reads, providers without `skip`, EOF
  and provider exceptions. The HTTP fallback also buffers each chunk before
  returning it, so late stream failures cannot escape its error handler.
- Storage failures become normal filesystem I/O errors with the file name,
  offset and length in diagnostics. They no longer throw a DOM NetworkError
  through the WebAssembly engine. A later engine exit retains that message.
- **Back to launcher** replaces the native Activity/WebView. The WebView also
  permits navigation back to its own launcher. **Launcher** in settings uses
  the normal engine save/flush/exit path before recreating the view. If the
  page is unresponsive, Android Back or the native watchdog opens recovery
  diagnostics with **Reload launcher**. Emergency reload ends the current
  session and retains completed saves, folder access and settings.

Local Java and Chromium regression tests passed, including the exact reported
movie offset (1,310,720), final chunk length (224,732) and file size (1,535,452),
using generated bytes. The native document provider and recovery still need
verification on the Realme. No GOG gameplay data is distributed.

## Renderer options and diagnostics (introduced in 0.1.1)

- **Controls & settings → Renderer** selects Auto, OpenGL ES 3, or real OpenGL
  ES 2 compatibility rendering. Changing it takes effect on the next launch;
  use **Launcher** in settings. Vulkan is not available: WebView/SDL exposes
  WebGL/OpenGL ES to this engine. Selecting WebView's internal Vulkan driver
  from this application is not supported, and there is no native Vulkan backend.
- **Test graphics & sound** runs without game files. It uses the actual engine
  to upload/draw/read back a four-color texture, displays it for a few seconds,
  and plays a short 440 Hz test tone. A successful pixel readback proves that
  pipeline works; it does not prove that a full game or the phone's speakers work.
- **Settings → Logs** opens native startup diagnostics. If the renderer is
  unresponsive, use Android Back or the native watchdog recovery dialog.
  Use **Copy report** if startup fails.
  The report records device/WebView versions, engine phases, file reads, frame
  progress and errors. It stays on-device until you choose to copy/share it.
- Lifecycle and settings own game pause state. Stale SDL/browser blur events
  no longer keep the game paused after launch/resume. SDL now uses the audio
  context unlocked by the Launch tap; the previous pause code referred to the
  wrong SDL audio-context object.
- Boot reads check TIM headers, missing/truncated files and the sizes of known
  fixed boot buffers. Android file-provider reads use explicit offsets and
  lengths. Failures appear as specific messages.
- **Skip movies (troubleshooting)** bypasses all movies temporarily to isolate
  movie decoding/loading stalls. Turn it off to restore normal movies.

For a black screen on the Realme 10, first try the graphics/sound test with
OpenGL ES 2 selected. Then launch with your GOG files. If it still fails, copy
the native Logs report instead of waiting on a blank screen. Version 0.1.0 was
reported to black-screen after Launch; 0.1.1 exposed the movie-file read error.
The user subsequently reported successful gameplay with version 0.1.2.

## Install and use

1. Install `residentevil-debug.apk` on the phone. Android may ask you to allow
   installation from the app opening the APK. It is a development/debug build,
   not a Play Store release.
2. Copy the **installed/extracted USA GOG PC game folder** to local phone
   storage. Keep the complete Data, Stage1…StageN, Sound, Voice and Movie trees.
   A GOG installer `.exe`, disc image, or source ZIP is not an installed game
   folder. The PC executable is not run on Android.
3. Open **Resident Evil Launcher → Select game folder**. Select the game
   installation or its `USA` folder through Android's folder picker. The
   launcher checks the common boot resources and that room data is present;
   this is not an exhaustive asset-integrity check.
4. Tap **Launch game**. The original PC game logic handles menus and gameplay.
   Folder access is remembered. Nothing is uploaded; the app has no Internet
   permission. Choose a local storage provider for reliable random file reads.
5. Use the gear button or Android Back for launcher settings. Adjust button
   size/opacity, hide controls, change controller bindings, or choose **Edit
   touch layout**, drag or resize each control, then **Save layout**. In-game Options
   control the original game's settings and action mappings.

Default touch controls: D-pad for tank movement, ACTION for interact/confirm,
RUN for run/cancel, AIM for ready weapon, ITEMS for inventory and PAUSE for
the in-game options shortcut. TURN performs the port's quick 180° turn while
movement is enabled. Hold AIM + ACTION to fire. Multiple fingers
are supported; releasing one finger does not release another's action.
The game itself can remap these actions, so custom in-game bindings also change
what these default touch buttons do.

Bluetooth/USB controllers are paired in Android settings. The Android bridge
handles face buttons, shoulders, triggers, Start/Select, stick clicks, D-pad
and left stick. The launcher offers remapping and a stick dead zone. One
active controller is supported. Browser standard-layout gamepads provide a
fallback. Custom/unmapped HID devices may need Android-side configuration.

Saves and `config.ini` are stored in this app's WebView IndexedDB storage,
separate from read-only GOG data. Files flush after closing a save/config and
when pausing; normal game exit waits for pending writes. Clearing app data or
uninstalling deletes these saves. Use **Save backups** in the launcher to
export a portable backup first, or import one with matching-slot confirmation.
Android lifecycle events release
held input and pause the game and WebAudio.

## Build

Requirements: Java JDK 17 or newer; Android SDK platform 35 and build tools
35.0.0; Python 3; CMake, Ninja, Make; Emscripten 3.1.69; clean FFmpeg n7.1.1
source (commit `db69d06eeeab4f46da15030a80d539efb4503ca8`). No game data is
needed to build. Use the supplied source archives or the official upstreams.
Do not disable TLS, package signatures or checksums when installing tools.

```sh
# Activate your Emscripten installation first (for example source emsdk_env.sh).
export RE1_FFMPEG_SOURCE=/absolute/path/to/clean/FFmpeg
export ANDROID_HOME=/absolute/path/to/android-sdk
bash tools/android/build-web.sh
bash tools/android/package.sh
```

The first helper builds static wasm FFmpeg and the game, and places generated
`residentevil.js` and `residentevil.wasm` in the Android assets directory.
The second uses official Android `aapt2`, `d8`, `zipalign` and `apksigner`, so
Gradle/Maven is not required. It creates `build/android/residentevil-debug.apk`
and verifies its signature. Its debug key is generated in `build/android/`.
Keep that key for updates to an installed debug APK. Do not use it for a
production release. Gradle project files are also provided but the Gradle
build path has not been tested here; run the web build first.

In the prepared cloud instance, tools are retained outside the checkout:

```sh
. /workspace/toolchains/activate.sh
export RE1_FFMPEG_SOURCE=/workspace/toolchains/ffmpeg
bash tools/android/build-web.sh
bash tools/android/package.sh
```

The wasm FFmpeg libraries are cached in `build/ffmpeg-wasm`. Remove the cache
and rebuild when changing FFmpeg source or codec configuration; use a clean
source tree for an out-of-tree FFmpeg build. Web builds use four jobs by
default (`RE1_BUILD_JOBS` overrides this). Linux and Windows retain their
existing build paths. The one shared source spelling change replaces
`unsigned __int8` with its equivalent `unsigned char` for Clang compatibility.

## Validation and limits

Executed in the cloud environment:

- Complete wasm engine compilation/link, retaining packed-layout assertions.
- Twelve input tests: simultaneous touches, independent releases, opposite
  directions, controller dead zone/buttons/triggers/remapping, pause/disconnect,
  short-tap delivery, buffered-tap clearing, joystick directions/dead zone,
  automatic menu movement preferences, and persisted/bounded individual sizes.
- Actual engine scheduler in wasm: stack locals, sleep, suspend/resume,
  chaining, exit, asynchronous callbacks, restart/reset.
- Actual wasm FFmpeg decoded a generated AVI: 5 Cinepak pictures and 11,025
  PCM audio samples. The fixture contains no commercial content.
- Chromium with the application's CSP: settings/layout persistence, runtime
  initialization, binary reads spanning range-cache boundaries, seek/EOF,
  save flush and restore across reload, GLSL ES compilation/link, and the actual engine SDL/GL/audio
  initialization, first game ticks with valid generated TIM/item fixtures after
  a simulated SDL blur, and a 640×480 backbuffer capture. Both GLES 3 and GLES 2
  paths draw and verify all four test colors; WebAudio is running, pause/resume
  works, and invalid/oversized boot files produce visible errors.
- The APK's actual Java file reader: all bytes across the reported movie tail,
  partial/zero reads, non-seekable providers, truncation, provider exceptions
  and invalid ranges. Chromium's native bridge fixture: exact movie bytes/EOF
  without any `/data` XHR, failed/short replies converted to filesystem EIO,
  file-specific errors, native recovery dispatch after failure, normal save/
  exit/recovery dispatch, and navigation after an injected XHR NetworkError.
- Existing `tests/check_platform_boundary.py` passed.
- Actual wasm quick-turn service: half-turn/wrap, ordinary movement, and
  guards for menus, movies, pause, scripted actions, damage and death; recovery
  with stale menu/load/fade flags; inactive task entries; torso projection and
  UI/cinematic/gameplay presentation modes; scripted idle poses with camera
  locks, the mansion intro input mask, subtitle gaps, control restoration,
  combat grab windows with blocked directional input, independent menu state,
  door phases, world versus scripted messages, item-view versus full-menu modes,
  and transition framing hints.
- Six presentation tests: uniform 16:9 crop, complete 4:3 UI, aspect bounds,
  player tracking/camera cuts/dead band, bounded smoothing time, and door/prompt
  crops retaining the source rows used by text, choices and cursors.
- Browser controls: persisted aspect/loading/joystick/FPS settings, native Logs
  and loading-toggle dispatch, individual button/joystick resize and persistence,
  both movement layouts in the editor, menu style changes clearing held input,
  16:9 crop and full UI/cinematic geometry, stable
  cutscene framing across actor/camera changes and pan reset on control return,
  centered door geometry, bottom-aligned prompts and retained transition framing,
  draggable joystick, actual multi-touch pointer input and independent finger releases, switching
  back to arrows, transparent tap highlight, live rendered-frame FPS overlay,
  pause/stop display and quick-turn button dispatch.
- Private local tests with user-supplied USA data: original `capcom.avi`
  (including its formerly failing final chunk) and `ou.avi` play through the
  wasm decoder with audio enabled; title screen renders in GLES 2 and GLES 3;
  title input and normal save/exit/return to launcher work. The 0.1.4 GLES 2
  check also enables 16:9 and confirms return to the launcher. Version 0.1.5
  replays Chris's mansion intro in GLES 2 and Jill's in GLES 3 (movie skipping
  enabled), checks full-frame geometry across scripted camera/dialogue changes,
  then verifies 16:9 control, inventory, TURN and normal launcher return.
  Version 0.1.6 also replays the title and inventory switches with automatic
  arrows enabled, checking that gameplay restores the joystick and that the
  intro remains complete. Enemy-grab framing has a wasm state regression;
  the user subsequently reported good grab framing on the phone.
  Version 0.1.7 checks Chris's intro and its actual door animation, plus a
  world-message handler and a pickup handler invoked in local test memory.
  Private screenshots verify the wide room text and the entire key model above
  its pickup text. This samples one item and door, not all rooms or item models.
  Version 0.9.1 repeats the intro, door, world prompt and pickup checks with the
  optimized renderer, including No/confirm, joystick/TURN recovery and normal
  return to the launcher. Its frame-wait arithmetic test and both GLES pixel
  regressions pass, alongside the launcher/browser suite.
  These tests ran in desktop Chromium with local storage fixtures, not Android's
  document provider.
  Game files and captures remain outside the repository and all downloads.
- Java compilation, DEX generation, APK packaging/alignment and v3 signature
  verification passed.

**Still requires device validation:** version 0.9.1's frame pacing, sustained
performance and temperature on the Realme 10; door and interaction framing
across rooms and item types; TURN
recovery, loading overlay and native recovery fallback; audible phone output,
real Bluetooth/USB controllers, performance, room changes,
in-game save/reload and an end-to-end playthrough. Successful phone gameplay
in the earlier builds is user-reported; there is no attached Android device
in this environment. Local tests do not establish phone responsiveness or full
playthrough correctness. Report the visible error and last action if anything
fails.

Input and presentation tests: `node --test tests/android/input.test.js tests/android/presentation.test.js`.
Java file-reader test (JDK 17+):

```sh
mkdir -p build/filechunks-test
javac -d build/filechunks-test android/app/src/main/java/org/re1/launcher/FileChunks.java tests/android/FileChunksTest.java
java -cp build/filechunks-test org.re1.launcher.FileChunksTest
```

Browser test: `python tests/android/browser.py` (Python Playwright and system
Chromium; generated web runtime must exist). The browser test uses synthetic
storage and generated textures; it does not use or validate commercial game data.
See `tests/android/scheduler.cpp` and `media.cpp` for additional wasm harnesses.
Quick-turn service test:

```sh
em++ -O2 -sASYNCIFY=1 -sENVIRONMENT=node -sEXIT_RUNTIME=1 -I src tests/android/quick_turn.cpp src/platform/web/services.cpp -o build/quick-turn-test.js
node build/quick-turn-test.js
```

Browser frame scheduling and asset-free renderer regressions:

```sh
em++ -O2 -sASYNCIFY=1 -sENVIRONMENT=node -sEXIT_RUNTIME=1 -I src tests/android/frame_pacing.cpp -o build/frame-pacing-test.js
node build/frame-pacing-test.js
python3 tests/android/frame_pacing.py
em++ -O2 -sUSE_SDL=2 -sUSE_ZLIB=1 -sMIN_WEBGL_VERSION=1 -sMAX_WEBGL_VERSION=2 -sDYNAMIC_EXECUTION=0 -sINVOKE_RUN=0 -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1 '-sEXPORTED_RUNTIME_METHODS=["FS","callMain"]' -I src tests/android/render_batch.cpp src/marni/MarniDX_GL.cpp -o build/render-batch.js
python3 tests/android/render_batch.py
```

The renderer test reads back nine complete RGBA images per backend, checks
known colors and exercises ordered blending, texture update/delete boundaries,
sampler transitions, queue capacity, depth and coplanar alpha. The release check
also compared all pixels with the same harness built against the 0.1.7 renderer.

Android/Web code is included in the project's GPL-3.0 source distribution;
the adapted FusionFix presentation code is GPL-3.0-or-later with attribution.
Runtime license notices are in `android/app/src/main/assets/licenses`.
`launcher-art.png` is a composition of separately supplied launcher artwork,
and `ic_launcher_art.png` is the separately supplied APK icon. Their rights
remain with the respective owners; the code license does not
relicense that artwork. GOG gameplay data is never included in the APK or
source archives.

HD checks (synthetic images only):

```sh
node --test tests/android/hd.test.js
c++ -std=c++17 -O2 -I src tests/android/hd_image.cpp -o build/hd-image-test
build/hd-image-test
c++ -std=c++17 -O3 -ffp-contract=off -I src tests/android/hd_neural.cpp -o build/hd-neural-test
build/hd-neural-test
python3 tools/web/package.py
python3 tests/android/hd_browser.py
```

Actual asynchronous HD upload pixels, resident reuse, invalid-header recovery, original logical
texture dimensions and streaming movie updates are also checked in both GLES
versions with this asset-free harness:

```sh
em++ -O2 -sUSE_SDL=2 -sUSE_ZLIB=1 -sMIN_WEBGL_VERSION=1 -sMAX_WEBGL_VERSION=2 -sDYNAMIC_EXECUTION=0 -sINVOKE_RUN=0 -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1 '-sEXPORTED_RUNTIME_METHODS=["FS","callMain"]' -I src tests/android/hd_render.cpp src/marni/MarniDX_GL.cpp -o build/hd-render.js
python3 tests/android/render_batch.py --stem hd-render --hd
```

The interpolated renderer uses Asyncify for its midpoint presentation wait:

```sh
em++ -O2 -sASYNCIFY=1 -sUSE_SDL=2 -sUSE_ZLIB=1 -sMIN_WEBGL_VERSION=1 -sMAX_WEBGL_VERSION=2 -sDYNAMIC_EXECUTION=0 -sINVOKE_RUN=0 -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1 '-sEXPORTED_RUNTIME_METHODS=["FS","callMain"]' -I src tests/android/interpolated_render.cpp src/marni/MarniDX_GL.cpp -o build/interpolated-render.js
python3 tests/android/render_batch.py --stem interpolated-render --interpolation
```

Build 16 uploads each collected span of geometry before drawing it, rather
than updating the active vertex buffer between actor/blood draws. Submission
order, blending, depth, textures and resolution are preserved. The pixel
harness also checks that 80 alternating render-state draws need one vertex
upload and no buffer sub-updates. Texture changes, readback and resize flush
pending draws before modifying their resources.
More FPS also backs off for approximately five seconds when the full frame
interval reveals a delayed GPU/compositor stall after submission.

The browser package embeds the matching engine wasm in its generated script
and versions launcher asset URLs. This supports opening extracted `index.html`
directly, avoiding the failed wasm fetch on `file://`. HD packs still need
localhost/HTTPS storage permissions. To verify direct opening with synthetic
files and durable saves, install Playwright's Chromium and run:

```sh
python3 -m playwright install chromium
python3 tests/web/direct_file.py
```

Build 17 also isolates browser keyboard shortcuts from SDL's keyboard state.
Holding Ctrl previously bypassed the JavaScript keydown handler, while the
keyup handler intercepted arrow releases. SDL could retain a turn key until
focus changed. Both Ctrl keys now act as run/cancel through the same buffered
input as V; Ctrl+arrows never enter a second game-input path. Android/native
keyboard handling is preserved. These asset-free checks exercise the actual
platform backend and wide-menu renderer:

```sh
em++ -O2 -sUSE_SDL=2 -sENVIRONMENT=node -sWASM_ASYNC_COMPILATION=0 -I src tests/android/keyboard_bridge.cpp src/platform/linux/platform.cpp -o build/keyboard-bridge.js
node build/keyboard-bridge.js
em++ -O2 -sUSE_SDL=2 -sUSE_ZLIB=1 -sMIN_WEBGL_VERSION=1 -sMAX_WEBGL_VERSION=2 -sDYNAMIC_EXECUTION=0 -sINVOKE_RUN=0 -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1 '-sEXPORTED_RUNTIME_METHODS=["FS","callMain"]' -I src tests/android/menu_layout.cpp src/marni/MarniDX_GL.cpp -o build/menu-layout.js
python3 tests/android/render_batch.py --stem menu-layout --menu
```

Build 18 keeps version 1.00. Inventory Equip/Check/Combine sprites, their
opening/closing animation and the selection highlight follow the right side
of the widened preview. Black masks follow the frame instead of obscuring
the relocated actions. Broad lower-panel fills extend with their frame;
glyphs, icons and models retain their proportions.

Rendering now avoids an intermediate copy of already-formatted 3D vertices
and reflows menu sprites directly into the batch. Model reflow no longer
calculates sprite bounds. HD slots retain a direct reference to their shared
resident texture/filter, and HD uploads reuse a bounded scratch buffer across
camera changes. The 1x1 white filler texture no longer receives irrelevant
filter changes when alternating textured and solid passes. Draw order,
sampling and pixels are preserved.
The worker decodes the requested camera only once when also prefetching its
room, and inflates directly into the transferable header-plus-RGBA buffer.
Existing HD packs remain compatible; no rebuild or resolution reduction is
required. Local GLES checks cover identical original-mode pixels, shared HD
textures, complete samplers after deletion, lower-panel coverage and action
placement. Phone performance still needs device testing.

Build 19 (still version 1.00) holds the current camera crop during world
messages such as automatic key/lockpick use. Only the game's message-font
glyphs, shadows and Yes/No choices move into the visible part of the wide
view; backgrounds, scene masks, models, text proportions and resolution are
unchanged. The held framing survives an inventory visit and resets when the
room camera changes. Original 4:3 and cinematic text retain their layout.

Normal player-state stair/ladder interactions remain wide while movement or
camera switching is temporarily locked. Scripted state-8 animations retain
the complete cinematic image. These traversal motions use the original
rendered frames rather than More FPS interpolation. The service harness
checks both traversal states and the cinematic/paused/death guards.
The browser item-box cheat was also checked in real gameplay through F2,
Cheats and keyboard cancel, including return to movement; it worked in the
current code without a separate browser implementation. Device testing is
still needed for the particular staircase reported by the player.

Build 20 retains version 1.00 and targets combat rendering cost. Unused
texture units keep their complete texture bindings across character/effect
passes, instead of repeatedly switching to white and back. Texture deletion
and HD resident eviction invalidate these bindings before reuse. GLES3 uses
a flat integer texture-page selector, since the page is constant across each
triangle. Alpha-blended effects skip exactly-zero-alpha fragments only when
they cannot write depth; additive colors, replacement draws, depth-writing
faces and faint alpha remain intact. Transparency metadata is recorded on
texture creation/update, with no texture scanning during draw submission.

The 34 GLES2/3 pixel captures match build 19 byte for byte, including empty,
faint, additive and depth-writing transparency. The alternating texture-bank
fixture requires zero redundant texture binds after warmup. GLES3 occlusion
queries verify that invisible alpha-effect samples are discarded while the
same colored additive samples remain visible. Resolution, filtering, HD
packs, effect count and simulation speed are unchanged. Local graphics and
gameplay checks cannot establish FPS or thermal behavior on a Realme 10;
the reported 12 FPS shooting case still needs device retesting.

Build 21 retains version 1.00 and the full HD resolution. Single-texture
draws use a dedicated shader without texture-bank dispatch. Depth-writing
model passes use programs without fragment-discard instructions, preserving
their original alpha/depth behavior and allowing early depth rejection.
Sampler uniforms are assigned once; projection uniforms update on drawable
size changes. Per-program uniform caches survive shader switches.

A conservative alpha-coverage index identifies fully opaque sampled regions,
including a filtering margin. An entire existing batch may use full color
writes only if every contribution is opaque. This never splits or reorders
character draws. Coverage is updated from actual original/replacement upload
pixels; fades, holes, partial alpha, additive effects and depth-writing
transparent faces retain their behavior. The mixed-atlas regression checks
both exact pixels and unchanged draw count. The 34 saved GLES2/3 captures
remain byte-identical to build 20. These checks establish rendering behavior,
not guaranteed FPS on phone GPUs; the crowded upstairs fight needs device
retesting with the existing HD pack.

The item box uses its own complete 16:9 arrangement: the list, names, preview,
arrows and masks move together, while the inventory remains on the right.
Lid opening/settling and the return to play hold the preceding world crop.
Maps, documents, options, movies, scripted scenes and save screens keep their
complete original framing. Presentation queries do not change box contents,
input or saves. GLES checks cover box alignment and proportional text; the
service harness covers opening states, movie/save guards and interpolation.
Local original-data checks also verified the live lid animation, item-box
navigation/exchange and return to gameplay at both original and HD resolution.

The launcher now displays **Made by 4wu4_** and has a **Credits** button.
Edit `android/app/src/main/assets/web/plain.txt` and rebuild to update APK
credits. The browser ZIP includes its own editable `plain.txt`: HTTP/localhost
opens it in a dialog with fresh reads, and direct `file://` launches open the
current file in another tab. Credits are displayed as text.

The additional alpha-coverage check can be run without game data:

```sh
g++ -std=c++17 -O2 -I src tests/android/texture_coverage.cpp -o build/texture-coverage-test
build/texture-coverage-test
```


## Version 1.00, build 22: combat effects, map/file layout and browser credits

Build 22 replaces the remaining fragment-discard effect shaders from builds
20–21 with the simpler texture/color shaders used before that change. Both
single-texture and four-texture-bank programs retain their uniform caches.
This targets the reported mobile slowdown on bullet impacts and green zombie
effects; it does not change enemy AI, effect count or game timing. Existing
opaque-batch optimization and depth behavior remain intact.

Exactly invisible alpha sprites can now be rejected before submission, using
a conservative coverage index built when the actual texture is uploaded.
The index includes linear-filter and GLES2 precision margins and follows the
ready HD replacement. Colored alpha-zero additive draws, faint alpha and
depth-writing geometry retain their original behavior. No fragment-discard
instruction remains in either GLES2 or GLES3 shader. The 34 saved renderer
captures remain byte-identical to build 21; HD and interpolation checks also
cover live replacement uploads and alpha/additive behavior. Texture quality,
resolution, filtering, pack format and original files are unchanged. Existing
HD packs do not need rebuilding.

Map and file selectors now use the complete wide menu frame. Map zoom keeps
its markers and labels in proportion; document pages use one centered
coordinate system so the last text columns stay with the rest of each page.
Only decorative frame strips and flat backgrounds expand. Original 4:3 mode,
save screens, options, movies and scripted scenes remain available. Read-only
port presentation queries do not change menu logic or collected files.

Browser Credits now opens the same text dialog even when `index.html` is
opened directly. The packager escapes and embeds a snapshot of `plain.txt`
for that mode; HTTP/localhost and Android still read the file when opened.
Edit the canonical `android/app/src/main/assets/web/plain.txt` and rebuild to
change direct-file credits, or use `serve.py` for fresh reads of the extracted
file. No new tab or local-file fetch is required for direct-file credits.

Logs now includes the GPU, output dimensions, HD/More FPS settings and recent
frame-gap and tick-elapsed timing. Timing uses a fixed 64-sample buffer, with
no per-frame graphics queries. Tick elapsed includes render processing and
is not an isolated CPU or GPU measurement. Copy a report shortly after a
slowdown to retain recent samples.

Local graphics/gameplay checks do not establish Realme 10 GPU performance.
The reported upstairs bullet-hit/green-effect case still needs phone testing,
with the same HD pack and More FPS disabled for comparison. No supplied game
data, generated HD images or private test saves are included in downloads.


## Version 1.00, build 23: reduce background and combat shader work

The renderer now keeps opaque 2D backgrounds separate from transparent room
masks. A full-screen background uses one texture and full color writes rather
than joining a blended texture bank. This preserves draw order and avoids
adding boundaries within mask tiles or 3D actor batches. Backgrounds, masks and their pixels
are unchanged; opacity is still verified from the actual uploaded texture.

GLES2 and GLES3 now have cached programs for exactly one, two, three or four
samplers. Two-texture actor/weapon draws no longer run the four-sampler shader.
Shader precision, filtering, RGBA uploads, depth behavior and HD resolution
remain unchanged. The 34 saved pixel captures are byte-identical to build 22;
additional checks exercise every sampler-count variant. Existing HD packs
remain compatible and do not need rebuilding.

The Android Logs button now captures current JavaScript diagnostics before
opening the native log. GPU, output size, HD/More FPS settings and recent frame
and tick timings appear first in the copied report, with build 23 identified
in its header. Opening settings no longer triggers a false no-progress warning.

Private gameplay checks use a handgun-equipped save and verify that the aim
animation actually starts. Local software-GPU results cannot establish FPS on
the Realme 10. Retest aiming and bullet impacts on the phone; if a slowdown
remains, copy Logs immediately afterward and include its initial timing lines.
No game data or private test saves are included in the packages.


## Version 1.00, build 24: retained aiming diagnostics

Logs retains the slowest eight-real-frame aiming window separately for rooms
with and without living enemies. Each snapshot includes frame gap, tick
elapsed, room/camera, time and enemy count, and survives releasing AIM or
opening settings. Camera changes and pauses reset the sampling window, while
interpolated frames do not inflate the real-frame rate. Storage is fixed at
eight samples per rolling window; no per-frame graphics queries are added.
The stale movie phase label also returns to Game loop running during play.

The reported build-23 Mali-G57 MC2 log shows 35.3 ms frame gaps and 5.3 ms tick
elapsed after the fight, so it does not capture the reported 12 FPS interval.
The aiming slowdown remains under investigation; this build does not claim a
confirmed device performance fix. Reproduce it and copy the initial Worst
aiming with enemies lines. Source-game targeting code and graphics quality
are unchanged, and existing HD packs remain compatible.


## Version 1.00, build 25: foreground scheduling without an active touch

The reported slowdown disappears with Hold AIM, or by keeping a finger on an
empty part of the screen while Toggle AIM stays latched. The input model
delivers the same steady AIM/fire masks in both modes, and the latched button
does not repeatedly update the DOM. This distinguishes the issue from repeated
aiming presses or zombie targeting work.

The foreground WebAssembly loop now schedules requestAnimationFrame alongside
the existing tick-deadline timer. The first callback wakes the loop and cancels
the other; early display callbacks cannot run an early game tick. Keeping the
deadline avoids waiting an extra display refresh when it falls between frames.
The original limiter still decides when a tick is due; animation, combat,
movement, menus and scripts retain their game timing.
SDL automatic Asyncify yielding is disabled through its supported hint. It
otherwise adds emscripten_sleep(0) after every buffer swap, creating another
timer wait outside the game-loop scheduler. Browser traces identified that
wait, and delaying timers to 80 ms reproduced approximately 11 FPS before
disabling it. The main loop and optional interpolation still yield explicitly.
There is no catch-up loop, synthetic touch, busy wait or forced CPU/GPU power
mode. Paused/background loops retain a 100 ms timer. Toggle controls, texture
quality, HD packs and More FPS settings are unchanged.

The async scheduling harness verifies display callbacks without active touch,
cancellation of the losing callback, recovery from delayed timers, paused
waits/resuming and the original 33 ms budget at 60/90/120/144 Hz.
Retained aiming diagnostics remain available. The scheduling change needs
confirmation on the Realme 10: test Toggle AIM with fingers lifted in the
upstairs zombie room and compare its Worst aiming timing line. Local software
rendering cannot establish the phone's performance. Existing HD packs need no
rebuild; no supplied game data or private test saves are distributed.


## Version 1.00, build 26: FMV audio/video synchronization

The live-action intro could play sound several seconds ahead of the picture.
Private original-data playback reproduced a mixer cursor roughly four seconds
ahead of the movie clock. SDL's main-thread ScriptProcessor callbacks can be
delayed during loading/decoding and then consume movie PCM ahead of its video
timeline. The old video clock rejected that cursor and fell back to wall time.

Android/browser FMV now uses an AudioBufferSourceNode for the already decoded
S16 stereo soundtrack. It preserves its sample rate and samples, copies PCM
once, reuses the AudioBuffer for scenario-cut seeks and releases it on close.
The first start is scheduled after the copy completes. Video follows the same
AudioContext timeline, including suspension/resume, instead of comparing it to
wall time. A short silent audio tail keeps its clock running until video EOF.
Game sound effects and music retain their SDL mixer, and native Linux keeps its
existing movie backend. No movie conversion or replacement files are required.

Generated-PCM tests cover exact channels/samples, long preparation stalls,
segment seeks, EOF, pause/resume and cleanup. Real WebAudio checks model a
main-thread stall and verify its clock does not jump through samples. Run:

```sh
node --test tests/android/movie_audio.test.js
python3 tests/android/movie_audio.py
```

Version remains 1.00. Image quality, HD packs, movie frames and game timing are
unchanged. Existing HD packs need no rebuild. No original movies, game files,
private recordings or test saves are included in source or binary packages.


## Version 1.00, build 27: launcher credits, Donation and social links

Credits now lists Made by: 4wu4_ and playtesters Kali Kyle, Vhmed_hosny and
khoilinux. The editable plain.txt remains the source, including the embedded
direct-file browser snapshot. The bottom-left footer places X, Discord and GitHub
icons beneath the maintainer name, alongside Donation. X uses a white mark;
Discord retains its brand color against a dark button.

Donation shows the maintainer’s message with Cancel/OK before displaying the
three supplied crypto addresses. Reopening always starts with that message.
Cancel, Escape, Close and Android Back return to the launcher. Social links
open a separate browser tab on the web and an external handler on Android.
The native bridge accepts only the three configured HTTPS destinations; the
launcher WebView stays on its offline origin and retains its file selection.

The browser package omits HD controls, builder scripts, worker/model binaries,
model notices and the diagnostic HD field. It uses original images at 640×480
with the existing widescreen layouts. Android keeps its HD installer and
existing packs. The shared game engine, rendering, movie synchronization and
game timing are unchanged by this launcher update. Version remains 1.00.

## Version 1.00, build 28: portable save backups

The launcher now has **Save backups -> Export saves / Import saves** in both
editions. Android uses the system document picker; browsers download/select a
`.re1saves.json` file, including direct `index.html` launches. The format holds
only the eight original USA `savedat1.dat` through `savedat8.dat` slots, with
per-file CRC32 corruption checks. It excludes config, controls, game data and
HD packs. Save in-game and return to the launcher first; unsaved gameplay is
not captured.

Import validates the entire bounded backup before showing a matching-slot
overwrite confirmation. A single IndexedDB transaction commits the import;
failure rolls it back. Unmatched slots and configuration remain intact.
Save management is unavailable while this page runs the engine. Where Web
Locks are supported, the engine holds a shared-origin save-storage exclusion
lock until its page closes, preventing another cooperating game tab from
importing or exporting stale data. Existing saves use the original IDBFS
database and require no migration. Engine/WASM and image quality are unchanged.

## Version 1.00, build 29: signed save bytes and ZIP/GOG compatibility

The exporter now accepts the signed `Int8Array` buffers produced by the
compiled C/WASI `fd_write` path and persisted by IDBFS. Build 28 incorrectly
required `Uint8Array`, rejecting valid 2690-byte saves. Normalization uses a
byte view over the original buffer and offset, preserving every bit without
rewriting saved data. Both the 2048-byte compact and 2690-byte extended save
formats written by the engine are accepted.

The two launcher actions now import/export standard ZIPs containing original
USA GOG `savedat1.dat` through `savedat8.dat` files. ZIPs from either edition
transfer to the other. Files may be at the root or inside `SAVE/`, with
case-insensitive slot names. Stored and ordinary Deflate compression, ZIP
comments and streaming data descriptors are supported. Compressed imports
use bounded `DecompressionStream('deflate-raw')` reads. No ZIP library or
other dependency is downloaded. Exports use stored entries and work with
ordinary PC ZIP tools. To use them with GOG, extract the `.dat` files into
the PC game's `SAVE` folder after backing up existing saves.

The whole archive is limited to 128 KiB. Before confirmation, every member
name, header, extent, size, compression method and CRC is checked. Unknown
files, duplicate slots, path traversal, encrypted/split archives and oversized
expansion are rejected without writing. Matching slots commit together in
one IndexedDB transaction. Android transfers ZIP bytes through its document
picker, with no broad storage permission. The JSON format from build 28 is
replaced by this ZIP workflow. Game-engine and rendering bytes are unchanged.

## Version 1.00, build 30: Android controller input

A G8 Galileo report described a detected controller accepting only one or two
inputs. Android previously intercepted button keys before WebView, but read
joystick motion only in Activity's fallback after WebView. Native snapshots
also replaced browser Gamepad polling even when created by empty focus/pause
cleanup. A launcher regression test reproduced that empty-snapshot failure.

Joystick motion now enters through `dispatchGenericMotionEvent`, before
WebView, alongside the existing key dispatch. Button packets from a composite
device advertising GAMEPAD/JOYSTICK are accepted even when labelled KEYBOARD.
Native state tracks Android device IDs separately, so neutral motion or removal
of one HID interface cannot erase another interface's held buttons or stick.
Disconnect/focus cleanup explicitly releases native ownership, allowing browser
polling again. Native taps between game ticks are buffered for one tick, with
the same pause/clear rules as touch input. Logs include device names, source
masks and key/motion event counts for follow-up device reports.

Validation: 52 Node tests; JVM controller-state tests for standard buttons,
axes, hats/triggers, composite interfaces, cleanup and reconnect; real Chromium
launcher tests with simulated controller packets; existing GLES 2/3, audio,
touch and IDBFS persistence smoke tests. These do not verify a physical G8;
the reporter should retest the APK. Version remains 1.00 with versionCode 30.
The established APK signature and game-engine/WASM bytes are preserved.
