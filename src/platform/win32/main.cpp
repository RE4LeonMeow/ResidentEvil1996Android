// main.cpp - Entry point for Resident Evil 1 PC
// Original function: main at 0x00441350 (Ghidra)
// Adapted from Ghidra decompilation for modern Win32/VS2022

#include "Globals.h"
#include "system/AssetPath.h"
#include "system/ConfigFile.h"

// Forward declarations for local helpers
static int  CheckSystemRequirements(void);
static BOOL CheckSingleInstance(void);
static BOOL VerifyInstallation(void);
static void RunSetup(void);
static void ResolveDisplayMode(void);
static BOOL LoadIniConfiguration(void);
static BOOL InitSoftwareRenderer(void);
static BOOL CreateGameWindow(int nCmdShow);
static int  RunMessageLoop(void);

// ============================================================================
// LoadIniConfiguration - Read settings from config.ini
//
// config.ini is the settings store for both builds now (src/system/
// ConfigFile.h). The registry is still read first so an install that predates
// the switch keeps its bindings and play counts; config.ini then overrides it
// and is created with documented defaults when it does not exist.
// ============================================================================
static BOOL LoadIniConfiguration(void)
{
    if (IsGameInstalled()) {
        LoadInstallationConfiguration((BYTE*)g_szInstallPath);
    }

    ConfigFile_EnsureExists();
    return ConfigFile_Load();
}

