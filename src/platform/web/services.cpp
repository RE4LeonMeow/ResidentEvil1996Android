// Port-added services for the Android WebView's wasm32 runtime.
#include "../platform.h"
#include "../../Globals.h"
#include "../../system/ConfigFile.h"
#include <emscripten.h>
#include <emscripten/heap.h>
#include <stdio.h>
#include "presentation.h"
#include "../../game/PortCheats.h"
#include "../../marni/MarniSystem.h"

// Port-owned session state, deliberately outside BioCardLayout/save data.
static unsigned s_cheats = 0;
static bool s_cheatsAgreed = false;
static int s_cheatRequest = 0;
static bool s_smoothFrames = false; // port setting; not saved in the game card

static bool CheatGameplay()
{
    return WebGameplayActive() && !g_bMCINotifyEnabled && !WebDoorActive() &&
        !(g_main_state_flags & (MSF_CAMERA_LOCK | MSF_PLAYER_DEAD | MSF_FMV_REQUEST)) &&
        !(g_main_state_flags2 & MSF2_ATTRACT_DEMO);
}

static bool CheatActionAllowed()
{
    return CheatGameplay() && !g_openMenuFlag && !(g_menu_choice_id & 0x80) &&
        !g_typewriter_state && (g_message_flags & 0x141) == 0x141 &&
        g_playerEntity.health > 0 && !(g_playerEntity.isBeingAttackedFlag & 0x3f) &&
        g_playerEntity.animationId == 1 && g_playerEntity.animFrameId <= 2;
}

static void CheatHeal()
{
    // The existing entity maxHealth also accounts for character/difficulty.
    if (g_playerEntity.maxHealth > 0) {
        g_playerEntity.health = g_playerEntity.maxHealth;
        g_PlayerHealthCopy = g_playerEntity.health;
    }
}

static void CheatRefill()
{
    // Only touch weapons already carried, never keys, puzzle items or ribbons.
    ItemSlot* slots = static_cast<ItemSlot*>(g_ItemSlotsPointer);
    if (slots != g_ItemsSlots && slots != g_RebeccaItemSlots) return;
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned id = slots[i].Id;
        if (id >= ITEM_BERETTA && id <= ITEM_ROCKET_LAUNCHER) {
            const unsigned capacity = g_ItemMaxQty[id * 4];
            if (capacity) slots[i].qty = static_cast<unsigned char>(capacity |
                (id == ITEM_FLAMETHROWER ? 0 : slots[i].qty & 0x80));
        }
    }
}

void PortCheatsMaintain()
{
    if (!s_cheats || !CheatGameplay()) return;
    // Refill around actor updates rather than changing damage scripts or
    // permanently granting an unlock flag. Scripted instant deaths can still
    // occur; this is HP maintenance, not a resurrection/state repair feature.
    if (s_cheats & 1) CheatHeal();
    if (s_cheats & 2) CheatRefill();
}

