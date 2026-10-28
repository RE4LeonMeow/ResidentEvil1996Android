if(NOT RE1_FFMPEG_ROOT)
    message(FATAL_ERROR "Set RE1_FFMPEG_ROOT to the wasm32 FFmpeg install prefix; see docs/ANDROID.md")
endif()
list(APPEND RE1_SOURCES
    src/platform/linux/platform.cpp
    src/platform/linux/input.cpp
    src/platform/linux/audio.cpp
    src/platform/linux/video.cpp
    src/platform/linux/stubs.cpp
    src/platform/linux/main.cpp
    src/platform/web/services.cpp
    src/marni/MarniDX_GL.cpp
)
add_executable(residentevil ${RE1_SOURCES})
set_target_properties(residentevil PROPERTIES SUFFIX ".js")
target_include_directories(residentevil PRIVATE src "${RE1_FFMPEG_ROOT}/include")
target_compile_options(residentevil PRIVATE -O2 -sUSE_SDL=2 -sUSE_ZLIB=1 -fno-strict-aliasing)
set_property(TARGET residentevil APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_SOURCE_DIR}/android/app/src/main/assets/web/runtime.js")
target_link_options(residentevil PRIVATE
    -O2 -sUSE_SDL=2 -sUSE_ZLIB=1 -sMIN_WEBGL_VERSION=1 -sMAX_WEBGL_VERSION=2
    -sEMULATE_FUNCTION_POINTER_CASTS=1 -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=131072 -sSTACK_SIZE=2097152
    -sINITIAL_MEMORY=134217728 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=1073741824
    -sFORCE_FILESYSTEM=1 -sEXIT_RUNTIME=1 -sASSERTIONS=1
    "-sEXPORTED_FUNCTIONS=['_main','_re1_web_suspend','_re1_web_settings_save','_re1_web_request_quit','_re1_web_quick_turn','_re1_web_cheat','_re1_web_set_smooth','_re1_web_localization_load','_re1_web_restore_game_actions','_re1_web_game_action_binding','_re1_web_campaign_load','_re1_web_scene_state','_re1_web_enemy_state']"
    "-sEXPORTED_RUNTIME_METHODS=['FS','callMain']"
    -sINVOKE_RUN=0 -sDYNAMIC_EXECUTION=0
    --pre-js "${CMAKE_SOURCE_DIR}/android/app/src/main/assets/web/runtime.js"
    -lidbfs.js)
target_link_libraries(residentevil PRIVATE
    "${RE1_FFMPEG_ROOT}/lib/libavformat.a"
    "${RE1_FFMPEG_ROOT}/lib/libavcodec.a"
    "${RE1_FFMPEG_ROOT}/lib/libswresample.a"
    "${RE1_FFMPEG_ROOT}/lib/libavutil.a")