// ============================================================================
// WinMain - Entry point (0x00441350)
// ============================================================================
int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Dev tooling: capture fail-fast precursors to crash.log (see CrashLog.cpp)
    extern void crashlog_install(void);
    crashlog_install();

    // 0x00441350: __chkstk() - stack probe for large stack frame

    // --- 0x0044136b: OS Version Check ---
    OSVERSIONINFOA osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    GetVersionExA(&osvi);
    
    // --- 0x0044137b: Load DirectInput DLL ---
    LoadLibraryA("dinput.dll");
    
    // --- 0x0044138f: Memory Check ---
    MEMORYSTATUS memStatus = {};
    GlobalMemoryStatus(&memStatus);
    GlobalMemoryStatus(&memStatus); // Called twice in original
    
    // --- 0x00441385: If less than 20MB physical, check paging file on system drive ---
    if (memStatus.dwTotalPhys < 0x01400000) {
        char sysDir[MAX_PATH];
        GetSystemDirectoryA(sysDir, MAX_PATH);
        
        char pagingDrive[260] = {};
        GetPrivateProfileStringA("386Enh", "PagingDrive", "", pagingDrive, 260, "system.ini");
        
        DWORD freeMB = GetFreeDiskSpaceMB(pagingDrive);
        if (freeMB < 100) {
            char msg[512];
            sprintf(msg, "%s %ld MByte", msg, freeMB);
            int result = ShowMessageBox(NULL, msg, "RESIDENT EVIL", MB_OKCANCEL);
            if (result == IDCANCEL) {
                return 0;
            }
        }
    }
    
    // --- 0x00441452: Color depth check ---
    HDC hdc = GetDC(NULL);
    int bitsPerPixel = GetDeviceCaps(hdc, BITSPIXEL);
    ReleaseDC(NULL, hdc);
    
    if (bitsPerPixel < 16) {
        ShowMessageBox(NULL, 
            "Resident Evil cannot be run in 8-bit or lower color depth.\n"
            "Please change the color depth to 16-bit or higher.",
            "RESIDENT EVIL", MB_OK | MB_ICONSTOP);
        return 1;
    }
    
    // --- 0x00441486: CD-ROM check (skipped in dev mode) ---
    BOOL hasCDROM = EnumerateDriveTypes();
    
    // --- 0x004414a2: Single instance checks via mutex ---
    if (!CheckSingleInstance()) {
        return 3; // already running
    }
    
    // --- 0x004414f6: Initialize task scheduler stacks ---
    TaskScheduler_Init();
    
    // --- DEV MODE: Try INI config before installation checks ---
    BOOL bDevMode = LoadIniConfiguration();
    
    if (!bDevMode) {
        if (!hasCDROM) {
            ShowMessageBox(NULL,
                "Cannot find a CD-ROM drive. RESIDENT EVIL discontinues.",
                "RESIDENT EVIL", MB_OK | MB_ICONSTOP);
            return 2;
        }
        
        // --- 0x004414fc: Original path - check if game is installed ---
        if (!VerifyInstallation()) {
            // Not installed or config load failed after user retry
            RunSetup();
            DWORD exitCode = 0;
            char currentDir[MAX_PATH];
            GetCurrentDirectoryA(MAX_PATH, currentDir);
            CloseHandle(g_hMutex);
            return 7; // Setup completed, restart needed
        }
        
        // --- 0x004416c9: Build install path and create directory ---
        char fullPath[MAX_PATH];
        sprintf(fullPath, "%s", g_szInstallPath);
        char* lastSlash = strrchr(fullPath, '\\');
        if (lastSlash) {
            *lastSlash = '\0';
        }
        
        SECURITY_ATTRIBUTES saLocal = {sizeof(SECURITY_ATTRIBUTES), NULL, FALSE};
        CreateDirectoryA(fullPath, &saLocal);
        
        // Copy install path to working path
        size_t pathLen = strlen(g_szInstallPath);
        char workingPath[MAX_PATH];
        memcpy(workingPath, g_szInstallPath, pathLen + 1);
        
        // Open shared memory (file mapping)
        HANDLE hSharedMemory = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "bio1997");
        g_hFileMapping = hSharedMemory;
        
        if (hSharedMemory != NULL) {
            BYTE* pSharedView = (BYTE*)MapViewOfFile(hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, 0);
            g_pSharedMemory = pSharedView;
            
            if (pSharedView != NULL) {
                g_dwSelectedDisplayAdapterID = *(DWORD*)(pSharedView + 0);
                g_dwSelectedDisplayAdapterID = *(DWORD*)(pSharedView + 0);
                g_dwScreenWidth   = *(DWORD*)(pSharedView + 4);
                g_dwScreenHeight  = *(DWORD*)(pSharedView + 8);
                g_bFullScreen     = *(BOOL*)(pSharedView + 12);
                *(BYTE*)(pSharedView + 16) = 1;
                g_dwSelectedDisplayModeID = *(DWORD*)(pSharedView + 20);
                UnmapViewOfFile(pSharedView);
                g_pSharedMemory = NULL;
            }
        } else {
            ResolveDisplayMode();
        }
    }
    
    // --- 0x00441a4e: Adjust adapter ID (skip in dev mode) ---
    if (!bDevMode) {
        if (g_dwSelectedDisplayAdapterID == 6) {
            g_dwSelectedDisplayAdapterID = 3;
        }
        if (g_dwSelectedDisplayAdapterID == 4) {
            g_dwSelectedDisplayAdapterID = 0;
        }
        if (g_dwSelectedDisplayAdapterID == 5 || g_dwSelectedDisplayAdapterID == 7) {
            g_dwSelectedDisplayModeID = 0;
        }
    }
    
    // --- 0x00441a7e: Determine if software rendering ---
    g_bIsSoftwareRendering = (g_dwSelectedDisplayAdapterID == 0);
    g_dwPlayCount++;
    g_GPU_VENDOR_ID = g_dwSelectedDisplayAdapterID;
    
    // --- 0x00441a96: Software renderer setup ---
    if (g_bIsSoftwareRendering) {
        if (!InitSoftwareRenderer()) {
            return 9; // Software rendering memory error
        }
    }
    
    // --- 0x00441ba8: System parameters ---
    g_hInstance = hInstance;
    SystemParametersInfoA(SPI_GETANIMATION, 0, &g_bAccessibilityAnimations, 0);
    if (g_bAccessibilityAnimations) {
        SystemParametersInfoA(SPI_SETANIMATION, 0, (PVOID)FALSE, SPIF_SENDCHANGE);
    }
    
    // --- 0x00441bfc: Create game window ---
    if (!CreateGameWindow(nCmdShow)) {
        DestroyAllSoundBanks();
        CleanupAsyncTasks();
        CleanupVideoConfigAndSaveAllSettings();
        CloseHandle(g_hMutex);
        CleanupSharedMemory();
        return 0;
    }
    
    // --- 0x00441d31: Initialize Marni System ---
    InitializeMarniSystem();
    if (g_pMarniDirect3D == NULL) {
        OutputDebugStringA("[MAIN] g_pMarniDirect3D is NULL after InitMarniSystem, destroying window\n");
        DestroyWindow(g_hWnd);
    } else {
        OutputDebugStringA("[MAIN] g_pMarniDirect3D OK after InitMarniSystem\n");
    }
    
    // --- 0x00441d45: Enumerate display info ---
    EnumerateDisplayModes();
    EnumerateD3DRenderers();
    
    // --- 0x00441d71: Main message loop ---
    int exitCode = RunMessageLoop();
    
    // Final cleanup (this is after GetMessage returns 0 or error)
    return exitCode;
}

