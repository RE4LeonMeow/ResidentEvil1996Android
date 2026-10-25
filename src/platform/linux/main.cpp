// main.cpp - Linux entry point.
//
// Counterpart of src/platform/win32/main.cpp: reads config.ini, creates
// the SDL2 window and GL context, brings up the Marni system, then drives
// main_loop() at the engine's 33 ms tick exactly as RunMessageLoop does on
// Windows (see src/platform/win32/main.cpp:706-757 for the
// instruction-for-instruction commentary on that limiter).
#include <SDL2/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "../web/frame_pacing.h"
#include "../web/frame_pacing.inl"
extern "C" void re1_web_flush();
extern "C" void re1_web_finish(int status);
extern "C" void re1_web_stage(const char* phase);
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Globals.h"
#include "platform/platform.h"
#include "system/AssetPath.h"
#include "system/ConfigFile.h"
#include "marni/MarniDX.h"
#include "marni/MarniGLFuncs.h"
#include "marni/MarniSystem.h"

// 0x00441f26: the original's frame interval, 0x21 == 33 ms == 30 ticks/s. The
// play clock and every scripted wait are built on it.
static const int kFrameIntervalMs = 33;
static const BOOL kFrameLimiterEnabled = TRUE;
#ifdef __EMSCRIPTEN__
#include "../web/graphics_check.inl"
#endif

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
#ifdef __EMSCRIPTEN__
    // Keep WebView's persistence runtime alive after the game stops.
    struct WebResult {
        int status = 1;
        ~WebResult() { re1_web_finish(status); }
    } webResult;
#endif
    // Crash diagnostics and the single-instance guard, in the game root like
    // their Windows counterparts (WinMain does the same, in this order).
    crashlog_install();
    if (!plat_single_instance_check()) {
        fprintf(stderr, "[RE1] RESIDENT EVIL is already running.\n");
        return 3;
    }

    // --capture <file> [frames]: render normally, dump the back buffer to
    // <file> once `frames` have been presented (default 120 ≈ 4 s), then exit.
    // This is the static-frame comparison hook from docs/LINUX_PORT.md 6.7.
    const char* capturePath = NULL;
    int captureFrame = 120;

    // --press <scancode> <frame> [hold]: inject one synthetic key press at
    // `frame` (down, then up `hold` frames later; default 3). Lets the boot
    // flow be driven past "PRESS ANY BUTTON" without a keyboard, so
    // interactivity is testable headlessly. SDL_SCANCODE_E == 8 is the default
    // confirm key; a long hold drives held-key behaviour (walking, rotating a
    // model in the item viewer).
    struct PressEvent { int scancode; int frame; int hold; };
    PressEvent presses[16];
    int pressCount = 0;
#ifdef __EMSCRIPTEN__
    bool graphicsTest = false;
    int graphicsFrames = 0;
    MarniHandle testTexture = MARNI_NULL_HANDLE;
#endif

    for (int i = 1; i < argc; ++i) {
#ifdef __EMSCRIPTEN__
        if (strcmp(argv[i], "--graphics-test") == 0) { graphicsTest = true; continue; }
#endif
        if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc) {
            capturePath = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') captureFrame = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--press") == 0 && i + 2 < argc && pressCount < 16) {
            presses[pressCount].scancode = atoi(argv[++i]);
            presses[pressCount].frame = atoi(argv[++i]);
            presses[pressCount].hold = 3;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                presses[pressCount].hold = atoi(argv[++i]);
            }
            ++pressCount;
        }
    }

    // config.ini is the settings store for both builds now: created with
    // documented defaults on first run, then read here. The same file drives
    // the Windows build, so settings travel between them.
#ifdef __EMSCRIPTEN__
    re1_web_stage("Reading game settings");
#endif
    ConfigFile_EnsureExists();
    ConfigFile_Load();
#ifdef __EMSCRIPTEN__
    // Launcher-owned output size, independent of original logical 320x240
    // coordinates and save data. Turning the HD pack off restores 640x480.
    if(getenv("RE1_HD_MODE")) {
        const bool hd=strcmp(getenv("RE1_HD_MODE"),"1")==0;
        g_dwScreenWidth=hd?1440:640;g_dwScreenHeight=hd?1080:480;
    }
#endif

    const int width  = (int)g_dwScreenWidth;
    const int height = (int)g_dwScreenHeight;

#ifdef __EMSCRIPTEN__
    // This loop already yields to the browser. SDL otherwise inserts an
    // additional emscripten_sleep(0) after every swap, which can be delayed
    // without an active touch even when the rendering work is inexpensive.
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

