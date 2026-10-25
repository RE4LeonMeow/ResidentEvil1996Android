// MarniXInput.h - XInput (XBox 360 / XBox One / generic PC gamepad) backend
//
// Port addition. Not present in the original 1997 binary, which only spoke
// WinMM joystick (joyGetPosEx) and Microsoft SideWinder.
//
// This backend does NOT introduce a new input path: it synthesises the exact
// bitmask layout that CMarniDirectInput::UpdateAllInputStates already builds
// from JOYINFOEX, so everything downstream (JoyToPSX, g_JoyRemapTbl[1], the
// options-menu SideWinder binding editor and the save-file remap tables)
// keeps working unmodified.
//
// Mask layout (see UpdateAllInputStates, 0x00420570):
//   bit 0   UP      | bit 1  DOWN   | bit 2  LEFT  | bit 3  RIGHT  (axis)
//   bit 4   UP      | bit 5  DOWN   | bit 6  LEFT  | bit 7  RIGHT  (POV hat)
//   bit 8+  buttons (WinMM dwButtons << 8)
//
// The D-pad is folded into the AXIS bits (0-3), not the POV bits, because
// g_JoyRemapTbl[1][4..7] are zero and the options DEFAULT restore never
// writes them - anything landing in the hat bits is silently dropped.
//
// Buttons occupy bits 8-19, so pad buttons 1-8 (bits 8-15) are exactly the
// eight rows the SideWinder config screen rebinds.
#pragma once
#include "../platform/types.h"

// Left-stick deflection (0..32767) past which a direction bit is raised.
// XInput's own XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE (7849) is tuned for free
// analogue aiming; this game only ever consumes four digital directions, so
// a slightly larger gate keeps diagonals from flickering during tank turns.
#define MARNI_XI_DEFAULT_DEADZONE 10000

// Left-stick deflection (0..32767) past which a direction bit is raised.
// XInput's own XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE (7849) is tuned for free
// analogue aiming; this game only ever consumes four digital directions, so
// a slightly larger gate keeps diagonals from flickering during tank turns.
// Button bit assignments within the Marni mask.
#define MARNI_XI_BTN_A          0x00000100u   // pad button 1
#define MARNI_XI_BTN_B          0x00000200u   // pad button 2
#define MARNI_XI_BTN_X          0x00000400u   // pad button 3
#define MARNI_XI_BTN_Y          0x00000800u   // pad button 4
#define MARNI_XI_BTN_LB         0x00001000u   // pad button 5
#define MARNI_XI_BTN_RB         0x00002000u   // pad button 6
#define MARNI_XI_BTN_BACK       0x00004000u   // pad button 7
#define MARNI_XI_BTN_START      0x00008000u   // pad button 8
#define MARNI_XI_BTN_LTHUMB     0x00010000u   // pad button 9
#define MARNI_XI_BTN_RTHUMB     0x00020000u   // pad button 10
#define MARNI_XI_BTN_LTRIGGER   0x00040000u   // pad button 11 (digital)
#define MARNI_XI_BTN_RTRIGGER   0x00080000u   // pad button 12 (digital)

namespace MarniXInput {

// Enable/disable the backend (config.ini [Input] EnableXInput). Enabled by
// default; when disabled Poll() always reports "no pad" and the WinMM path
// owns joystick slot 0 exactly as before.
void SetEnabled(bool enabled);
bool IsEnabled();

// Left-stick deflection, 0..32767, past which a direction bit is raised.
// Default MARNI_XI_DEFAULT_DEADZONE.
void SetDeadzone(int deadzone);

// Scan every slot once and latch the first connected pad. Cheap to call.
void Init();

// True if the last Poll()/Init() found a pad. Does not itself poll.
bool IsConnected();

// Poll the active pad and return the Marni-format mask (0 when no pad).
// Slots that are known-disconnected are only rescanned once a second, so a
// controller-less machine never pays the full four-slot XInputGetState cost
// inside the 33 ms frame budget.
DWORD Poll();

} // namespace MarniXInput