// ============================================================================
// CheckSystemRequirements - Check memory, color depth, CD-ROM
// 0x00441385 - 0x00441498
// ============================================================================
int CheckSystemRequirements(void)
{
    MEMORYSTATUS memStatus = {};
    GlobalMemoryStatus(&memStatus);
    
    if (memStatus.dwTotalPhys < 0x01400000) {
        char pagingDrive[260] = {};
        GetPrivateProfileStringA("386Enh", "PagingDrive", "", pagingDrive, 260, "system.ini");
        
        DWORD freeMB = GetFreeDiskSpaceMB(pagingDrive);
        if (freeMB < 100) {
            char msg[512];
            sprintf(msg, "Resident Evil has detected that less than 100 megabytes\n"
                "of free hard space is available.\n"
                "Loading the game without this required amount may cause problems.\n"
                "It is advised that you clear the sufficient space before proceeding.\n"
                "Do you wish to load Resident Evil anyway?");
            int result = ShowMessageBox(NULL, msg, "RESIDENT EVIL", MB_OKCANCEL);
            if (result == IDCANCEL) {
                return 0;
            }
        }
    }
    
    HDC hdc = GetDC(NULL);
    int bitsPerPixel = GetDeviceCaps(hdc, BITSPIXEL);
    ReleaseDC(NULL, hdc);
    
    if (bitsPerPixel < 16) {
        ShowMessageBox(NULL,
            "Resident Evil cannot be run in 8-bit or lower color depth.\n"
            "Please change the color depth to 16-bit or higher.",
            "RESIDENT EVIL", MB_OK | MB_ICONSTOP);
        return 1;
    }
    
    return -1; // OK
}

// ============================================================================
// CheckSingleInstance - Check mutexes to prevent double start
// 0x004414a2 - 0x004414ea
// ============================================================================
BOOL CheckSingleInstance(void)
{
    g_hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, "RESIDENT EVIL PC DoubleStart Check");
    if (g_hMutex != NULL) {
        CloseHandle(g_hMutex);
        return FALSE;
    }
    
    g_hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, "RESIDENT EVIL PC Setup DoubleStart Check");
    if (g_hMutex != NULL) {
        CloseHandle(g_hMutex);
        return FALSE;
    }
    
    g_hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, "RESIDENT EVIL PC Uninstall DoubleStart Check");
    if (g_hMutex != NULL) {
        CloseHandle(g_hMutex);
        return FALSE;
    }
    
    g_hMutex = CreateMutexA(NULL, FALSE, "RESIDENT EVIL PC DoubleStart Check");
    return TRUE;
}

// ============================================================================
// VerifyInstallation - Check registry and load configuration
// 0x004414fc - 0x004416bc
// ============================================================================
BOOL VerifyInstallation(void)
{
    g_szInstallPath[0] = '\0';
    
    if (!IsGameInstalled()) {
        return FALSE; // Will trigger setup
    }
    
    if (!LoadInstallationConfiguration((BYTE*)g_szInstallPath)) {
        // Config failed to load - prompt user to retry
        while (!LoadInstallationConfiguration((BYTE*)g_szInstallPath)) {
            int result = ShowMessageBox(NULL,
                "Failed to load game configuration.",
                "RESIDENT EVIL", MB_RETRYCANCEL);
            if (result != IDRETRY) {
                CloseHandle(g_hMutex);
                return FALSE;
            }
        }
    }
    
    return TRUE;
}