void PortCheatsProcessRequests()
{
    if (!s_cheatRequest || !g_bWindowFocused) return;
    const int request = s_cheatRequest;
    s_cheatRequest = 0;
    // Recheck after the settings dialog closes and the game resumes.
    if (!CheatActionAllowed()) return;
    if (request == 6) {
        // Join the existing typewriter flow at its confirmed-choice state.
        // RoomEvents.cpp 0x0041c330 then fades out, opens LoadSaveGameState,
        // and restores the room. -1 + 1 passes useInkRibbon=0. No fabricated
        // room-action entry, scheduler switch from JS, or save-file writer.
        g_typewriter_id = ~0u;
        g_typewriter_state = 2;
        g_menu_choice_id = 0;
        g_message_flags &= 0xffba;
    } else if (request == 7) {
        g_debugOpenItemboxFlag = 1; // existing guarded game_loop item-box path
    }
}
size_t re1_web_memory_size() { return emscripten_get_heap_size(); }
EM_JS(unsigned int, re1_web_pad_mask, (), {
    return globalThis.RE1Input ? globalThis.RE1Input.poll() : 0;
});
EM_JS(void, re1_web_audio_suspend, (int paused), {
    const audio = Module['SDL2'] && Module['SDL2'].audioContext;
    if (audio && audio.state !== 'closed') {
        const promise = paused ? audio.suspend() : audio.resume();
        promise.catch(error => globalThis.RE1Diag?.log('Audio: '+error));
    }
});
BOOL plat_single_instance_check() { return TRUE; }
void crashlog_install() {}
void crashlog_mark(const char* step) { fprintf(stderr, "[RE1] %s\n", step); }
void CleanupVideoConfigAndSaveAllSettings()
{
    if (!g_bHasFinalizedSettings) {
        g_bHasFinalizedSettings = TRUE;
        ConfigFile_Save();
    }
}
extern "C" {
EMSCRIPTEN_KEEPALIVE void re1_web_set_smooth(int enabled) { s_smoothFrames=enabled!=0; }
// Only interpolate ordinary gameplay. Menus, doors, movies, scripted shots,
// camera cuts and any stalled/paused session retain the original frames.
unsigned re1_web_interpolation_scene()
{
    if (!s_smoothFrames || !g_bWindowFocused || !WebGameplayActive() || WebDoorActive() || WebTraversalActive() ||
        g_bMCINotifyEnabled || g_openMenuFlag || (g_menu_choice_id&0x80) ||
        (g_main_state_flags&MSF_MIRROR_ENABLE) ||
        WebPresentationMode()!=1 || g_bUseFrameSkip!=1 || g_bFrameSkipDetected) return 0;
    return 1u+(unsigned(g_stageId)<<16)+(unsigned(g_roomId)<<8)+g_roomCameraId;
}
int re1_web_interpolation_wait_ms()
{
    // Extra drawing/waiting spends the existing 33 ms tick budget. Never
    // change the simulation limiter or queue catch-up updates.
    const int remaining=int32_t(g_dwGameTimer1+33u-plat_time_ms());
    return s_smoothFrames && g_bWindowFocused && remaining>=24 ? 16 : 0;
}
// 0: status; 1: agree; 2/3: health/ammo toggle; 4/5: heal/refill;
// 6/7: queue save/item box; 8: disable all. Toggles last this runtime only.
EMSCRIPTEN_KEEPALIVE int re1_web_cheat(int action, int enabled)
{
    if (action == 0) return int(s_cheats) | (CheatActionAllowed() ? 256 : 0) |
        (s_cheatsAgreed ? 512 : 0);
    if (action == 1) { s_cheatsAgreed = true; return 1; }
    if (action == 8) { s_cheats = 0; s_cheatRequest = 0; return 1; }
    if (!s_cheatsAgreed) return 0;
    if (action == 2 || action == 3) {
        const unsigned bit = 1u << (action - 2);
        // Switching off must always work, even during a scene transition.
        if (enabled && !CheatActionAllowed()) return 0;
        s_cheats = enabled ? s_cheats | bit : s_cheats & ~bit;
        if (enabled) PortCheatsMaintain();
        return 1;
    }
    if (!CheatActionAllowed() || s_cheatRequest) return 0;
    if (action == 4) { CheatHeal(); return 1; }
    if (action == 5) { CheatRefill(); return 1; }
    if (action == 6 || action == 7) { s_cheatRequest = action; return 1; }
    return 0;
}
EMSCRIPTEN_KEEPALIVE void re1_web_suspend(int suspended)
{
    g_bWindowFocused = suspended ? FALSE : TRUE;
    re1_web_audio_suspend(suspended);
    if (suspended) ConfigFile_Save();
}
EMSCRIPTEN_KEEPALIVE void re1_web_request_quit() { g_bQuitFlag = TRUE; }
EMSCRIPTEN_KEEPALIVE void re1_web_settings_save() { ConfigFile_Save(); }
// Android port feature: an instant half-turn during ordinary player movement.
// Uses the existing player entity (0x00be62e4), not the legacy standalone
// g_playerAngle variable. Never modifies the packed layout or scripted states.
EMSCRIPTEN_KEEPALIVE int re1_web_quick_turn()
{
    const unsigned blocked = MSF_FMV_REQUEST |
        MSF_DOOR_TRANSITION | MSF_ROOM_TRANSITION | MSF_OBJECT_PUSH |
        MSF_PLAYER_DEAD;
    const unsigned behavior = g_playerEntity.action_behavior;
    if (!g_bWindowFocused || !WebGameplayActive() || g_bMCINotifyEnabled ||
        g_openMenuFlag || (g_menu_choice_id & 0x80) || (g_main_state_flags & blocked) ||
        (g_main_state_flags2 & MSF2_ATTRACT_DEMO) || (g_message_flags & 1) == 0 ||
        g_playerEntity.health <= 0 || (g_playerEntity.isBeingAttackedFlag & 0x3f) ||
        g_playerEntity.animationId != 1 || g_playerEntity.animFrameId > 2 ||
        !(behavior <= 8 || (behavior >= 0x0d && behavior <= 0x0f))) return 0;
    g_playerEntity.directionAngle = (g_playerEntity.directionAngle + 0x800) & 0xfff;
    return 1;
}
}

