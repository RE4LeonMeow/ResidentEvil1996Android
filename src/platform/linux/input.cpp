// input.cpp - Linux input backend (Phase 5).
//
// Counterpart of src/marni/MarniInput.cpp + MarniXInput.cpp. Reproduces the
// mask contract exactly (docs/GAMEPAD_INPUT.md): a 32-bit word where
//   bits 0-3  = axis directions (up, down, left, right)
//   bits 4-7  = POV hat - deliberately unused, g_JoyRemapTbl[1] zeroes them
//   bits 8-19 = buttons (MARNI_XI_BTN_*)
// published into joysticks[0] with the same prev/curr/newPress transitions as
// the WinMM path, so nothing downstream can tell the backends apart.
//
// There is no WinMM on Linux, so the device sweep is SDL's game-controller API
// instead of joyGetPosEx; the bit assignments are identical.
#include "Globals.h"
#include "platform/platform.h"
#include "marni/MarniInput.h"
#include "marni/MarniXInput.h"

#include <SDL2/SDL.h>

// ---------------------------------------------------------------------------
// Pad state
// ---------------------------------------------------------------------------

#ifdef __EMSCRIPTEN__
extern "C" DWORD re1_web_pad_mask();
#endif
bool MarniPadIsConnected(void);
static SDL_GameController* s_pad = NULL;
static Uint32 s_nextRescan = 0;

static const int kDeadzone = 10000;                 // MARNI_XI_DEFAULT_DEADZONE
static const int kTriggerThreshold = 3855;          // XInput's 30/255, scaled

static void TryOpenPad(void)
{
#ifdef __EMSCRIPTEN__
    return; // The Android/Web bridge owns mapping; do not poll a second SDL pad.
#endif
    if (s_pad != NULL && !SDL_GameControllerGetAttached(s_pad)) {
        SDL_GameControllerClose(s_pad);
        s_pad = NULL;
        s_nextRescan = 0;
    }
    if (s_pad != NULL) return;

    // Throttled rescan, like the Windows backend's one-second throttle, so a
    // pad plugged in mid-session is picked up without polling every frame.
    Uint32 now = SDL_GetTicks();
    if (now < s_nextRescan) return;
    s_nextRescan = now + 1000;

    int n = SDL_NumJoysticks();
    for (int i = 0; i < n; ++i) {
        if (SDL_IsGameController(i)) {
            s_pad = SDL_GameControllerOpen(i);
            if (s_pad != NULL) {
                fprintf(stderr, "[PAD] opened: %s\n", SDL_GameControllerName(s_pad));
                break;
            }
        }
    }
}

// SDL controller -> Marni mask. Mirrors MarniXInput::BuildMask bit for bit.
static DWORD BuildPadMask(void)
{
#ifdef __EMSCRIPTEN__
    return 0;
#endif
    if (s_pad == NULL) return 0;

    DWORD mask = 0;

    Sint16 lx = SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_LEFTX);
    Sint16 ly = SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_LEFTY);

    // D-pad and left stick both drive the AXIS bits. SDL's Y axis is positive
    // downwards, the opposite of XInput's sThumbLY, hence the sign flip.
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_UP)    || ly < -kDeadzone) mask |= 1;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)  || ly >  kDeadzone) mask |= 2;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)  || lx < -kDeadzone) mask |= 4;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || lx >  kDeadzone) mask |= 8;

    // Opposite directions cannot be held at once; UP beats DOWN, LEFT beats
    // RIGHT, matching PlayerPad_Update's own post-pass.
    if (mask & 1) mask &= ~2u;
    if (mask & 4) mask &= ~8u;

    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_A))             mask |= MARNI_XI_BTN_A;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_B))             mask |= MARNI_XI_BTN_B;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_X))             mask |= MARNI_XI_BTN_X;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_Y))             mask |= MARNI_XI_BTN_Y;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))  mask |= MARNI_XI_BTN_LB;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) mask |= MARNI_XI_BTN_RB;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_BACK))          mask |= MARNI_XI_BTN_BACK;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_START))         mask |= MARNI_XI_BTN_START;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_LEFTSTICK))     mask |= MARNI_XI_BTN_LTHUMB;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_RIGHTSTICK))    mask |= MARNI_XI_BTN_RTHUMB;

    // Triggers are analogue; the game has no analogue consumer, so report them
    // as digital past the XInput threshold.
    if (SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT)  > kTriggerThreshold) mask |= MARNI_XI_BTN_LTRIGGER;
    if (SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > kTriggerThreshold) mask |= MARNI_XI_BTN_RTRIGGER;

    return mask;
}

// ---------------------------------------------------------------------------
// CMarniDirectInput
// ---------------------------------------------------------------------------

void CMarniDirectInput::UpdateKeyboardInputState(MasterInputState* pState)
{
    // 0x004202f0: zero keyboardCurr, then test all 32 mapped keys.
    pState->keyboardCurr = 0;
    for (int i = 0; i < 32; i++) {
        if (plat_key_state(pState->keyMap[i]) & 0x8000) {
            pState->keyboardCurr |= (1u << i);
        }
    }
}

void CMarniDirectInput::UpdateAllInputStates(MasterInputState* pState)
{
    // 0x0042057a-0x004206e7: keyboard state transitions.
    UpdateKeyboardInputState(pState);

    DWORD oldPrev = pState->keyboardPrev;
    pState->keyboardRepeat   = oldPrev;
    pState->keyboardPrev     = pState->keyboardCurr;
    pState->frameFlag        = 1;
    pState->keyboardNewPress = (~oldPrev) & pState->keyboardCurr;

    // --- Pad publishes into joysticks[0] ---
    TryOpenPad();

    JoystickEntry* pJoy = &pState->joysticks[0];
    DWORD padMask = BuildPadMask();
#ifdef __EMSCRIPTEN__
    padMask |= re1_web_pad_mask();
#endif

    if (padMask != 0 || MarniPadIsConnected()) {
        pJoy->enabled   = 1;
        pJoy->prevPress = pJoy->currPress;
        pJoy->currPress = padMask;
        pJoy->newPress  = (~pJoy->prevPress) & padMask;
    } else {
        // No pad: release the slot and clear any latched press state.
        pJoy->enabled   = 0;
        pJoy->prevPress = 0;
        pJoy->currPress = 0;
        pJoy->newPress  = 0;
    }
}

void CMarniDirectInput::InitJoysticks(MasterInputState* pState)
{
#ifndef __EMSCRIPTEN__
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
#endif
    TryOpenPad();
    pState->joystickCount = MarniPadIsConnected() ? 1u : 0u;
}

// ---------------------------------------------------------------------------
// Capability queries used by InputUpdate
// ---------------------------------------------------------------------------

bool MarniPadIsConnected(void)
{
#ifdef __EMSCRIPTEN__
    return true; // The touch/controller bridge is always attached.
#else
    return s_pad != NULL;
#endif
}

namespace MarniXInput {
bool IsConnected() { return MarniPadIsConnected(); }
}