// ============================================================================
// RunSetup - Launch setup.exe and wait for completion
// 0x00442060
// ============================================================================
void RunSetup(void)
{
    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi = {};
    
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, FALSE};
    
    BOOL result = CreateProcessA("\\Setup.exe", NULL,
        &sa, &sa, FALSE, CREATE_NEW_CONSOLE, NULL, ".", &si, &pi);
    
    if (result) {
        CloseHandle(pi.hThread);
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
    }
}

// ============================================================================
// ResolveDisplayMode - Determine display adapter and mode
// 0x00441996 - 0x00441a46
// ============================================================================
void ResolveDisplayMode(void)
{
    // Try each adapter in sequence
    int capsResult = CheckVideoCapabilities();
    
    // The original checks capsResult in a chain of if/else for each adapter (0-7)
    // We simplify here but preserve the logic structure
    
    // Adapter 0: software 
    if (capsResult == 0) {
        g_dwSelectedDisplayAdapterID = 0;
        g_dwSelectedDisplayModeID = 0;
        return;
    }
    
    // Check each additional adapter capability
    // In the original, CheckVideoCapabilities is called multiple times
    // with each call returning the highest functioning adapter index
    INT_PTR modeResult = (INT_PTR)EnumerateAndSelectDisplayMode();
    
    if (modeResult == -1) {
        // Failed - use software defaults
        g_dwSelectedDisplayAdapterID = 0;
        g_dwSelectedDisplayModeID = 0;
        return;
    }
    
    if (modeResult == 0) {
        // Windowed 640x480
        g_dwScreenWidth = 640;
        g_dwScreenHeight = 480;
        g_bFullScreen = FALSE;
        g_dwSelectedDisplayModeID = 0;
    } else if (modeResult == 1) {
        // Windowed 640x480 another variant
        g_dwScreenWidth = 640;
        g_dwScreenHeight = 480;
        g_bFullScreen = FALSE;
        g_dwSelectedDisplayModeID = 1;
    } else {
        // Fullscreen mode
        g_bFullScreen = TRUE;
        int idx = (int)modeResult;
        g_dwScreenWidth  = g_DisplayModeBuffer[idx].dwWidth;
        g_dwScreenHeight = g_DisplayModeBuffer[idx].dwHeight;
        g_dwSelectedDisplayModeID = (DWORD)modeResult;
    }
}

// ============================================================================
// InitSoftwareRenderer - Set up software rendering shared memory
// 0x00441a96
// ============================================================================
BOOL InitSoftwareRenderer(void)
{
    g_hFileMapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
        0, 0x11C, "bio1997");
    
    if (g_hFileMapping == NULL || g_hFileMapping == INVALID_HANDLE_VALUE) {
        ShowMessageBox(NULL,
            "Software emulation is not available.",
            "RESIDENT EVIL", MB_OK | MB_ICONSTOP);
        CloseHandle(g_hMutex);
        return FALSE;
    }
    
    g_pSharedMemory = (BYTE*)MapViewOfFile(g_hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (g_pSharedMemory == NULL) {
        ShowMessageBox(NULL,
            "Software emulation is not available.",
            "RESIDENT EVIL", MB_OK | MB_ICONSTOP);
        CloseHandle(g_hMutex);
        return FALSE;
    }
    
    // Initialize shared memory
    g_pSharedMemory[0] = 1;  // Ready flag
    g_pSharedMemory[1] = 0;  // ACK flag
    g_pSharedMemory[4] = 0;  // Status flag
    
    // Init slots 0xC through 0x1C with 0xFF
    for (int i = 0; i < 8; i++) {
        g_pSharedMemory[0x0C + i] = 0xFF;
        g_pSharedMemory[0x14 + i] = 0;
    }
    
    return TRUE;
}

