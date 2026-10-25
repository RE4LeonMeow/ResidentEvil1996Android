// Room.cpp - Room sprite, camera, and background management
// Decompiled from Ghidra
#include "../Globals.h"
#include "FileLoader.h"
#include "SpriteRenderer.h"
#include <cstdio>
#include "../system/AssetPath.h"
#include "../DebugPrint.h"
#include "dc/ArrangeStages.h"   // room_file_stage()

extern void SetSpriteBufferFlag(void);

// ============================================================================
// cut_set (0x004628c0)
// Handles room cutscene/camera transition when entering a new room context.
// Restores texture bank, rebuilds sprite entries, sets up camera, loads
// background image, and processes sprite visibility flags.
// ============================================================================
void cut_set(void) // 0x004628c0
{
    printf("cut set  start\n");
    if ((unsigned char)g_SavedTextureBankID != 0) {
        int maskFrames;
        if ((g_stageId == STAGE_MANSION_RETURN_1F) && (g_roomId == ROOM_MANSION_BATHROOM)) {
            maskFrames = 3;
        } else {
            maskFrames = 2;
        }
        StMask(0, maskFrames);

        *(unsigned short*)&g_TextureBankCell = g_SavedTextureBankID;

        Room_LoadCameraSprites();

        if ((g_main_state_flags & MSF_ROOM_TRANSITION) != 0) {
            return;
        }

        Room_SetupCamera();
        load_room_bg_image();
        Room_ApplySpriteFlags();
    }
    printf("cut set  end\n");
}

// ============================================================================
// RestoreRoomCamera (0x00462940)
// Restore the room texture bank and camera after the main menu closes.
// The menu's fixed menu camera (MATRIX_00d22680) is written into
// g_RoomCameraData on open; this re-applies the RDT camera record the same
// way cut_set does. Skips the camera when the cutscene flag is set, and does
// nothing when no bank was saved (menu never opened a bank-swapping view).
// ============================================================================
void RestoreRoomCamera(void) // 0x00462940
{
    if ((unsigned char)g_SavedTextureBankID != 0) {
        *(unsigned short*)&g_TextureBankCell = g_SavedTextureBankID;
        if ((g_main_state_flags & MSF_ROOM_TRANSITION) == 0) {
            Room_SetupCamera();
        }
    }
}

// ============================================================================
// is_entity_in_switch_zone (0x00462d90)
// Point-in-quadrilateral test: is `position` inside `zone`?
//
// Four cross-product edge tests, pivoting on corner 0 for the first two edges
// and on corner 2 for the last two. Only the X and Z components of `position`
// are used (the room's floor plane); Y is ignored.
//
// FAITHFULNESS NOTE: the original zero-extends every zone coordinate to 32 bits
// (XOR reg,reg / MOV reg16,[zone+n]), so the shorts behave as UNSIGNED 16-bit
// values. For the zone-relative differences this is harmless — both operands
// shift by the same 0x10000, so x1-x0 is unchanged — but `position->x - x0`
// mixes a full signed int with a zero-extended coordinate, so a zone corner at
// a negative coordinate does NOT behave as negative here. That is the original's
// behaviour and is reproduced deliberately; do not "fix" it to a signed read.
// ============================================================================
int is_entity_in_switch_zone(VECTOR* position, void* zoneData) // 0x00462d90
{
    CAM_SWITCH_ZONE* zone = (CAM_SWITCH_ZONE*)zoneData;

    // Port-only guard: the original has no NULL check, but the port calls this
    // from the options-menu entity preview, where there is no RDT zone table.
    if (zone == NULL) return 1;

    // 0x00462d9a-0x00462da2: pivot on corner 0, zero-extended
    int x0 = (int)(unsigned short)zone->x0;
    int y0 = (int)(unsigned short)zone->y0;

    // 0x00462db1-0x00462dbe: position relative to corner 0
    int dx = position->x - x0;
    int dz = position->z - y0;

    // 0x00462db3-0x00462de6: the other corners, relative to corner 0
    int x1r = (int)(unsigned short)zone->x1 - x0;
    int y1r = (int)(unsigned short)zone->y1 - y0;
    int x3r = (int)(unsigned short)zone->x3 - x0;
    int y3r = (int)(unsigned short)zone->y3 - y0;

    // 0x00462e08: edge 0->1
    if (x1r * dz > y1r * dx) return 0;

    // 0x00462e32: edge 0->3
    if (x3r * dz < y3r * dx) return 0;

    // 0x00462e38-0x00462e59: re-pivot every term on corner 2
    int x2 = (int)(unsigned short)zone->x2;
    int y2 = (int)(unsigned short)zone->y2;

    int dx2 = position->x - x2;
    int dz2 = position->z - y2;

    int x1p = (int)(unsigned short)zone->x1 - x2;
    int y1p = (int)(unsigned short)zone->y1 - y2;
    int x3p = (int)(unsigned short)zone->x3 - x2;
    int y3p = (int)(unsigned short)zone->y3 - y2;

    // 0x00462e6e: edge 2->1
    if (x1p * dz2 < y1p * dx2) return 0;

    // 0x00462e7a: edge 2->3
    if (x3p * dz2 > y3p * dx2) return 0;

    // 0x00462e7e
    return 1;
}