#ifdef __EMSCRIPTEN__
    re1_web_stage("Creating OpenGL context");
    const bool compatGL = getenv("RE1_WEB_GLES") && strcmp(getenv("RE1_WEB_GLES"), "2") == 0;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, compatGL ? 2 : 3);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#endif
#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    SDL_Window* window = SDL_CreateWindow("RESIDENT EVIL", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, width, height, flags);
    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    if (g_bFullScreen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (ctx == NULL) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, ctx);
    SDL_GL_SetSwapInterval(g_bVSync ? 1 : 0);

    if (!MarniGL_LoadFunctions((void* (*)(const char*))SDL_GL_GetProcAddress)) {
        fprintf(stderr, "failed to resolve GL entry points\n");
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    printf("[RE1] GL %s | %s\n",
           (const char*)glGetString(GL_VERSION),
           (const char*)glGetString(GL_RENDERER));

    g_hWnd = window;

    // Windows brings the sound system up from WM_CREATE (WindowProc.cpp ->
    // StartSoundSystemAsync). There is no window procedure here, so do it
    // explicitly - without it g_pDirectSound stays NULL and every bank load
    // reports "NOT READY".
#ifdef __EMSCRIPTEN__
    re1_web_stage("Starting audio");
#endif
    StartSoundSystemAsync(g_hWnd);

    // 0x004414f6: allocate the guard-paged task stacks. the Windows main.cpp calls this
    // in WinMain before anything schedules a task; without it the first
    // TaskSwitch_Start passes a null stack to makecontext.
    TaskScheduler_Init();

    // Bring up CMarniDirect3D + the renderer backend. Same call the Windows
    // build makes; the constructor creates the MarniDX backend internally.
#ifdef __EMSCRIPTEN__
    re1_web_stage("Preparing renderer and tasks");
#endif
    InitializeMarniSystem();
    if (!IsGraphicsSystemReadyForOperation()) {
        fprintf(stderr, "graphics system failed to initialize\n");
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    g_bWindowFocused = TRUE;
    g_bWindowActive  = TRUE;
    g_bQuitFlag      = FALSE;

    DWORD lastTick = plat_time_ms();
    g_dwGameTimer1 = lastTick;
    int running = 1;
    int presented = 0;

    while (running) {
#ifdef __EMSCRIPTEN__
        // Keep a browser display callback active while awaiting the original
        // tick deadline. The unchanged limiter below admits only due ticks.
        WebWaitForFrame(WebFrameWaitMs(plat_time_ms(), g_dwGameTimer1,
            g_bWindowFocused, g_bUseFrameSkip==1, g_bFrameSkipDetected),g_bWindowFocused);
#endif
        for (int p = 0; p < pressCount; ++p) {
            // Hold for at least 3 frames so the rising edge is always observed.
            if (presented == presses[p].frame) {
                SDL_Event ev = {};
                ev.type = SDL_KEYDOWN;
                ev.key.type = SDL_KEYDOWN;
                ev.key.state = SDL_PRESSED;
                ev.key.repeat = 0;
                ev.key.keysym.scancode = (SDL_Scancode)presses[p].scancode;
                ev.key.keysym.sym = SDL_GetKeyFromScancode((SDL_Scancode)presses[p].scancode);
                SDL_PushEvent(&ev);
            } else if (presented == presses[p].frame + presses[p].hold) {
                SDL_Event ev = {};
                ev.type = SDL_KEYUP;
                ev.key.type = SDL_KEYUP;
                ev.key.state = SDL_RELEASED;
                ev.key.keysym.scancode = (SDL_Scancode)presses[p].scancode;
                ev.key.keysym.sym = SDL_GetKeyFromScancode((SDL_Scancode)presses[p].scancode);
                SDL_PushEvent(&ev);
            }
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_WINDOWEVENT:
#ifndef __EMSCRIPTEN__
                if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) g_bWindowFocused = TRUE;
                else if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) g_bWindowFocused = FALSE;
#endif
                if (e.window.event == SDL_WINDOWEVENT_CLOSE) running = 0;
                break;
            case SDL_KEYDOWN:
                plat_key_event(e.key.keysym.scancode, TRUE);
                break;
            case SDL_KEYUP:
                plat_key_event(e.key.keysym.scancode, FALSE);
                break;
            default:
                break;
            }
        }

        if (!running || g_bQuitFlag) break;
        if (!g_bWindowFocused) {
#ifndef __EMSCRIPTEN__
            SDL_Delay(1);
#endif
            continue;
        }

        DWORD now = plat_time_ms();

        // 0x00441f0c: the frame limiter. Skip a tick that arrives early.
        if (kFrameLimiterEnabled) {
            BOOL bSkipFrame = FALSE;
            if (g_bUseFrameSkip == 1) {
                if (!g_bFrameSkipDetected) {
                    if ((int)(kFrameIntervalMs + g_dwGameTimer1) > (int)now &&
                        (int)(g_dwGameTimer1 - 0x10000) < (int)now) {
                        bSkipFrame = TRUE;
                    }
                }
            } else {
                if ((int)(g_dwGameTimer1 + 16) > (int)now &&
                    (int)(g_dwGameTimer1 - 0x10000) < (int)now) {
                    bSkipFrame = TRUE;
                }
            }
            if (bSkipFrame) {
#ifndef __EMSCRIPTEN__
                SDL_Delay(1);
#endif
                // Web already yielded above. Avoid adding another timer
                // before checking the next display callback/tick deadline.
                continue;
            }
        }

        g_dwGameTimer1 = now;