// ============================================================================
// CreateGameWindow - Register class and create the game window
// 0x00441bfc
// ============================================================================
BOOL CreateGameWindow(int nCmdShow)
{
    DWORD dwStyle;
    DWORD dwExStyle;
    
    // Window style based on fullscreen, not adapter ID
    // Original used adapter ID 5/7 for windowed; we use g_bFullScreen directly
    //
    // 0x00441bd1/0x00441bda pick between two immediates:
    //   windowed   0x00441c15: 0x00CA0000 = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX
    //   fullscreen 0x00441bdc: 0x82000000 = WS_POPUP | WS_CLIPCHILDREN
    // The windowed style deliberately omits WS_THICKFRAME (0x00040000) and
    // WS_MAXIMIZEBOX (0x00010000): the game renders at one fixed back-buffer
    // size, so the border must not be draggable and the maximise button must
    // show up greyed out. WS_OVERLAPPEDWINDOW carries both of those bits, which
    // is what made this port resizable.
    if (g_bFullScreen) {
        dwStyle = WS_POPUP | WS_CLIPCHILDREN;
        dwExStyle = WS_EX_TOPMOST;
    } else {
        dwStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        dwExStyle = WS_EX_APPWINDOW;
    }
    
    // Calculate window rect
    RECT windowRect = {0, 0, (LONG)g_dwScreenWidth, (LONG)g_dwScreenHeight};
    HWND hDesktop = GetDesktopWindow();
    RECT desktopRect;
    GetWindowRect(hDesktop, &desktopRect);
    AdjustWindowRect(&windowRect, dwStyle, FALSE);
    
    int winWidth = windowRect.right - windowRect.left;
    int winHeight = windowRect.bottom - windowRect.top;
    int winX = 0;
    int winY = 0;
    
    if (g_bFullScreen == FALSE) {
        // Center window on desktop
        winX = ((desktopRect.right - desktopRect.left) - winWidth) / 2;
        winY = ((desktopRect.bottom - desktopRect.top) - winHeight) / 2;
    }
    
    // Register window class
    WNDCLASSA wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = g_hInstance;
    wc.hIcon = LoadIconA(g_hInstance, MAKEINTRESOURCEA(103));
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "RESIDENT EVIL";
    
    RegisterClassA(&wc);
    
    // Create window
    g_hWnd = CreateWindowExA(dwExStyle,
        "RESIDENT EVIL",
        "RESIDENT EVIL",
        dwStyle,
        winX, winY, winWidth, winHeight,
        NULL, NULL, g_hInstance, NULL);
    
    if (g_hWnd == NULL) {
        return FALSE;
    }
    
    ShowWindow(g_hWnd, nCmdShow);
    
    // Disable IME
    ImmAssociateContext(g_hWnd, NULL);
    
    return TRUE;
}

// ============================================================================
// RunMessageLoop - Main message pump with integrated game loop
// 0x00441d71 - end
// ============================================================================
// 0x004d46d8 - frame-limiter enable. Read once, at 0x00441f0e, and written
// nowhere in the image: the limiter is unconditionally armed.
static const BOOL kFrameLimiterEnabled = TRUE;

// 0x004bcb54 - gameplay frame interval in ms. Read once, at 0x00441f26, and
// written nowhere; 0x21 in the image. 33 ms is 30 ticks/s, the rate the play
// clock (0x1A5E0 == 3600*30) and every scripted frame count assume.
static const int kFrameIntervalMs = 33;