// ============================================================================
// display_room_camera_bg (0x00462d50)
// Point g_CurrentRdtDataTypePtr at the switch-zone group belonging to the
// current camera, then hand off to cut_set() to actually put the room on
// screen (sprites, camera transform, background image).
//
// The walk has no bound: the RDT is expected to contain a group for every
// camera id that g_roomCameraId can hold. A bad g_roomCameraId runs off the
// end of the table, which is the original's behaviour.
// ============================================================================
void display_room_camera_bg(void) // 0x00462d50
{
    // 0x00462d5d: start at the head of the switch-zone table
    CAM_SWITCH_ZONE* zone = (CAM_SWITCH_ZONE*)g_RdtPointer->cam_switch_zones;
    g_CurrentRdtDataTypePtr = zone;

    // 0x00462d66-0x00462d81: advance to the group whose camFrom is this camera
    while ((unsigned short)zone->camFrom != (unsigned short)g_roomCameraId) {
        zone++;
        g_CurrentRdtDataTypePtr = zone;
    }

    // 0x00462d83
    cut_set();
}

// ============================================================================
// check_camera_switch (0x00462cc0)
// Test the player against each switch zone of the current camera's group. On a
// hit, switch g_roomCameraId to that zone's camTo and redisplay.
//
// param_1 != 0 forces a cut_set() even when no zone matched — that is how the
// initial room display happens: room_set() calls check_camera_switch(1) after
// loading everything, and the "no zone matched" path is what actually puts the
// starting camera on screen.
//
// The original returns EAX (0 on the paths that do work, and whatever was in
// EAX on entry when camera changes are disabled). No caller reads it, so this
// is declared void.
// ============================================================================
void check_camera_switch(int param_1) // 0x00462cc0
{
    // 0x00462cc1-0x00462cc7: first candidate zone is the one AFTER the group
    // header that g_CurrentRdtDataTypePtr points at
    CAM_SWITCH_ZONE* zone = (CAM_SWITCH_ZONE*)g_CurrentRdtDataTypePtr + 1;

    // 0x00462cca: camera changes disabled during cutscenes
    if ((g_main_state_flags & MSF_CAMERA_LOCK) != 0) {
        return;
    }

    // 0x00462cdb-0x00462d02: walk this camera's zones
    while ((unsigned short)zone->camFrom == (unsigned short)g_roomCameraId) {
        if (is_entity_in_switch_zone(
                (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, zone) != 0)
        {
            // 0x00462d1b: enter the new camera
            g_roomCameraId = (unsigned char)zone->camTo;

            if ((g_main_state_flags & MSF_CAMERA_DEFER) != 0) {
                // 0x00462d24: defer the redisplay to game_loop (bit 0x20)
                g_main_state_flags |= MSF_CAMERA_REDRAW;
                StMask(0, 5);
                return;
            }

            // 0x00462d3b: redisplay immediately
            StMask(0, 4);
            display_room_camera_bg();
            return;
        }
        zone++;
    }

    // 0x00462d04: no zone matched
    if (param_1 != 0) {
        cut_set();
    }
}

// ============================================================================
// Room_LoadCameraSprites (0x004757c0)
// Populate room sprite entries (g_RoomSprEntries) from RDT camera sprite
// overlay data. Called by cut_set during camera transitions.
// ============================================================================
void Room_LoadCameraSprites(void) // 0x004757c0
{
    unsigned short sprIndex = 0;
    unsigned char totalCount = 0;

    RDT_Camera* cameras = (RDT_Camera*)((char*)g_RdtPointer + sizeof(RDT));
    int* spriteGroupBase = (int*)cameras[g_roomCameraId].mask_pointer;
    int groupCount = spriteGroupBase[0];
    unsigned short* groupHeaders = (unsigned short*)(spriteGroupBase + 1);
    unsigned short* spriteData = groupHeaders + groupCount * 4;

    if (groupCount != 0) {
        load_room_masks(g_roomCameraId);
    }

    unsigned int grpIdx = 0;
    if (*spriteGroupBase != 0) {
        do {
            unsigned int sprCount = 0;
            if (groupHeaders[0] != 0) {
                unsigned short pageByte = (unsigned short)g_TextureCurrentPage;
                unsigned short bankID = (unsigned short)g_TextureBankID;
                unsigned short* sprPtr = spriteData;
                do {
                    RoomSprEntry* entry = &g_RoomSprEntries[sprIndex];

                    entry->active = 1;
                    entry->id = (unsigned char)grpIdx + 1;
                    entry->texDesc.clutX = (groupHeaders[1] & 0x3f) << 4;
                    entry->texDesc.clutY = (short)(pageByte + 0x1e0);
                    entry->texDesc.texU = (unsigned char)sprPtr[0];
                    entry->texDesc.texV = *((unsigned char*)sprPtr + 1);
                    entry->texDesc.screenX = (short)((unsigned short)(unsigned char)sprPtr[1] + groupHeaders[2] - 0xa0);
                    entry->texDesc.screenY = (short)(*((unsigned char*)sprPtr + 3) + groupHeaders[3] - 0x78);
                    entry->posData = sprPtr[2];

                    unsigned short flags = sprPtr[3];
                    entry->texDesc.texturePage = (short)((flags & 0x1f) + bankID);

                    if ((flags & 0xf000) == 0) {
                        spriteData = sprPtr + 6;
                        entry->texDesc.width = sprPtr[4];
                        entry->texDesc.height = sprPtr[5];
                    } else {
                        spriteData = sprPtr + 4;
                        unsigned short sz = (flags & 0xf1ff) >> 9;
                        entry->texDesc.width = sz;
                        entry->texDesc.height = sz;
                    }

                    entry->texDesc.flags = 0x40;
                    if ((flags & 0xc00) == 0) {
                        entry->texDesc.flags = 0x8000040;
                    }

                    sprCount++;
                    unsigned int transMode = (unsigned int)((flags & 0x180) >> 7) << 0x18;
                    sprIndex++;
                    entry->texDesc.flags |= transMode;
                    entry->texDesc.flags |= (unsigned int)((flags & 0x60) >> 5) << 0x1c;

                    sprPtr = spriteData;
                } while (sprCount < groupHeaders[0]);
            }
            totalCount = (unsigned char)sprIndex;
            groupHeaders += 4;
            grpIdx++;
        } while (grpIdx < (unsigned int)(*spriteGroupBase));
    }
    g_RdtPointer->sprites_count = totalCount;
}

// ============================================================================
// Room_SetupCamera (0x00462970)
// Set camera rendering parameters from RDT camera data.
// Sets the scene render param from camera FOV and computes the camera matrix.
// ============================================================================
void Room_SetupCamera(void) // 0x00462970
{
    RDT_Camera* cameras = (RDT_Camera*)((char*)g_RdtPointer + sizeof(RDT));
    set_scene_render_param(cameras[g_roomCameraId].fov);
    MatrixToCamera((MATRIX*)&cameras[g_roomCameraId].cam_from_x);
}

// ============================================================================
// Room_ApplySpriteFlags (0x00432220)
// Process sprite visibility flags from DAT_00d22770.
// For each bit set in DAT_00d22770, disables (active=0) all room sprites
// whose id matches that bit index. Then clears DAT_00d22770.
// ============================================================================
int Room_ApplySpriteFlags(void) // 0x00432220
{
    unsigned int bitIdx = 0;
    do {
        if ((DAT_00d22770 & 1) != 0) {
            unsigned int sprIdx = 0;
            if (g_RdtPointer->sprites_count != 0) {
                RoomSprEntry* entry = g_RoomSprEntries;
                do {
                    if (entry->id == bitIdx) {
                        entry->active = 0;
                    }
                    entry++;
                    sprIdx++;
                } while (sprIdx < g_RdtPointer->sprites_count);
            }
        }
        DAT_00d22770 = (int)DAT_00d22770 >> 1;
        bitIdx++;
    } while (bitIdx < 0x10);
    DAT_00d22770 = 0;
    return 1;
}

// ============================================================================
// Room mask ordering tables (0x004c3be8 / 0x004c3c88 / 0x004c4188)
//
// DrawRoomSpr does NOT sort the overlays by their raw posData. It first looks
// up a per-(room, camera) record that biases both the sprite's brightness and
// its ordering-table key, and a per-room bitmask that selects which of the two
// entry walks to use. All three tables are contiguous in the original's .data:
//
//   0x004c3be8  unsigned char[160]     bit per camera: 1 = walk entries forward
//   0x004c3c88  unsigned char[160][8]  record index for (room, camera)
//   0x004c4188  { int, int, int }[32]  { depthBias, fadeBias, mode }
//
// The room index is roomId + stage*0x20 with stages 5-9 folded onto 0-4
// (0x00475ba4-0x00475bb1).
//
// The port had none of this: every overlay went out with depth = posData >> 2
// and sort key = posData, i.e. record 0 for every room. That is right for the
// ~130 (room, camera) pairs whose record IS 0, and wrong for the 29 that are
// not - which is why only *some* masks sorted incorrectly against the player.
// ============================================================================
struct RoomSprDepthRecord {
    int depthBias;   // 0x004c4188 + i*0xc: subtracted from the brightness key
    int fadeBias;    // 0x004c418c + i*0xc: subtracted from the OT sort key
    int mode;        // 0x004c4190 + i*0xc: non-zero pins the brightness key
};

// 0x004c3be8
static const unsigned char kRoomSprPathFlags[160] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// 0x004c3c88 - indexed [roomIdx * 8 + cameraId]
static const unsigned char kRoomSprRecordIndex[160 * 8] = {
    0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 31, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0,
    0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 31, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 21, 10, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 27, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 26, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 18, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    22, 0, 0, 0, 0, 0, 0, 0, 23, 24, 25, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 13, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// 0x004c4188 - { depthBias, fadeBias, mode }
static const RoomSprDepthRecord kRoomSprDepthRecords[32] = {
    {      0,      0,      0 },  // 0  - the default for every unlisted camera
    {   -250,      0,      0 },  // 1
    {     30,     -5,      0 },  // 2
    {      0,    -40,      0 },  // 3
    {    -30,      0,      0 },  // 4
    {    -70,      0,      0 },  // 5
    {     60,    -40,      0 },  // 6
    {   -100,    -20,      0 },  // 7
    {    100,    -10,      0 },  // 8
    {      0,    -10,      0 },  // 9
    {      0,     19,      0 },  // 10
    {      5,      0,      0 },  // 11
    {      1,      8,      0 },  // 12
    {   -100,      0,      0 },  // 13
    {    -50,      0,      0 },  // 14
    {      0,     -2,      0 },  // 15
    {      0,      1,      0 },  // 16
    {      0,     -4,      0 },  // 17
    {      0,    -20,      0 },  // 18
    {      0,     -5,      0 },  // 19
    {   1000,      0,      0 },  // 20
    {      0,      0,      1 },  // 21 - the only record that pins the key
    {     90,      0,      0 },  // 22
    {    -30,    -65,      0 },  // 23
    {    -70,    -30,      0 },  // 24
    {      0,      0,      0 },  // 25
    {     10,      0,      0 },  // 26
    {    -10,      0,      0 },  // 27
    {   -100,      0,      0 },  // 28
    {     84,      0,      0 },  // 29
    {      0,    100,      0 },  // 30
    {      0,    -11,      0 },  // 31
};

// 0x004760ec: the brightness key is posData >> 2, clamped once posData leaves
// the 12-bit ordering-table range. The compare is on posData & 0xfffc, not on
// posData, so the bottom two bits never push it over the clamp.
static inline unsigned short RoomSprBrightnessKey(unsigned short posData)
{
    return ((posData & 0xfffc) < 0x1000) ? (unsigned short)(posData >> 2)
                                         : (unsigned short)0x3ff;
}

// A positive fadeBias on an overlay sitting almost on the camera can drive the
// sort key negative. The original fed that straight into the ordering table,
// which masked the index; this port stores depthSort as an unsigned int, where
// the same value wraps to ~4e9 and drops the overlay behind the entire scene.
// Clamp instead - a key of 0 is the nearest slot, which is what a bias meant to
// pull the overlay forward was asking for.
//
// The `& ~3` is the ordering-table BUCKET, and it is not cosmetic. The PS1
// DrawRoomSpr (SLUS_001.70 0x80037964) submits every overlay with
//
//     GsSortFastSprite(spr, ot, min(posData >> 2, 0x3ff))
//
// so four consecutive posData values share ONE ordering-table slot and their
// relative order comes from the submission order alone. That is exactly what
// the backward entry walk below exists to preserve (see the path-flag note) -
// but only if overlays the original could not tell apart keep one key here too.
// This port's depthSort is `fade << 4`, four times finer than the original's
// slot, so two overlapping overlays two posData units apart got reordered.
//
// Found on the Director's Cut arrange wardrobe closet (stage 1 room 0x1C ->
// Stage8/ROOM81C0, camera 0). Groups 1 and 5 are the dressed and the bare
// mannequin and the room's script leaves BOTH active in that camera, relying
// on group 1 (the earlier entry) to cover group 5. Group 5's 24x32 chest tile
// carries posData 220 against group 1's 222: one bucket on PS1, two keys here,
// and the bare mannequin painted a grey rectangle across the outfit. 61 other
// (room, camera) pairs in the shipped USA rooms carry the same kind of pair.
static inline int RoomSprClampSortKey(int fade)
{
    return (fade < 0) ? 0 : (fade & ~3);
}

// ==========================================================================
// DrawRoomSpr (0x00475b80)
// Submit active room-overlay sprites to the 2D sprite queue. The room4080
// numeric panel is made from nine of these entries: the passcode state machine
// only changes their active flags, while this per-frame pass renders them.
//
// AddSprite's arguments are not named the way they read: arg 2 ("depth") only
// picks the sprite's brightness and, when zero, a fixed 550 sort slot; arg 4
// ("fade") is the real ordering-table key - AddSprite stores fade << 4 as
// depthSort. Because entities enter the ordering table at t[2] >> 4 (the
// depthShift FUN_00483250 passes), fade << 4 is view-space Z in the same units
// as the TMD pass, which is what lets the two interleave. See FlushTmdObjects.
// ==========================================================================
void DrawRoomSpr(void)
{
    // 0x00475b83: skip room sprites while the corresponding render-state bit is
    // active or before the room has populated its camera sprite table.
    if ((g_main_state_flags2 & MSF2_ROOM_SPRITES_OFF) != 0 ||
        g_RdtPointer == NULL || g_RdtPointer->sprites_count == 0) {
        return;
    }

    // 0x00475ba4-0x00475bbe: stages 5-9 reuse the stage 0-4 table rows.
    unsigned int stage = get_stage_id();
    if (stage > 4) stage -= 5;
    const unsigned int roomIdx = (unsigned int)g_roomId + stage * 0x20;
    const unsigned int cam     = (unsigned int)g_roomCameraId;

    // 0x00475bdb-0x00475c07: the two bias globals the original adds to the
    // record (0x004c3bc0 / 0x004c3bc4) have no writer anywhere in the binary
    // and are both zero, so the record supplies the biases outright.
    //
    // The original indexes both tables blind. A room past the end would read
    // whatever follows them in .data; fall back to record 0 and the backward
    // walk instead, which is the entry every unlisted camera uses anyway.
    const RoomSprDepthRecord* rec = &kRoomSprDepthRecords[0];
    unsigned char pathFlags = 0;
    if (roomIdx < 160 && cam < 8) {
        rec = &kRoomSprDepthRecords[kRoomSprRecordIndex[roomIdx * 8 + cam]];
        pathFlags = kRoomSprPathFlags[roomIdx];
    }
    const short depthBias = (short)rec->depthBias;
    const int   fadeBias  = rec->fadeBias;
    const int   mode      = rec->mode;

    // 0x00475c0d: the courtyard boulder 1 passage / camera 5 shot hides two overlays.
    if (g_stageId == STAGE_COURTYARD && g_roomId == ROOM_BOULDER_1_PASSAGE && cam == 5) {
        g_RoomSprEntries[25].active = 0;
        g_RoomSprEntries[26].active = 0;
    }

    const int count = (int)g_RdtPointer->sprites_count;

    // 0x00475c3a: the per-room bitmask picks the walk direction. Only 8 rooms
    // walk forward; everything else walks the entries backwards so that
    // same-depth overlays keep the original painter order.
    if ((pathFlags & (1u << (cam & 7))) != 0) {
        // 0x00475c42: the falls / camera 0 shot pushes overlay 18
        // 200 units farther back than its posData asks for.
        const bool pushEntry18 = (g_stageId == STAGE_COURTYARD && g_roomId == ROOM_FALLS && cam == 0);

        for (int i = 0; i < count; i++) {
            RoomSprEntry* entry = &g_RoomSprEntries[i];
            if (entry->active == 0) continue;

            const unsigned short posData = entry->posData;
            short depth;
            int   fade;

            if (pushEntry18 && i == 18) {
                depth = (short)(RoomSprBrightnessKey(posData) - depthBias);
                fade  = (int)posData - fadeBias + 200;
            }
            else if (mode == 0) {
                depth = (short)(RoomSprBrightnessKey(posData) - depthBias);
                fade  = (int)posData - fadeBias;
            }
            else {
                // 0x00475cc3: the key is pinned to the record's depthBias, so
                // every overlay in this camera shares one brightness.
                depth = depthBias;
                fade  = (int)posData - fadeBias;
            }

            AddSprite(&entry->texDesc, depth, ROOM_MASK_TEXTURE_SLOT,
                      RoomSprClampSortKey(fade));
        }
        return;
    }

    // 0x00475da2: the backward walk, with two rooms carrying hand-tuned
    // per-overlay offsets that the shared record cannot express.
    const bool isRoom3_0e = (g_stageId == STAGE_GUARDHOUSE && g_roomId == ROOM_WATER_TANK);
    const bool isRoom3_0f = (g_stageId == STAGE_GUARDHOUSE && g_roomId == ROOM_SECURITY_ROOM);

    for (int i = count - 1; i >= 0; i--) {
        RoomSprEntry* entry = &g_RoomSprEntries[i];
        if (entry->active == 0) continue;

        const unsigned short posData = entry->posData;
        const short key = (short)RoomSprBrightnessKey(posData);
        short depth;
        int   fade;

        if (isRoom3_0e) {
            // 0x00475dde-0x00475fa0
            if      (cam == 1 && i == 11) { depth = (short)(key - depthBias); fade = 0x4b0; }
            else if (cam == 1 && i ==  9) { depth = (short)(key - 0x5a); fade = (int)posData - fadeBias; }
            else if (cam == 3 && i ==  9) { depth = (short)(key - 0x3d); fade = (int)posData - fadeBias; }
            else if (cam == 3 && i ==  7) { depth = (short)(key - 0x4b); fade = (int)posData - fadeBias; }
            else if (cam == 3 && i == 31) { depth = (short)(key - 0x3c); fade = (int)posData - fadeBias; }
            else if (cam == 3 && i == 32) { depth = (short)(key - 0x3c); fade = (int)posData - fadeBias; }
            else if (cam == 3 && i == 33) continue;   // 0x00475f89: never drawn
            else if (cam == 2 && i == 28) continue;   // 0x00475f9a: never drawn
            else { depth = (short)(key - depthBias); fade = (int)posData - fadeBias; }
        }
        else if (isRoom3_0f && i >= 34 && i <= 52) {
            // 0x0047602c: this run of overlays is pulled 0x352 brighter and its
            // sort key is offset by a constant instead of the camera's bias.
            depth = (short)((short)(key - depthBias) - 0x352);
            fade  = (int)posData + (i == 49 ? 0x23 : 0x46);
        }
        else if (isRoom3_0f && cam == 0 && i == 54) {
            // 0x004760b4
            depth = (short)((short)(key - depthBias) - 0x12c);
            fade  = (int)posData - fadeBias;
        }
        else {
            // 0x004760e4: the common case.
            depth = (short)(key - depthBias);
            fade  = (int)posData - fadeBias;
        }

        AddSprite(&entry->texDesc, depth, ROOM_MASK_TEXTURE_SLOT,
                  RoomSprClampSortKey(fade));
    }
}

// RoomSpr_SetActive (0x00476170) - Enable room sprite by ID
// Iterates through the room sprite table and sets active=1 for all entries
// whose id matches the given parameter. Called by SCD command 0x25.
// ============================================================================
void RoomSpr_SetActive(char id) // 0x00476170
{
    unsigned int i = 0;
    if (g_RdtPointer->sprites_count != 0) {
        RoomSprEntry* entry = g_RoomSprEntries;
        do {
            if (entry->id == id) {
                entry->active = 1;
            }
            entry++;
            i++;
        } while (i < g_RdtPointer->sprites_count);
    }
}

// ============================================================================
// RoomSpr_SetInactive (0x00476130) - Disable room sprite by ID
// Iterates through the room sprite table and sets active=0 for all entries
// whose id matches the given parameter. Called by SCD command 0x25.
// ============================================================================
void RoomSpr_SetInactive(char id) // 0x00476130
{
    unsigned int i = 0;
    if (g_RdtPointer->sprites_count != 0) {
        RoomSprEntry* entry = g_RoomSprEntries;
        do {
            if (entry->id == id) {
                entry->active = 0;
            }
            entry++;
            i++;
        } while (i < g_RdtPointer->sprites_count);
    }
}

// ============================================================================
// load_room_masks (0x00475a90)
// Load sprite mask/texture data for the specified camera.
// If the camera has sprite groups, loads the corresponding PAK file and
// sets up texture pages. Otherwise, deletes texture slot 4.
// ============================================================================
void load_room_masks(int param_1) // 0x00475a90
{
    RDT_Camera* cameras = (RDT_Camera*)((char*)g_RdtPointer + sizeof(RDT));
    int* spriteGroupPtr = (int*)cameras[param_1].mask_pointer;

    // An arrange room's mask art is not in objspr/ - it is EMBEDDED in the RDT,
    // at the camera's tim_mask_pointer, and it is already in memory. That is
    // where the PC's objspr paks come from in the first place: the pak for
    // stage 2 room 3 camera 0 is that camera's RDT TIM, LZW'd, and the two
    // match to a single byte in 66080. The base rooms keep reading their paks,
    // which is the shipped, tested path; the arrange rooms read the RDT
    // directly.
    //
    // Those TIMs are 256 px WIDE but not always 256 tall - Stage8/ROOM81C0
    // carries 256x176, 256x184, 256x184 and 256x216 for its four cameras. Both
    // the page upload (LoadEffectTextureSheet) and the UV normalisation take
    // the height from the TIM header, so a short page samples correctly; do
    // not reintroduce a 256x256 assumption anywhere on this path.
    if (*spriteGroupPtr != 0 && room_file_stage() >= STAGE_ARRANGE_FIRST) {
        void* timMask = (void*)cameras[param_1].tim_mask_pointer;
        if (timMask != NULL) {
            TexturePage_SetupFull(timMask, g_TextureBankID, g_TextureCurrentPage,
                                  ROOM_MASK_TEXTURE_SLOT);
        } else {
            TexturePage_DeleteSet(ROOM_MASK_TEXTURE_SLOT);
        }
        return;
    }

    if (*spriteGroupPtr != 0) {
        // The revisit fold, which applies inside the arrange block too - see
        // load_room_bg. This site does its arithmetic on the ID rather than the
        // character (`+ 0x2b` is `- 5 + 0x30`), so it is already correct for an
        // arrange stage: id 12 -> '7'.
        g_maskPathTemplate[GAME_DATA_PATH_IDX(0x11)] = (char)(g_stageId + 0x30);
        if (get_stage_id() > 4) {
            g_maskPathTemplate[GAME_DATA_PATH_IDX(0x11)] = (char)(g_stageId + 0x2b);
        }
        g_maskPathTemplate[GAME_DATA_PATH_IDX(0x12)] = (char)(g_roomId / 10 + 0x30);
        g_maskPathTemplate[GAME_DATA_PATH_IDX(0x14)] = (char)(param_1 + '0');
        g_maskPathTemplate[GAME_DATA_PATH_IDX(0x13)] = (char)(g_roomId % 10 + 0x30);

        void* pakData;
        // ROOM3110.RDT (scrapped dev heliport, Stage 3 + ROOM_SCRAPPED_HELIPORT) is
        // a stub with empty camera mask blocks, so its mask pak is loaded directly
        // instead of coming from the preloaded g_bgMaskDataBuffer.
        if ((g_stageId == STAGE_COURTYARD) && (g_roomId == ROOM_SCRAPPED_HELIPORT)) {
            LoadFile(g_maskPathTemplate, &g_bgPakLoadBuffer, 0x20);
            pakData = &g_bgPakLoadBuffer;
        } else {
            pakData = &g_bgMaskDataBuffer[g_bgMaskOffsets[param_1]];
        }
        unpack_pakfile_(pakData, g_TimImageBuffer__bitmap);
        // The original passes texture-set parameter 0 here. Its legacy page
        // handle is stored in texture set 4 internally (SetupFull adds the 4),
        // while this port keeps the D3D SRV at the direct slot - see
        // ROOM_MASK_TEXTURE_SLOT.
        TexturePage_SetupFull(g_TimImageBuffer__bitmap, g_TextureBankID,
                              g_TextureCurrentPage, ROOM_MASK_TEXTURE_SLOT);
    } else {
        TexturePage_DeleteSet(ROOM_MASK_TEXTURE_SLOT);
    }
}

// ============================================================================
// load_room_bg (0x00462b00) - Load room background images for all cameras
// For each camera in the room, loads the background PAK file, decompresses it,
// and either displays it immediately (mode 0) or caches it (mode non-zero).
// ============================================================================
void load_room_bg(void) // 0x00462b00
{
    if (g_bgCacheMode == 0) {
        int cameraIdx = 0;
        if (g_RdtPointer->cameras_count != 0) {
            do {
                // The stage the FILES come from: g_stageId, or its arrange
                // twin when ADVANCED has one for this room (dc/ArrangeStages).
                const unsigned char fileStage = room_file_stage();
                DAT_004c2090 = g_hexCharTable[cameraIdx];
                g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0f)] = g_hexCharTable[fileStage + 1];
                g_bgPathTemplate[GAME_DATA_PATH_IDX(0x10)] = g_hexCharTable[g_roomId >> 4];
                g_bgPathTemplate[GAME_DATA_PATH_IDX(0x11)] = g_hexCharTable[g_roomId & 0xf];
                // The revisit fold, which applies INSIDE the arrange block too:
                // STAGED/STAGEE ship no backgrounds of their own and reuse
                // STAGE8/STAGE9's, exactly as base stages 6/7 reuse stage 1/2's.
                // get_stage_id() > 4 covers both (base 5,6 and arrange 12,13)
                // and leaves arrange 7-11, which do have their own art, alone.
                //
                // The original's character arithmetic only works inside the run
                // of decimal digits ('6' - 5 == '1'), so it breaks for an
                // arrange stage ('d' - 5 is '_'). Arrange recomputes from the
                // id; the base path keeps the original expression untouched,
                // including its quirk (see below).
                //
                // QUIRK, reproduced: this branch folds from index 0x11 - the
                // ROOM's low hex digit - where the cached branch below uses
                // 0x0f, the stage digit. Verified against the original
                // (0x00462b00): `[0xf] = [0x11] + -5` here, `[0xf] = [0xf] + -5`
                // there. It is a Capcom bug, and for stage 5/6 it builds a path
                // with '-' or similar in it that simply fails to load. Left as
                // it is because the port reproduces the original; normal room
                // loads take the cached branch.
                if (STAGE_DATA_ROW_OF(fileStage) > 4) {
                    if (fileStage >= STAGE_ARRANGE_FIRST) {
                        g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0f)] =
                            g_hexCharTable[fileStage - 5 + 1];
                    } else {
                        g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0f)] = g_bgPathTemplate[GAME_DATA_PATH_IDX(0x11)] - 5;
                    }
                }
                g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0b)] = g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0f)];

                SetSpriteBufferFlag();
                LoadFile(g_bgPathTemplate, g_bgPakLoadBuffer, 2);
                unpack_pakfile_(g_bgPakLoadBuffer, g_TimImageBuffer);

                int width, height;
                if ((g_main_state_flags2 & MSF2_SCREEN_BORDER) == 0) {
                    width = 320;
                    height = 240;
                } else {
                    width = 316;
                    height = 236;
                }

                display_image(cameraIdx, g_TimImageBuffer__bitmap, width, height);
                title_setup_texture_pages(cameraIdx, 1);
                cameraIdx++;
            } while (cameraIdx < (int)(unsigned int)g_RdtPointer->cameras_count);
        }
    } else {
        int totalSize = 0;
        int camCounter = 0;
        if (g_RdtPointer->cameras_count != 0) {
            do {
                const unsigned char fileStage = room_file_stage();
                g_bgPathTemplate[GAME_DATA_PATH_IDX(0x12)] = g_hexCharTable[camCounter];
                g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0f)] = g_hexCharTable[fileStage + 1];
                g_bgPathTemplate[GAME_DATA_PATH_IDX(0x10)] = g_hexCharTable[g_roomId >> 4];
                g_bgPathTemplate[GAME_DATA_PATH_IDX(0x11)] = g_hexCharTable[g_roomId & 0xf];
                // The revisit fold, which applies INSIDE the arrange block too:
                // STAGED/STAGEE ship no backgrounds of their own and reuse
                // STAGE8/STAGE9's, exactly as base stages 6/7 reuse stage 1/2's.
                // get_stage_id() > 4 covers both (base 5,6 and arrange 12,13)
                // and leaves arrange 7-11, which do have their own art, alone.
                //
                // The original's character arithmetic only works inside the run
                // of decimal digits ('6' - 5 == '1'), so it breaks for an
                // arrange stage ('d' - 5 is '_'). Arrange recomputes from the
                // id; the base path keeps the original expression untouched,
                // including its quirk (see below).
                if (STAGE_DATA_ROW_OF(fileStage) > 4) {
                    if (fileStage >= STAGE_ARRANGE_FIRST) {
                        g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0f)] =
                            g_hexCharTable[fileStage - 5 + 1];
                    } else {
                        g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0f)] = g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0f)] - 5;
                    }
                }
                camCounter++;
                g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0b)] = g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0f)];

                SetSpriteBufferFlag();
                // 0x00462b?? : *(int *)(iVar3 * 4 + 0xaea08c) = iVar2, with the
                // camera counter ALREADY incremented - so the offset of image i
                // lands in slot i+1 of the array based at 0x00aea08c. That is not
                // a bug in the original, because load_room_bg_image reads the
                // table from a base 4 bytes HIGHER (0x00aea090), which cancels
                // the shift exactly. See the matching note there: the port shares
                // one array for both, so the reader has to add the 1 back.
                g_bgCameraOffsets[camCounter] = totalSize;
                int fileSize = (int)LoadFile(g_bgPathTemplate, &g_bgCacheBuffer[totalSize], 2);
                totalSize += fileSize;
            } while (camCounter < (int)(unsigned int)g_RdtPointer->cameras_count);
        }
    }
}

