# CMakeLists.txt - Linux build (docs/LINUX_PORT.md).
#
# Windows still builds through Game.vcxproj / build.bat; this file is the
# source-of-truth list for the Linux target. The two lists must be kept in
# sync (a diff check is on the Phase 2 follow-up list).
#
# Build (from WSL/Linux, x86-32):
#   cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/linux -j
cmake_minimum_required(VERSION 3.16)
project(residentevil CXX)

if(WIN32)
    message(FATAL_ERROR
        "Windows builds use Game.vcxproj. This CMakeLists targets Linux.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Shared, platform-neutral translation units.
#
# Identical set to tests/compile_linux.sh. Windows-only TUs are excluded and
# have per-phase replacements: MarniDX.cpp -> MarniDX_GL.cpp (P2),
# MarniSound.cpp -> SDL audio (P6), MarniInput.cpp -> SDL input (P5),
# system/{CrashLog,DisplayConfig,Installation,SystemChecks,Cleanup}.cpp -> P8.
# VideoPlayback.cpp is shared since P7: the state machine moved behind the
# plat_video_* backend (MCI on Windows, ffmpeg on Linux).
# ---------------------------------------------------------------------------
set(RE1_SOURCES
    src/Globals.cpp
    src/system/AssetPath.cpp
    src/system/AudioFile.cpp
    src/system/PngImage.cpp
    src/system/ConfigFile.cpp

    src/marni/MarniBits.cpp
    src/marni/PSXTexture.cpp
    src/marni/Marni3DObject.cpp
    src/marni/MarniSystem.cpp

    src/video/VideoPlayback.cpp

    src/game/CharacterSelectionScreen.cpp
    src/game/CmdFunctions.cpp
    src/game/CollisionDebug.cpp
    src/game/ComputerLab.cpp
    src/game/DeathScreen.cpp
    src/game/DebugMenu.cpp
    src/game/DebugSaveLoad.cpp
    src/game/DebugScreens.cpp
    src/game/DoorSystem.cpp
    src/game/EffectSprites.cpp
    src/game/EffectSystem.cpp
    src/game/EndingScreen.cpp
    src/game/Ps1EndingCredits.cpp
    src/game/Ps1FmvSubtitles.cpp
    src/game/EntityModelLoader.cpp
    src/game/FadeSprite.cpp
    src/game/FileLoader.cpp
    src/game/GameInit.cpp
    src/game/GameLoop.cpp
    src/game/GameStart.cpp
    src/game/GteMatrix.cpp
    src/game/InputSystem.cpp
    src/game/InteractiveScreen.cpp
    src/game/JpnTextTables.cpp
    src/game/LabSlides.cpp
    src/game/LogosScreen.cpp
    src/game/MainLoop.cpp
    src/game/MainMenu.cpp
    src/game/MenuData.cpp
    src/game/ObjectManager.cpp
    src/game/OptionsMenu.cpp
    src/game/PathTrail.cpp
    src/game/PlayerAnimations.cpp
    src/game/PrintText.cpp
    src/game/Rendering.cpp
    src/game/Room.cpp
    src/game/RoomCollision.cpp
    src/game/RoomEvents.cpp
    src/game/RoomInit.cpp
    src/game/SaveLoadScreen.cpp
    src/game/SoundApi.cpp
    src/game/SoundSystem.cpp
    src/game/SoundTables.cpp
    src/game/SpriteRenderer.cpp
    src/game/TaskScheduler.cpp
    src/game/TextureLoader.cpp
    src/game/TitleScreen.cpp
    src/game/TmdAnimation.cpp
    src/game/TmdRenderer.cpp
    src/game/WeaponDamage.cpp
    src/game/dc/WeaponDamageTables.cpp
    src/game/dc/ItemTables.cpp
    src/game/dc/ItemNames.cpp
    src/game/dc/ItemModels.cpp
    src/game/dc/ItemDescriptions.cpp
    src/game/dc/Items.cpp
    src/game/dc/EntityModels.cpp
    src/game/dc/ArrangeStages.cpp
    src/game/dc/ArrangeSoundRows.cpp

    src/game/entities/Adder.cpp
    src/game/entities/BlackTiger.cpp
    src/game/entities/Cerberus.cpp
    src/game/entities/CharacterNpc.cpp
    src/game/entities/Chimera.cpp
    src/game/entities/ComputerArms.cpp
    src/game/entities/Crow.cpp
    src/game/entities/EntityCommon.cpp
    src/game/entities/Hunter.cpp
    src/game/entities/MonsterPlant.cpp
    src/game/entities/Neptune.cpp
    src/game/entities/Plant42.cpp
    src/game/entities/Plant42Roots.cpp
    src/game/entities/SpiderWeb.cpp
    src/game/entities/Tyrant.cpp
    src/game/entities/Wasp.cpp
    src/game/entities/WebSpinner.cpp
    src/game/entities/Yawn.cpp
    src/game/entities/Zombie.cpp
)

# wasm32 preserves the original pointer-bearing binary layouts on ARM64 devices.
if(EMSCRIPTEN)
    include(cmake/Web.cmake)
    return()
endif()

# ---------------------------------------------------------------------------
# Linux platform layer + renderer backend.
# ---------------------------------------------------------------------------
list(APPEND RE1_SOURCES
    src/platform/linux/platform.cpp
    src/platform/linux/input.cpp
    src/platform/linux/audio.cpp
    src/platform/linux/video.cpp
    src/platform/linux/config.cpp
    src/platform/linux/crash.cpp
    src/platform/linux/stubs.cpp
    src/platform/linux/main.cpp
    src/marni/MarniDX_GL.cpp
    src/marni/MarniGLFuncs.cpp
)

add_executable(residentevil ${RE1_SOURCES})

target_include_directories(residentevil PRIVATE src)

# ---------------------------------------------------------------------------
# 32-bit x86: the game's layout and the scheduler assume it.
#
# The 32-bit runtime is not on a 64-bit host's default search path, and where it
# lives is distro-specific:
#   Debian/Ubuntu   /usr/lib/i386-linux-gnu   (packages: *-dev:i386)
#   Arch/CachyOS    /usr/lib32                (packages: lib32-*)
# Detected rather than hardcoded so the same tree configures on both.
# ---------------------------------------------------------------------------
set(RE1_M32_LIBDIR "")
foreach(cand "/usr/lib/i386-linux-gnu" "/usr/lib32")
    if(RE1_M32_LIBDIR STREQUAL "" AND IS_DIRECTORY "${cand}")
        set(RE1_M32_LIBDIR "${cand}")
    endif()
endforeach()
if(RE1_M32_LIBDIR STREQUAL "")
    message(FATAL_ERROR
        "No 32-bit library directory found (looked for /usr/lib/i386-linux-gnu "
        "and /usr/lib32). Install the 32-bit runtime first.\n"
        "  Debian/Ubuntu: dpkg --add-architecture i386; apt install g++-multilib "
        "libc6-dev-i386 libsdl2-dev:i386 libavformat-dev:i386 libavcodec-dev:i386 "
        "libavutil-dev:i386 libswresample-dev:i386\n"
        "    On 24.04/noble libsdl2-dev:i386 is unresolvable (its "
        "libpulse-dev:i386/libibus-1.0-dev:i386 deps need an i386 "
        "libglib2.0-dev noble does not ship). Install libgl-dev, "
        "libsdl2-2.0-0:i386 and the ffmpeg :i386 dev packages, then "
        "dpkg -i --force-depends the downloaded libsdl2-dev:i386 - see the "
        "README's Linux section.\n"
        "  Arch/CachyOS:  enable [multilib] in /etc/pacman.conf; pacman -S "
        "lib32-glibc lib32-gcc-libs sdl2 lib32-sdl2 ffmpeg lib32-ffmpeg")
endif()
message(STATUS "32-bit library directory: ${RE1_M32_LIBDIR}")

target_compile_options(residentevil PRIVATE -m32 -w)
# -rdynamic: keep the dynamic symbol table so crash.cpp's backtrace can
# name functions instead of printing bare addresses.
# -L ahead of every library: the linker takes the first -L that matches, so the
# 32-bit directory wins over a 64-bit object of the same name.
target_link_options(residentevil PRIVATE -m32 -rdynamic -L${RE1_M32_LIBDIR})

# Resolve one link library out of the 32-bit directory. Neither `-lSDL2` nor a
# hardcoded soname is portable: the lib32-* packages do not always ship the
# unversioned dev symlink, and the ffmpeg sonames move per release (so.60 =
# ffmpeg 6 on Ubuntu 24.04, so.62 = ffmpeg 9 on Arch). Globbing the name prefix
# and taking the shortest match (libX.so < libX.so.62 < libX.so.62.11.100)
# picks whatever version the installed runtime provides; linking by path
# records that file's SONAME, so the result is what -lX would have produced.
function(re1_link_32bit name out_var)
    file(GLOB candidates
        "${RE1_M32_LIBDIR}/lib${name}.so*"
        "${RE1_M32_LIBDIR}/lib${name}-*.so*")
    if(NOT candidates)
        message(FATAL_ERROR
            "lib${name} is not installed in ${RE1_M32_LIBDIR}.\n"
            "  Arch/CachyOS:  pacman -S lib32-${name}\n"
            "  Debian/Ubuntu: apt install lib${name}-dev:i386")
    endif()
    list(SORT candidates)
    list(GET candidates 0 resolved)
    set(${out_var} "${resolved}" PARENT_SCOPE)
endfunction()

# Headers. Arch installs both sets into /usr/include (the lib32-* packages are
# runtime only, so the ordinary sdl2/ffmpeg packages supply the headers), but
# Debian's i386 dev packages put them in the multiarch include directory, which
# is not on the default search path.
set(RE1_INCLUDE_HINTS /usr/include /usr/include/i386-linux-gnu)
find_path(RE1_SDL2_INCLUDE_DIR SDL.h
    PATH_SUFFIXES SDL2 HINTS ${RE1_INCLUDE_HINTS})
if(NOT RE1_SDL2_INCLUDE_DIR)
    message(FATAL_ERROR "SDL.h not found.\n"
        "  Arch/CachyOS:  pacman -S sdl2\n"
        "  Debian/Ubuntu: apt install libsdl2-dev:i386")
endif()
find_path(RE1_AVFORMAT_INCLUDE_DIR libavformat/avformat.h
    HINTS ${RE1_INCLUDE_HINTS})
if(NOT RE1_AVFORMAT_INCLUDE_DIR)
    message(FATAL_ERROR "libavformat/avformat.h not found.\n"
        "  Arch/CachyOS:  pacman -S ffmpeg\n"
        "  Debian/Ubuntu: apt install libavformat-dev:i386")
endif()
target_include_directories(residentevil PRIVATE
    ${RE1_SDL2_INCLUDE_DIR} ${RE1_AVFORMAT_INCLUDE_DIR})

# FMV decoding (Phase 7). The movies are Cinepak AVIs; ffmpeg supplies the
# decoder, the container parser and the audio resampler. The frame is packed to
# RGBA by hand (Cinepak decodes to packed RGB), so swscale is not needed.
re1_link_32bit(SDL2       RE1_SDL2_LIB)
re1_link_32bit(avformat   RE1_AVFORMAT_LIB)
re1_link_32bit(avcodec    RE1_AVCODEC_LIB)
re1_link_32bit(avutil     RE1_AVUTIL_LIB)
re1_link_32bit(swresample RE1_SWRESAMPLE_LIB)

# No GL and no dl, deliberately: every GL entry point above 1.2 is resolved at
# runtime through SDL_GL_GetProcAddress (MarniGLFuncs), and nothing calls
# dlopen/dlsym, so neither library contributes a symbol. Linking them would
# only add a link-time requirement the lib32 packages need not satisfy (Arch's
# lib32-mesa ships no unversioned libGL.so). The GL runtime the context needs
# is dlopen'd by SDL at startup, not at link time.
target_link_libraries(residentevil PRIVATE
    ${RE1_SDL2_LIB}
    ${RE1_AVFORMAT_LIB} ${RE1_AVCODEC_LIB} ${RE1_AVUTIL_LIB} ${RE1_SWRESAMPLE_LIB}
    m)
