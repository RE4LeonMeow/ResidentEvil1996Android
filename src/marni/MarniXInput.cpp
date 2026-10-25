// MarniXInput.cpp - XInput gamepad backend (port addition)
// See MarniXInput.h for the mask contract this file has to honour.

#include "MarniXInput.h"
#include <xinput.h>
#include <cstdio>

#pragma comment(lib, "xinput.lib")

namespace MarniXInput {

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static bool  s_enabled     = true;
static int   s_deadzone    = MARNI_XI_DEFAULT_DEADZONE;

// Slot currently owning the pad, or -1 when nothing is connected.
static int   s_activeSlot  = -1;

// Throttle for the "is anything plugged in yet?" rescan. XInputGetState on an
// empty slot is measurably slow on some runtimes, and there are four of them;
// doing that every frame would eat into the 33 ms tick. Once we know we have
// no pad we only look again once a second.
static DWORD s_nextScanTick = 0;
#define MARNI_XI_RESCAN_MS 1000

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
void SetEnabled(bool enabled)
{
    s_enabled = enabled;
    if (!enabled) {
        s_activeSlot = -1;
    }
}

bool IsEnabled()
{
    return s_enabled;
}

void SetDeadzone(int deadzone)
{
    // Clamp to something that always leaves usable travel.
    if (deadzone < 0)     deadzone = 0;
    if (deadzone > 30000) deadzone = 30000;
    s_deadzone = deadzone;
}

bool IsConnected()
{
    return s_activeSlot >= 0;
}

// ---------------------------------------------------------------------------
// FindPad - scan all four slots, latch the first that answers.
// ---------------------------------------------------------------------------
static int FindPad(void)
{
    XINPUT_STATE state;
    for (int slot = 0; slot < XUSER_MAX_COUNT; slot++) {
        ZeroMemory(&state, sizeof(state));
        if (XInputGetState(slot, &state) == ERROR_SUCCESS) {
            return slot;
        }
    }
    return -1;
}

void Init()
{
    s_activeSlot   = -1;
    s_nextScanTick = 0;

    if (!s_enabled) {
        return;
    }

    s_activeSlot = FindPad();

    char dbg[128];
    if (s_activeSlot >= 0) {
        sprintf_s(dbg, "[Marni] XInput: pad on slot %d\n", s_activeSlot);
    } else {
        sprintf_s(dbg, "[Marni] XInput: no pad connected\n");
        s_nextScanTick = GetTickCount() + MARNI_XI_RESCAN_MS;
    }
    OutputDebugStringA(dbg);
}

// ---------------------------------------------------------------------------
// BuildMask - XINPUT_GAMEPAD to the Marni joystick bitmask.
// ---------------------------------------------------------------------------
static DWORD BuildMask(const XINPUT_GAMEPAD* pad)
{
    DWORD mask = 0;

    // --- Directions: D-pad and left stick both drive the AXIS bits (0-3) ---
    // (bits 4-7, the POV hat, are deliberately left clear - g_JoyRemapTbl[1]
    //  maps them to nothing, so input placed there would vanish.)
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)    mask |= 1;
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  mask |= 2;
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  mask |= 4;
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) mask |= 8;

    if (pad->sThumbLY >  s_deadzone) mask |= 1;   // UP
    if (pad->sThumbLY < -s_deadzone) mask |= 2;   // DOWN
    if (pad->sThumbLX < -s_deadzone) mask |= 4;   // LEFT
    if (pad->sThumbLX >  s_deadzone) mask |= 8;   // RIGHT

    // Opposite directions cannot be held at once (a D-pad press plus a stick
    // deflection the other way would otherwise produce both). UP beats DOWN
    // and LEFT beats RIGHT, matching PlayerPad_Update's own dpad post-pass.
    if (mask & 1) mask &= ~2u;
    if (mask & 4) mask &= ~8u;

    // --- Buttons: pad buttons 1-12 land in bits 8-19 ---
    if (pad->wButtons & XINPUT_GAMEPAD_A)              mask |= MARNI_XI_BTN_A;
    if (pad->wButtons & XINPUT_GAMEPAD_B)              mask |= MARNI_XI_BTN_B;
    if (pad->wButtons & XINPUT_GAMEPAD_X)              mask |= MARNI_XI_BTN_X;
    if (pad->wButtons & XINPUT_GAMEPAD_Y)              mask |= MARNI_XI_BTN_Y;
    if (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)  mask |= MARNI_XI_BTN_LB;
    if (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) mask |= MARNI_XI_BTN_RB;
    if (pad->wButtons & XINPUT_GAMEPAD_BACK)           mask |= MARNI_XI_BTN_BACK;
    if (pad->wButtons & XINPUT_GAMEPAD_START)          mask |= MARNI_XI_BTN_START;
    if (pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB)     mask |= MARNI_XI_BTN_LTHUMB;
    if (pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)    mask |= MARNI_XI_BTN_RTHUMB;

    // Triggers are analogue; the game has no analogue consumer, so report
    // them as digital buttons past the standard threshold.
    if (pad->bLeftTrigger  > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) mask |= MARNI_XI_BTN_LTRIGGER;
    if (pad->bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) mask |= MARNI_XI_BTN_RTRIGGER;

    return mask;
}

// ---------------------------------------------------------------------------
// Poll - called once per frame from UpdateAllInputStates.
// ---------------------------------------------------------------------------
DWORD Poll()
{
    if (!s_enabled) {
        return 0;
    }

    XINPUT_STATE state;

    if (s_activeSlot >= 0) {
        ZeroMemory(&state, sizeof(state));
        if (XInputGetState(s_activeSlot, &state) == ERROR_SUCCESS) {
            return BuildMask(&state.Gamepad);
        }
        // Unplugged mid-session: drop back to throttled rescanning.
        OutputDebugStringA("[Marni] XInput: pad disconnected\n");
        s_activeSlot   = -1;
        s_nextScanTick = GetTickCount() + MARNI_XI_RESCAN_MS;
        return 0;
    }

    // No pad. Hot-plug detection, but only once a second.
    DWORD now = GetTickCount();
    if ((int)(now - s_nextScanTick) < 0) {
        return 0;
    }
    s_nextScanTick = now + MARNI_XI_RESCAN_MS;

    s_activeSlot = FindPad();
    if (s_activeSlot < 0) {
        return 0;
    }

    char dbg[128];
    sprintf_s(dbg, "[Marni] XInput: pad connected on slot %d\n", s_activeSlot);
    OutputDebugStringA(dbg);

    ZeroMemory(&state, sizeof(state));
    if (XInputGetState(s_activeSlot, &state) != ERROR_SUCCESS) {
        s_activeSlot = -1;
        return 0;
    }
    return BuildMask(&state.Gamepad);
}

} // namespace MarniXInput
