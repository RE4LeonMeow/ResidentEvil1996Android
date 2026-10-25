cmake_minimum_required(VERSION 3.21)
project(re1_asset_migrator LANGUAGES CXX)

# RE1 Asset Migrator - portable Qt GUI that migrates the game assets the port
# needs: the PC release trees (USA/JPN) and the Director's Cut overlay, with the
# video conversions (AVI/STR -> MP4). It is a tool, not part of the game build;
# Game.vcxproj and the root CMakeLists.txt do not reference it.
#
# The generator is the caller's choice; this build.ps1 picks the Visual Studio
# generator that matches the installed Visual Studio, and CI uses Ninja (the
# runner image's Visual Studio major version is not stable enough to pin).
#
#   cmake -S tools/asset_migrator -B tools/asset_migrator/build \
#         -G "Visual Studio 18 2026" -A x64 \
#         -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
#   cmake --build tools/asset_migrator/build --config Release

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

file(GLOB_RECURSE RE1AM_CORE CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.h")

add_library(re1assets_core STATIC ${RE1AM_CORE})
target_include_directories(re1assets_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(re1assets_core PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(re1assets_core PRIVATE /W3 /utf-8)
else()
    target_compile_options(re1assets_core PRIVATE -Wall -Wextra)
endif()

file(GLOB RE1AM_GUI CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

add_executable(re1_asset_migrator WIN32 ${RE1AM_GUI})
target_link_libraries(re1_asset_migrator PRIVATE Qt6::Widgets re1assets_core)
set_target_properties(re1_asset_migrator PROPERTIES
    OUTPUT_NAME "re1_asset_migrator")
if(MSVC)
    target_compile_options(re1_asset_migrator PRIVATE /W3 /utf-8)
endif()

# Headless verification driver (console), used to compare the C++ decoders
# against the Python tools.
add_executable(re1am_selftest "${CMAKE_CURRENT_SOURCE_DIR}/tests/selftest.cpp")
target_link_libraries(re1am_selftest PRIVATE re1assets_core)
if(MSVC)
    target_compile_options(re1am_selftest PRIVATE /W3 /utf-8)
endif()