// ============================================================================
// load_room_bg_image (0x004629c0) - Load a specific camera's background image
// Called during camera switches. In mode 0, just activates the cached texture.
// In cache mode or for special rooms, loads/decompresses the specific camera bg.
// ============================================================================
void load_room_bg_image(void) // 0x004629c0
{
    // ROOM3110.RDT (Courtyard 0x11) is a scrapped dev heliport stub RDT that also
    // exists in the PS1 version - the game never enters it (the shipped heliport is
    // ROOM_HELIPORT). Its own bg paks rc3110-2.pak are the heliport at different
    // camera angles, loaded directly on every camera switch instead of from the cache.
    if (g_bgCacheMode == 0 && (g_stageId != STAGE_COURTYARD || g_roomId != ROOM_SCRAPPED_HELIPORT)) {
        // empty_00470960(g_roomCameraId): empty in the original - call dropped
        return;
    }

    void* pakData;
    if (g_stageId == STAGE_COURTYARD && g_roomId == ROOM_SCRAPPED_HELIPORT) {
        g_bgPathTemplate[GAME_DATA_PATH_IDX(0x10)] = g_hexCharTable[1];
        g_bgPathTemplate[GAME_DATA_PATH_IDX(0x11)] = g_hexCharTable[1];
        g_bgPathTemplate[GAME_DATA_PATH_IDX(0x12)] = g_hexCharTable[g_roomCameraId];
        g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0f)] = g_hexCharTable[3];
        g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0b)] = g_bgPathTemplate[GAME_DATA_PATH_IDX(0x0f)];
        LoadFile(g_bgPathTemplate, g_bgPakLoadBuffer, 2);
        pakData = g_bgPakLoadBuffer;
    } else {
        // 0x00462a5?: g_bgCacheBuffer + (&DAT_00aea090)[g_roomCameraId]. The
        // original reads this table from 0x00aea090 while load_room_bg WRITES it
        // from 0x00aea08c with a post-incremented index - two bases 4 bytes apart
        // that cancel out. The port keeps one array, so the +1 has to be explicit
        // here. Without it every camera showed the PREVIOUS camera's background
        // (camera 6 rendered image 5) and camera 0 only looked correct because
        // slot 0 is never written and happens to be 0.
        pakData = &g_bgCacheBuffer[g_bgCameraOffsets[g_roomCameraId + 1]];
    }

    unpack_pakfile_(pakData, g_TimImageBuffer);

    int width, height;
    if ((g_main_state_flags2 & MSF2_SCREEN_BORDER) == 0) {
        width = 320;
        height = 240;
    } else {
        width = 316;
        height = 236;
    }

    display_image(8, g_TimImageBuffer__bitmap, width, height);
    title_setup_texture_pages(8, 1);
    // empty_00470960(8): empty in the original - call dropped
}