#ifdef __EMSCRIPTEN__
        if (graphicsTest) {
            if (!WebGraphicsCheck(Marni_DX(), &testTexture, graphicsFrames == 0)) {
                re1_web_graphics_result(0);
                plat_fatal("Graphics test failed: texture/shader readback did not match. Try the other OpenGL renderer.");
            }
            if (++graphicsFrames >= 90) break;
            continue;
        }
#endif

        // Capture before the FMV gate: a movie's frames are presented by the
        // video backend, and the frame counter has to advance for those too or
        // --capture can never land inside a movie.
        if (capturePath != NULL && ++presented >= captureFrame) {
            MarniDX* dx = Marni_DX();
            void* pixels = NULL;
            DWORD cw = 0, ch = 0;
            if (dx != NULL && dx->CaptureBackbufferToRGBA(&pixels, &cw, &ch) && pixels != NULL) {
                FILE* f = fopen(capturePath, "wb");
                if (f != NULL) {
                    fprintf(f, "RE1CAP %u %u\n", (unsigned)cw, (unsigned)ch);
                    fwrite(pixels, 1, (size_t)cw * ch * 4, f);
                    fclose(f);
                    printf("[RE1] captured frame %d -> %s (%ux%u)\n",
                           presented, capturePath, (unsigned)cw, (unsigned)ch);
                }
                free(pixels);
            } else {
                fprintf(stderr, "[RE1] capture failed\n");
            }
            break;
        }

        // 0x00441dfe: FMV playback. While a movie is up the message loop runs
        // the video state machine instead of the game - the same gate
        // main.cpp uses, and the reason a movie freezes the game
        // underneath it.
        if (g_bMCINotifyEnabled) {
            if (g_bMCINotifyFlag) {
                UpdateVideoPlayback();
                continue;
            }
            g_bMCINotifyFlag = TRUE;
        }

#ifdef __EMSCRIPTEN__
        if (!init_game_flag) re1_web_stage("Loading game boot assets");
#endif
#ifdef __EMSCRIPTEN__
        const bool firstTick = !init_game_flag;
#endif
        if (main_loop() == 0) break;
#ifdef __EMSCRIPTEN__
        if (firstTick) re1_web_stage("Game loop running");
#endif

        if (getenv("RE1_INPUT_DEBUG") != NULL && (presented % 30) == 0) {
            fprintf(stderr, "[IN] f=%d keyMap[11]=0x%02x kbdCurr=0x%08x kbdPrev=0x%08x "
                            "padPressed=0x%x sw=%d btnWord=0x%x\n",
                    presented, g_pMasterInputState.keyMap[11],
                    (unsigned)g_pMasterInputState.keyboardCurr,
                    (unsigned)g_pMasterInputState.keyboardPrev,
                    (unsigned)g_PlayerPadPressed, read_sidewinder_pad(),
                    (unsigned)g_button_pressed_id);
        }

        if ((int)(g_dwSystemTimer1 + 1000) < (int)now) {
            g_dwSystemTimer1 = now;
        }
        (void)lastTick;
    }

#ifdef __EMSCRIPTEN__
    if (testTexture != MARNI_NULL_HANDLE && Marni_DX()) Marni_DX()->DestroyTexture(testTexture);
#endif
    // Write the session's settings back, exactly where the Windows build's
    // message loop calls it on exit.
    CleanupVideoConfigAndSaveAllSettings();
#ifdef __EMSCRIPTEN__
    re1_web_flush();
#endif

    MarniDX_DestroyGlobal();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
#ifdef __EMSCRIPTEN__
    webResult.status = 0;
#endif
    return 0;
}