// Wait for queued IDBFS writes before Emscripten tears its runtime down.
EM_ASYNC_JS(void, re1_web_flush, (), {
    await new Promise((resolve, reject) => {
        function flush() {
            if (globalThis.re1SyncBusy) { setTimeout(flush, 5); return; }
            FS.syncfs(false, error => error ? reject(error) : resolve());
        }
        flush();
    });
});

EM_JS(void, re1_web_finish, (int status), {
    globalThis.re1Exit?.(status);
});

EM_JS(void, re1_web_stage, (const char* phase), {
    globalThis.RE1Diag?.stage(UTF8ToString(phase));
});
EM_JS(void, re1_web_present_frame, (int mode, float playerY, unsigned camera, int menu, int generated,unsigned tickMs,int aiming,unsigned enemies), {
    globalThis.RE1Presentation?.frame(mode, playerY, camera);
    globalThis.re1TouchContext?.(!!menu);
    globalThis.RE1Diag?.frame(!!generated,tickMs,!!aiming,camera,enemies,mode);
});
EM_JS(float, re1_web_inventory_extra, (), {
    return globalThis.RE1Presentation?.inventoryExtra || 0;
});
EM_JS(float, re1_web_prompt_offset, (float y,unsigned camera), {
    return globalThis.RE1Presentation?.promptOffset(y,camera)||0;
});
extern "C" float re1_web_layout(float* promptShift,unsigned* promptFont,int* itemBox)
{
    *promptShift=0;*promptFont=0;*itemBox=0;
    const int mode=WebPresentationMode();
    if(mode==5||mode==6||mode==7){
        *itemBox=mode==7?(MenuMapFilesLayout()==2?2:1):int(mode==6);
        return re1_web_inventory_extra();
    }
    if(mode==3&&g_pMarniDirect3D){
        const unsigned camera=(unsigned(g_stageId)<<16)|(unsigned(g_roomId)<<8)|g_roomCameraId;
        *promptShift=re1_web_prompt_offset(WebPlayerScreenY(),camera);
        *promptFont=static_cast<CMarniDirect3D*>(g_pMarniDirect3D)->m_FontTexHandle;
    }
    return 0;
}
extern "C" void re1_web_presented(int generated)
{
    const int mode=WebPresentationMode();
    const unsigned camera=(unsigned(g_stageId)<<16)|(unsigned(g_roomId)<<8)|g_roomCameraId;
    const bool aiming=mode==1&&g_playerEntity.animationId==1&&g_playerEntity.animFrameId==3;
    unsigned enemies=0;
    if(aiming)for(unsigned i=0;i<unsigned(g_enemy_count)&&i<30;++i){
        const auto& enemy=g_EnemiesList[i];
        if((enemy.status_flags&1)&&enemy.health>=0)++enemies;
    }
    re1_web_present_frame(mode, mode==1||mode==3?WebPlayerScreenY():120.0f, camera, WebTouchMenuActive(), generated,
        plat_time_ms()-g_dwGameTimer1,aiming,enemies);
}

void re1_web_video_stage(const char* path) { re1_web_stage(path); }
EM_JS(void, re1_web_graphics_result, (int success), {
    globalThis.RE1Diag?.graphicsResult(success);
});