// ============================================================================
// load_room_bg_masks (0x004759d0) - Load room background masks for all cameras
// Masks are depth/occlusion data used for sprite rendering against the
// pre-rendered backgrounds.
// ============================================================================
void load_room_bg_masks(void) // 0x004759d0
{
    int totalSize = 0;
    int camCounter = 0;
    int camOffset = 0;
    int* offsetPtr = g_bgMaskOffsets;

    if (g_RdtPointer->cameras_count != 0) {
        do {
            RDT* pRdt = g_RdtPointer;
            *offsetPtr = totalSize;

            RDT_Camera* cameras = (RDT_Camera*)((char*)pRdt + sizeof(RDT));
            int* maskPointer = (int*)cameras[camCounter].mask_pointer;

            int fileSize;
            // Nothing to preload for an arrange room: there is no objspr file
            // for STAGE8-E (the DC disc has no such folder, and the PC only
            // shipped stages 1-7), and none is needed - load_room_masks reads
            // that camera's mask TIM straight out of the RDT instead. Naming
            // the base room's pak here would load a different camera set's art.
            if (*maskPointer == 0 || room_file_stage() >= STAGE_ARRANGE_FIRST) {
                fileSize = 0;
            } else {
                g_maskPathTemplate[GAME_DATA_PATH_IDX(0x11)] = (char)(g_stageId + 0x30);
                if (get_stage_id() > 4) {   // see load_room_bg
                    g_maskPathTemplate[GAME_DATA_PATH_IDX(0x11)] = (char)(g_stageId + 0x2b);
                }
                g_maskPathTemplate[GAME_DATA_PATH_IDX(0x12)] = (char)(g_roomId / 10 + 0x30);
                g_maskPathTemplate[GAME_DATA_PATH_IDX(0x13)] = (char)(g_roomId % 10 + 0x30);
                g_maskPathTemplate[GAME_DATA_PATH_IDX(0x14)] = (char)(camCounter + '0');

                fileSize = (int)LoadFile(g_maskPathTemplate, &g_bgMaskDataBuffer[totalSize], 0x20);
                // LoadFile answers a miss with (size_t)-1. Added to totalSize
                // that walks the write cursor BACKWARDS through the buffer and
                // leaves later cameras with negative offsets, so the next room
                // unpacks whatever lies before it. A miss is no mask.
                if (fileSize < 0) {
                    dbg_printf("[masks] %s missing - camera %d draws without its"
                               " occlusion layer\n", g_maskPathTemplate, camCounter);
                    fileSize = 0;
                }
            }

            totalSize += fileSize;
            camOffset += 0x2C;
            offsetPtr++;
            camCounter++;
        } while (camCounter < (int)(unsigned int)g_RdtPointer->cameras_count);
    }
}
