// stubs.cpp - placeholders for the few Windows-only entry points the
// shared code still references.
//
// Every phase's subsystem now has a real Linux implementation:
//   input  -> input.cpp (P5)
//   audio  -> audio.cpp (P6)
//   FMV    -> video.cpp + src/video/VideoPlayback.cpp (P7)
//   misc   -> config.cpp / crash.cpp (P8)
// What is left here is the message box and the two stubs the shared code calls
// but that have no Linux meaning.
#include <SDL2/SDL.h>

#include "Globals.h"
#include "marni/MarniInput.h"
#include "marni/MarniSound.h"
#include "marni/MarniSystem.h"
#include "marni/MarniXInput.h"

int ShowMessageBox(HWND hWnd, LPCSTR lpMsg, LPCSTR lpCaption, UINT uType)
{
    (void)hWnd;
    Uint32 flags = (uType & MB_ICONSTOP) ? SDL_MESSAGEBOX_ERROR
                                         : SDL_MESSAGEBOX_INFORMATION;
    SDL_ShowSimpleMessageBox(flags, lpCaption ? lpCaption : "RESIDENT EVIL",
                             lpMsg ? lpMsg : "", NULL);
    return 1;   // IDOK
}