int RunMessageLoop(void)
{
    MSG msg = {};
    int exitCode = 0;

    // The 33 ms limiter below compares timeGetTime values, and the original was
    // written against the ~1 ms timeGetTime of Win9x. On modern Windows the
    // default multimedia-timer period is 15.625 ms unless some process raises
    // it, which quantises "wake at last + 33" up to last + 46.9 - a 21 Hz tick.
    // Both observed runs of the room 10F probe showed this: the first reported
    // only multiples of 15.625 ms, the second (another process had raised the
    // resolution) sub-millisecond values. Ask for 1 ms so the limiter behaves
    // the way its constant assumes. Not in the original - it had no need.
    timeBeginPeriod(1);
    
    // 0x00441d71: Main loop start
    for (;;) {
        DWORD peekFlags = PM_NOREMOVE;
        // Check for pending messages
        if (PeekMessageA(&msg, NULL, 0, 0, peekFlags)) {
            // 0x00441d87: Get the message
            BOOL gotMsg = GetMessageA(&msg, NULL, 0, 0);
                if (!gotMsg) {
                // 0x00441da0: WM_QUIT received - cleanup and exit
                DestroyAllSoundBanks();
                CleanupAsyncTasks();

                if (g_bHasFinalizedSettings) {
                    if (!g_isGameCursorHiddenFlag) {
                    ShowCursor(TRUE);
                }
                    // Show error message if applicable
                    ShowMessageBox(NULL, "", "RESIDENT EVIL", MB_OK);
                }
                
                if (g_bAccessibilityAnimations) {
                    SystemParametersInfoA(SPI_SETANIMATION, 0, (PVOID)TRUE, SPIF_SENDCHANGE);
                }
                
                CleanupVideoConfigAndSaveAllSettings();
                CloseHandle(g_hMutex);
                CleanupSharedMemory();
                timeEndPeriod(1);
                return (int)msg.wParam;
            }
            
            // 0x00441dc2: Translate and dispatch
            TranslateMessage(&msg);
            
            // 0x00441dca: Handle WM_CLOSE (SC_CLOSE)
            if (msg.message == WM_SYSCOMMAND && msg.wParam == SC_CLOSE) {
                CleanupVideoConfigAndSaveAllSettings();
            }
            
            DispatchMessageA(&msg);
        }
        
        // 0x00441dd6: Check if window was destroyed (DAT_004d4604)
        if (g_hWnd == NULL) {
            CleanupVideoConfigAndSaveAllSettings();
            break;
        }
        
        // 0x00441de0: Check software rendering shared memory signals
        if (g_bIsSoftwareRendering && g_pSharedMemory != NULL) {
            if (g_pSharedMemory[3] == 0x01 && g_pSharedMemory[1] == 0x00) {
                CleanupVideoConfigAndSaveAllSettings();
                DestroyWindow(g_hWnd);
                g_hWnd = NULL;
                break;
            }
        }
        
        // 0x00441dfe: MCI video playback state machine
        if (g_bMCINotifyEnabled) {
            if (g_bMCINotifyFlag) {
                UpdateVideoPlayback();
                continue;
            }
            g_bMCINotifyFlag = TRUE;
        }
        
        // 0x00441e2b: Update game status in shared memory
        UpdateGameStatus();
        
        // 0x00441e35: Check graphics system is ready
        if (!IsGraphicsSystemReadyForOperation()) {
            OutputDebugStringA("[MAIN LOOP] IsGraphicsSystemReadyForOperation returned FALSE\n");
            if (!g_displayReturnToTitleScreen_Flag) {
                CleanupVideoConfigAndSaveAllSettings();
                DestroyWindow(g_hWnd);
                g_hWnd = NULL;
            }
            continue;
        }
        
        // 0x00441e50: Frame timing and game loop execution
        // 0x00441e9d/0x00441eb1: the original reads DAT_004bcb2c (window focused)
        // here, not the SideWinder pause flag.
        //
        // The original gate, instruction for instruction:
        //   0x00441e91 CMP g_bQuitFlag,      0 / JNZ loop_top
        //   0x00441e9d CMP g_bWindowFocused, 0 / JZ  loop_top   <- focus-loss pause
        //   0x00441ea9 CMP g_hWnd,           0 / JZ  0x00441eb9
        //   0x00441eb1 CMP g_bWindowFocused, 0 / JNZ run
        //   0x00441eb9 CMP g_bWindowActive,  0 / JZ  0x00441f88 (g_bWindowActive = 0)
        // Focus is already known non-zero at 0x00441eb1, so the second read always
        // takes the JNZ and g_bWindowActive only matters when g_hWnd is NULL.
        // Losing focus therefore stops main_loop() being called at all - that is
        // the pause. The port previously ORed the two flags together with an
        // always-TRUE g_bWindowActive, so it never paused.
        if (!g_bQuitFlag && g_bWindowFocused) {
            if (g_hWnd != NULL || g_bWindowActive) {
                DWORD currentTime = timeGetTime();
                
                // 0x00441e70: Frame rate counter every second
                if ((int)(g_dwSystemTimer1 + 1000) < (int)currentTime) {
                    // Update frame counters
                    g_numFramesRendered = g_numFramesRendered;
                    g_numFramesRendered = g_numFramesRendered;
                    g_numFramesPresented = g_numFramesPresented;
                    g_numFramesPresented = g_numFramesPresented;
                    g_dwSystemTimer1 = currentTime;
                }
                
                // 0x00441f0c: frame pacing - this is what pins the game to 30
                // ticks per second. Instruction for instruction:
                //
                //   0x00441f0c  CMP DAT_004d46d8, 0     / JZ  run   <- limiter gate
                //   0x00441f14  CMP g_bUseFrameSkip, 1  / JNZ 16ms path
                //   0x00441f1d  CMP g_bFrameSkipDetected, 0 / JNZ run
                //   0x00441f25  pace to g_dwFrameIntervalMs ms  (gameplay)
                //   0x00441f4f  pace to 16 ms            (menus, title, FMV)
                //
                // Both never-written gates matter. DAT_004d46d8 is read only at
                // 0x00441f0e and is 1 in the image, so the limiter is always armed;
                // g_dwFrameIntervalMs is read only at 0x00441f26 and is 0x21 = 33 - the 30
                // ticks/s the whole game is built on (the play clock divides by 30,
                // 0x1A5E0 == 3600*30, and every scripted wait is a frame count).
                //
                // The port did NO limiting on the gameplay branch, so the tick rate
                // was whatever the vsync'd Present allowed: measured 627 event-VM
                // ticks in 18937 ms = 33.1 ticks/s, ~10% fast, and it would reach 60
                // on a machine that renders every frame inside one vblank. Room 10F's
                // piano performance is the visible symptom - its 627 frames of
                // keypresses finished ~2 s before bgm_2b.wav did.
                //
                // g_bFrameSkipDetected is the escape hatch, not a second enable:
                // once the governor decides the machine cannot hold the target it
                // stops pacing here and drops presented frames instead.
                if (kFrameLimiterEnabled) {
                    BOOL bSkipFrame = FALSE;

                    if (g_bUseFrameSkip == 1) {
                        if (!g_bFrameSkipDetected) {
                            if ((int)(kFrameIntervalMs + g_dwGameTimer1) > (int)currentTime &&
                                (int)(g_dwGameTimer1 - 0x10000) < (int)currentTime) {
                                bSkipFrame = TRUE;
                            }
                        }
                    } else {
                        if ((int)(g_dwGameTimer1 + 16) > (int)currentTime &&
                            (int)(g_dwGameTimer1 - 0x10000) < (int)currentTime) {
                            bSkipFrame = TRUE;
                        }
                    }

                    if (bSkipFrame) {
                        continue;
                    }
                }

                // 0x00441f75: Execute game loop
                g_dwGameTimer1 = currentTime;
                int loopResult = main_loop();
                
                if (loopResult == 0) {
                    // Game loop requested exit
                    if (g_bAccessibilityAnimations) {
                        SystemParametersInfoA(SPI_SETANIMATION, 0, (PVOID)TRUE, SPIF_SENDCHANGE);
                    }
                    CloseHandle(g_hMutex);
                    return (int)msg.wParam;
                }
            }
            else {
                // 0x00441f88: mov [g_bWindowActive], ebx (ebx == 0), then jmp
                // back to the top of the pump.
                g_bWindowActive = FALSE;
            }
        }
    }
    
    return exitCode;
}
