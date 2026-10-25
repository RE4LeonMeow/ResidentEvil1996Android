// SPDX-License-Identifier: GPL-3.0-or-later
// Android port presentation policy. Reads existing engine state only.
// Projection adapted from FusionFix v1.1.0 source/RE1/Widescreen.ixx,
// blob 21d68a3ae1e5f09f60c799c32d31e1370858d468, FusionFix contributors.
#pragma once
#include "../../Globals.h"
#include "../../game/MenuPresentation.h"
#include <cmath>

inline bool WebTaskActive(int slot, void* entry)
{
    // Dead tasks retain their entry pointers. A historical menu pointer or
    // request flag must not permanently suppress gameplay features.
    return g_TasksTable[slot].state != 0 && g_TasksEIP[slot] == entry;
}

inline bool WebNativeMenuActive()
{
    // The typewriter runs its slot picker on game_start's task. Require its
    // live state as well as the flag: the flag alone can outlive a title load.
    return WebTaskActive(1, (void*)main_menu) || WebTaskActive(1, (void*)options_menu) ||
        (g_typewriter_state == 3 && g_loadSaveStateFlag);
}

inline bool WebItemViewActive()
{
    const unsigned itemModes=MSF_MENU_MODE_ITEM_VIEW | MSF_MENU_MODE_GOT_ITEM | MSF_MENU_MODE_KEY_DEPLETED;
    return WebTaskActive(1, (void*)main_menu) && (g_main_state_flags & itemModes);
}

inline bool WebGameplayActive()
{
    return g_playingGameFlag && WebTaskActive(0, (void*)game_start) &&
        !WebNativeMenuActive() && !g_displayExitGameScreen_flag;
}

inline bool WebDoorActive()
{
    return g_roomTransitionBusy || (g_main_state_flags & (MSF_DOOR_TRANSITION | MSF_ROOM_TRANSITION));
}

inline bool WebTraversalActive()
{
    // Normal player control's ladder/stair interaction family, selected by
    // player_input_to_behavior in PlayerAnimations.cpp (0x004956a0). These
    // motions temporarily lock input/camera, but are not scripted state 8.
    const unsigned behavior=g_playerEntity.action_behavior;
    return g_playerEntity.animationId==1 && g_playerEntity.animFrameId<=2 &&
        (behavior==0x0b || behavior==0x11) &&
        (g_playerEntity.isBeingAttackedFlag&0x80) &&
        !(g_playerEntity.isBeingAttackedFlag&0x3f) && g_playerEntity.health>0;
}

inline bool WebInteractionMessageActive()
{
    // pause_game can clear actor/input bits while a room prompt is visible.
    // Its saved mask distinguishes an interaction from scripted dialogue.
    return (g_menu_choice_id & 0x80) && (g_messageFlagsBackup & 0x101) == 0x101 &&
        !(g_main_state_flags & MSF_CAMERA_LOCK);
}

inline bool WebTouchMenuActive()
{
    // Touch navigation is independent of aspect ratio: a movie, a bite or
    // scripted movement is not an inventory/title/options menu.
    if (WebGameplayActive() && WebDoorActive()) return false;
    return !WebGameplayActive() || g_openMenuFlag != 0 || WebInteractionMessageActive();
}

// 0 = full 4:3; 1 = player; 2 = centered door; 3 = held world crop + reflowed text;
// 4 = hold framing; 5 = inventory/pickup; 6 = item box; 7 = map/files.
inline int WebPresentationMode()
{
    // Preserve movies and scripted scenes, including dialogue, as one complete
    // frame. Switching between a centered crop and full UI for each subtitle
    // made the mansion introduction repeatedly change size and position.
    if (g_bMCINotifyEnabled || !g_playingGameFlag ||
        !WebTaskActive(0, (void*)game_start) || g_displayExitGameScreen_flag ||
        (g_typewriter_state == 3 && g_loadSaveStateFlag) ||
        WebTaskActive(1, (void*)options_menu) || (g_main_state_flags & MSF_PLAYER_DEAD)) return 0;

    if (WebTaskActive(1, (void*)main_menu)) {
        if(MenuMapFilesLayout())return 7;
        if(MenuUsesItemBoxLayout())return 6;
        return MenuUsesInventoryLayout() ? 5 : 0;
    }
    // The player's position/camera may already belong to the destination while
    // the separate door task renders. Never use player tracking for that view.
    if (g_roomTransitionBusy || (g_main_state_flags & MSF_ROOM_TRANSITION)) return 2;
    // The live lid animation temporarily masks the message/input bits. Hold
    // the world framing while the box opens, settles and returns to play.
    if(g_itembox_state>=1&&g_itembox_state<=4)return 3;
    if(WebTraversalActive() && (g_message_flags&1) && !(g_menu_choice_id&0x80) && !g_openMenuFlag)return 1;
    if (g_main_state_flags & MSF_CAMERA_LOCK) return 0;
    if (g_menu_choice_id & 0x80) return WebInteractionMessageActive() ? 3 : 0;
    if (g_main_state_flags & MSF_DOOR_TRANSITION) return 1;
    if (g_openMenuFlag) return 4;

    // Bit 0 runs the actor state machine; it does not establish player control.
    // game_loop masks directional input when bit 0x100 is clear. Scripts can
    // also leave animationId == 1 while locking the camera (cmd_cut_lock_set).
    // zombie_attack sets animationId=5 and isBeingAttackedFlag=1 for a bite;
    // other grabs use windows 6/7. Retain combat framing even if directional
    // input is temporarily disabled, without treating scripted windows as play.
    const unsigned animation=g_playerEntity.animationId;
    const bool grabbed=(g_playerEntity.isBeingAttackedFlag & 0x3f) != 0 &&
        g_playerEntity.health >= 0 &&
        (animation == 2 || (animation >= 5 && animation <= 7));
    if (grabbed && (g_message_flags & 1)) return 1;
    if ((g_message_flags & 0x101) != 0x101) return 0;

    // Keep ordinary combat damage in the same framing as movement. Scripted
    // animation windows and blocked-input states retain the complete image.
    return g_playerEntity.animationId == 1 || g_playerEntity.animationId == 2 ? 1 : 0;
}

inline float WebPlayerScreenY()
{
    // Adapted from FusionFix v1.1.0 RE1/Widescreen.ixx PlayerY (GPL-3.0-or-later).
    // Its point is the actor's torso, 800 world units above the feet. Apply
    // the same camera/Y conventions as ApplyMatrixLV, without mutating GTE.
    const double point[3] = { double(g_playerEntity.scaMatrixData.localMatrix.t[0]),
        800.0 - g_playerEntity.scaMatrixData.localMatrix.t[1],
        double(g_playerEntity.scaMatrixData.localMatrix.t[2]) };
    double view[3];
    for (int row=0; row<3; ++row) {
        view[row]=g_RoomCameraData.t[row];
        for (int col=0; col<3; ++col)
            view[row]+=g_RoomCameraData.m[row][col]*point[col]/4096.0*(row==1?-1.0:1.0);
    }
    const double y=view[2]>1.0?120.0+view[1]*g_sceneRenderParam/view[2]:120.0;
    return std::isfinite(y)?float(y):120.0f;
}
