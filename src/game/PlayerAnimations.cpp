// PlayerAnimations.cpp - Player animation state machines (decompiled from Ghidra)
#include "../Globals.h"
#include "../marni/MarniSystem.h"
#include <cstdio>
#include <cstdlib>
#include "../DebugPrint.h"
#include "entities/EntityCommon.h"
#include "dc/Items.h"             // lockpick/ammo item ids the DC moved

// ============================================================================
// Player animation function stubs (populated into g_playerAnimFunctions by set_player_animations_functions)
// These are dispatched by FUN_00495290 based on g_playerEntity.animFrameId
// Full implementations to be decompiled from Ghidra.
// ============================================================================
// 0x00437a80
void player_anim_attack_recoil(void) {
    char cVar1;
    switch ((unsigned int)g_playerEntity.action_state) {
    case 0:
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.unk_8c = 0;
        g_playerEntity.action_state = 1;
        g_playerEntity.scaMatrixData.localMatrix.t[0] = (int)*(unsigned short*)((char*)ENTITY + 0xC6);
        g_playerEntity.scaMatrixData.localMatrix.t[2] = (int)*(unsigned short*)((char*)ENTITY + 0xC8);
        Play3DSnd(3, 0, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        g_playerEntity.flags |= 2;
    case 1:
        entity_apply_anim_vertex((Entity*)&g_playerEntity, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2);
        cVar1 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        if (cVar1 != 0) {
            g_playerEntity.action_state = 2;
            g_playerEntity.unk_bf = 0;
            g_playerEntity.attackAnim++;
            return;
        }
        break;
    case 2:
        entity_apply_anim_vertex((Entity*)&g_playerEntity, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2);
        Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        return;
    case 3:
        g_playerEntity.attackAnim++;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.action_state = 4;
    case 4:
        entity_apply_anim_vertex((Entity*)&g_playerEntity, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2);
        cVar1 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        if (cVar1 != 0) {
            g_playerEntity.animationId = 1;
            g_playerEntity.animFrameId = 0;
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state = 0;
            g_playerEntity.flags &= 0xfd;
            if ((g_playerEntity.attackAnim == 2) || (g_playerEntity.attackAnim == 8)) {
                g_playerEntity.directionAngle += 0x800;
            }
            g_playerEntity.isBeingAttackedFlag = 0;
            if (g_playerEntity.attackTimer == 0) {
                g_playerEntity.attackTimer = 0x96;
            }
        }
    }
}
// 0x0049abb0
void player_anim_simple_recovery(void) {
    char cVar1;
    if (g_playerEntity.action_state > 1) return;
    if (g_playerEntity.action_state == 0) {
        g_playerEntity.action_state = 1;
        g_playerEntity.move_speed_current = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.unk_8c = 0;
        g_playerEntity.attackAnim = 2;
        Play3DSnd(3, 2, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        return;
    }
    if (g_playerEntity.animation_frame_id == 0xf) {
        PlayEntitySnd(2);
    }
    if (g_playerEntity.health >= 0) {
        entity_apply_anim_vertex((Entity*)&g_playerEntity, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2);
    }
    cVar1 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x200);
    if (cVar1 != 0) {
        g_playerEntity.directionAngle += 0x800;
        g_playerEntity.action_state++;
    }
}
// 0x00430130
void player_anim_multi_attack(void) {
    char cVar1;
    switch ((unsigned int)g_playerEntity.action_state) {
    case 0:
        g_playerEntity.action_state = 1;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.move_speed_current = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 0;
    case 1:
        cVar1 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        g_playerEntity.action_state += cVar1;
        break;
    case 2:
        g_playerEntity.attackAnim = 1;
        g_playerEntity.action_state = 3;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
    case 3:
        Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        break;
    case 4:
        g_playerEntity.attackAnim = 2;
        g_playerEntity.action_state = 5;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
    case 5:
        cVar1 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        if (cVar1 != 0) {
            g_playerEntity.animationId = 1;
            g_playerEntity.animFrameId = 0;
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state = 0;
            g_playerEntity.isBeingAttackedFlag = 0;
        }
    }
    if ((int)(unsigned int)g_playerEntity.animation_frame_id <= (int)((unsigned int)(g_playerEntity.id & 1) * -4 + 10)) {
        EntityUpdateWeaponJoint(0);
        return;
    }
    EntityUpdateWeaponJoint(1);
}
// 0x004196d0 — Death animation with billboards (5 states)
void player_anim_dispatch_4c2ac8(void) {
    JointStruct* pJVar1;
    char cVar2;
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.action_state = 1;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = g_playerEntity.isBeingAttackedFlag - 1;
        g_playerEntity.unk_bc = 0xb4;
        g_playerEntity.unk_8c = 3;
        g_message_flags &= 0xffbf;
        g_playerEntity.jointsStructs[1].flags |= 8;
        pJVar1 = g_playerEntity.jointsStructs;
        {
            int* deadData = (int*)g_deadMoveValue;
            g_playerPosScratch.x = deadData[5];
            g_playerPosScratch.y = deadData[6];
            g_playerPosScratch.z = deadData[7];
            g_playerPosScratch.pad = deadData[8];
        }
        Effect_CreateBillboard(0, 3, 0, &pJVar1[1].world, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 3, 0, (void*)g_deadMoveValue, &pJVar1[1].world.t, 0);
        g_playerEntity.health = -1;
        // fall through
    case 1:
        pJVar1 = g_playerEntity.jointsStructs;
        if (ENTITY->animation_frame_id < 10) {
            int* deadData = (int*)g_deadMoveValue;
            g_playerPosScratch.x = deadData[5];
            g_playerPosScratch.z = deadData[7];
            g_playerPosScratch.pad = deadData[8];
            g_playerPosScratch.y = -0x898;
            Effect_CreateBillboard(0, 0, 0, (void*)((char*)ENTITY + 0x20), &g_playerPosScratch, 0);
        }
        if (ENTITY->animation_frame_id == 3) {
            JointApplyColorTint(g_playerEntity.jointsStructs, 0x30, 0x80820, &DAT_00606060);
            JointApplyColorTint(pJVar1 + 9, 0x30, 0x80820, &DAT_00606060);
            JointApplyColorTint(pJVar1 + 12, 0x30, 0x80820, &DAT_00606060);
        }
        if (ENTITY->animation_frame_id == 0x2a) {
            PlayEntitySnd(2);
        }
        cVar2 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        g_playerEntity.action_state += cVar2;
        EntityUpdateWeaponJoint(0);
        break;
    case 2: {
        g_svecScratch.y = 0;
        g_svecScratch.z = 0;
        g_svecScratch.x = -900;
        {
            int* src = (int*)g_deadMoveValue;
            int* dst = (int*)&g_matrixScratch;
            for (int i = 0; i < 8; i++) {
                dst[i] = src[i];
            }
        }
        RotMatrixY((int)(unsigned short)g_playerEntity.directionAngle, &g_matrixScratch);
        ApplyMatrixSV(&g_matrixScratch, &g_svecScratch, &g_svecScratch);
        ENTITY->pushVelocity.x = ENTITY->pushVelocity.x + g_svecScratch.x;
        ENTITY->pushVelocity.y = ENTITY->pushVelocity.y + g_svecScratch.y;
        ENTITY->pushVelocity.z = ENTITY->pushVelocity.z + g_svecScratch.z;
        BillboardSetColor(&ENTITY->pushVelocity, 1, 2, 0x00ffff50);
        BillboardAdjSize(&ENTITY->pushVelocity, 0xffffff38, 0xffffff38);
        g_playerEntity.action_state = 3;
        g_playerEntity.isBeingAttackedFlag = 0x80;
        return;
    }
    case 3:
        BillboardAdjSize(&ENTITY->pushVelocity, 0x10, 0x10);
        if (g_playerEntity.unk_bc == 0xa0) {
            g_fade_type_id = 1;
            g_fading_counter = 0x100;
            fade_update();
        }
        g_playerEntity.unk_bc--;
        if (g_playerEntity.unk_bc == 0x20) {
            g_playerEntity.animationId = 4;
            return;
        }
        break;
    case 4:
        g_playerEntity.animationId = 4;
        return;
    }
}
// 0x0048f060
void player_anim_crawling(void) {
    char cVar1;
    if (g_playerEntity.action_state == 0) {
        g_playerEntity.action_state = 1;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.move_speed_current = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 0;
    } else if (g_playerEntity.action_state != 1) {
        return;
    }
    entity_apply_anim_vertex((Entity*)&g_playerEntity, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2);
    cVar1 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
    if (cVar1 != 0) {
        g_playerEntity.isBeingAttackedFlag = 0;
        g_playerEntity.animationId = 1;
        g_playerEntity.animFrameId = 0;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 0;
    }
}
// ============================================================================
// player_anim_set_attacked_flag (0x00469400) and its three branches.
//
// 0x00469400 is nothing but `JMP [action_behavior*4 + 0x004c2ac8]` - the
// compiler's jump table for a three-case switch, NOT a data table of installable
// handlers. The port modelled it as the latter and left 0x004c2ac8 an
// all-NULL array, so this whole state machine did nothing.
//
// That machine is how a grabbed player RECOVERS. Plant 42 drops the player with
// animationId 6 / animFrameId 8 / action_behavior 2, which routes here (state 6
// -> player_dispatch_anim_fn(8 + 0x13) -> entry 27), and behaviour 2 runs the
// knockdown, then hands off to behaviour 0, whose state 4 and 0x0B are the only
// places `isBeingAttackedFlag` is cleared and `animationId` returns to 1. With
// the table empty the player stayed frozen mid-pose with input suppressed -
// a softlock after Plant 42 released Chris in room 40C0.
//
// Note these use the player's DAMAGE animation pointers at +0x16C/+0x170
// (emdScratchPtr1/2), not the ordinary animHeader/animBase at +0x90/+0x94, and
// the animation INDEX Joint_move reads for the player is `attackAnim` (+0xBD).
// ============================================================================

// 0x004c2a0c - Plant 42's capture matrix t[0], doubling as the shared knock-back
// facing (see Plant42.cpp).
extern MATRIX g_plant42CaptureMatrix;

static void player_anim_effects_at_joints(int a, int b, int c, int d,
                                          unsigned char type, unsigned char variant,
                                          int xBias)
{
    JointStruct* j = g_playerEntity.jointsStructs;
    VECTOR* dead = (VECTOR*)((uintptr_t)g_deadMoveValue + 0x14);
    g_playerPosScratch = *dead;
    g_playerPosScratch.x += xBias;
    Effect_CreateBillboard(type, variant, 0, &j[a].world, &g_playerPosScratch, 0);
    Effect_CreateBillboard(type, variant, 0, &j[b].world, &g_playerPosScratch, 0);
    if (c >= 0) Effect_CreateBillboard(type, variant, 0, &j[c].world, &g_playerPosScratch, 0);
    if (d >= 0) Effect_CreateBillboard(type, variant, 0, &j[d].world, &g_playerPosScratch, 0);
}

// 0x00469410 - action_behavior 0: the fall / slide / get-up chain. Its states 4
// and 0x0B are what give the player back to the controller.
static void player_anim_knockdown_recover(void)
{
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.action_state = 1;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 6;
        g_playerEntity.unk_8c = 4;
        g_playerEntity.move_speed_current = 1000;
        Play3DSnd(3, 2, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t[0]);
        // fall through
    case 1: {
        g_playerEntity.move_speed_current =
            (unsigned short)(g_playerEntity.move_speed_current +
                             (unsigned short)g_playerEntity.animation_frame_id * (unsigned short)-0xf);
        g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state +
            (char)Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400));
        Add_speedXZ(0x800);
        // The collision test is probe-only here: the position is restored
        // afterwards and only the hit/miss result is kept.
        unsigned int savedM0 = *(unsigned int*)&g_playerEntity.scaMatrixData.worldMatrix.m[0][0];
        int t0 = g_playerEntity.scaMatrixData.localMatrix.t[0];
        int t1 = g_playerEntity.scaMatrixData.localMatrix.t[1];
        int t2 = g_playerEntity.scaMatrixData.localMatrix.t[2];
        unsigned char hit = check_room_collision(
            (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t,
            *(short*)(g_playerEntity.Sca_info + 10));
        g_playerDisplacement = (int)hit;
        g_playerEntity.scaMatrixData.localMatrix.t[0] = t0;
        g_playerEntity.scaMatrixData.localMatrix.t[1] = t1;
        g_playerEntity.scaMatrixData.localMatrix.t[2] = t2;
        *(unsigned int*)&g_playerEntity.scaMatrixData.worldMatrix.m[0][0] = savedM0;
        if (g_playerDisplacement != 0) {
            g_playerEntity.action_state = 5;
            return;
        }
        break;
    }
    case 2:
        g_playerEntity.action_state = 3;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 7;
        Play3DSnd(2, 0x1d, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t[0]);
        // fall through
    case 3:
        if ((g_playerEntity.animation_frame_id & 1) == 0 &&
            g_playerEntity.animation_frame_id < 10) {
            player_anim_effects_at_joints(5, 8, -1, -1, 9, 0x16, 0);
        }
        g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state +
            (char)Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400));
        Add_speedXZ(0x800);
        g_playerEntity.move_speed_current = (unsigned short)(g_playerEntity.move_speed_current - 0xf);
        if ((short)g_playerEntity.move_speed_current < 0) {
            g_playerEntity.move_speed_current = 0;
            return;
        }
        break;
    case 4:
        g_playerEntity.animationId = 1;
        g_playerEntity.animFrameId = 0;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 0;
        g_playerEntity.isBeingAttackedFlag = 0;
        return;
    case 5:
        g_playerEntity.action_state = 6;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 4;
        g_playerEntity.unk_8c = 3;
        Play3DSnd(2, 0x1a, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t[0]);
        Play3DSnd(3, 2, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t[0]);
        player_anim_effects_at_joints(5, 8, -1, -1, 9, 0x16, -400);
        player_anim_effects_at_joints(0, 3, 6, -1, 9, 0x11, -400);
        // fall through
    case 6:
        g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state +
            (char)Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400));
        return;
    case 7:
        g_playerEntity.action_state = 8;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 5;
        g_playerEntity.unk_8c = 3;
        // fall through
    case 8:
        g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state +
            (char)Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400));
        return;
    case 9:
        g_playerEntity.action_state = 10;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 4;
        g_playerEntity.unk_8c = 3;
        // fall through
    case 10:
        g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state +
            (char)Joint_move(1, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400));
        break;
    case 0x0B:
        g_playerEntity.animationId = 1;
        g_playerEntity.animFrameId = 0;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 0;
        g_playerEntity.isBeingAttackedFlag = 0;
        g_playerEntity.flags &= 0xfd;
        return;
    default:
        break;
    }
}

// 0x00469840 - action_behavior 1: held/grabbed, driven entirely by the grabber.
static void player_anim_grabbed(void)
{
    JointStruct* joints = g_playerEntity.jointsStructs;

    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.action_state = 1;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 0;
        // fall through
    case 1:
        g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state +
            (char)Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400));
        return;
    case 2:
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 1;
        g_playerEntity.action_state = 3;
        g_playerEntity.unk_8c = 3;
        // fall through
    case 3:
        Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        return;
    case 4:
        g_playerEntity.action_state = 5;
        g_playerEntity.attackAnim = 2;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        // fall through
    case 5:
        Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        return;
    case 6: {
        JointStruct* head = joints + 2;
        head->flags |= 0x0c;
        g_playerEntity.action_state = 7;
        g_playerPosScratch = *(VECTOR*)((uintptr_t)g_deadMoveValue + 0x14);
        Effect_CreateBillboard(0, 3, 0, &joints[2].world, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 0, 0, NULL, joints[0].world.t, 0);
        JointApplyColorTint(head, 0x30, 0x80820, (void*)0x00606060);
        JointApplyColorTint(head, 0x30, 0x80820, (void*)0x00606060);
        return;
    }
    default:
        return;
    }
}

// 0x004699d0 - action_behavior 2: thrown/dropped. Hands over to behaviour 0.
static void player_anim_thrown(void)
{
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.action_state = 1;
        g_playerEntity.attackAnim = 3;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        if (g_playerEntity.health < 0) {
            Play3DSnd(3, 3, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t[0]);
        }
        // fall through
    case 1:
        if (g_playerEntity.animation_frame_id == 1) {
            player_anim_effects_at_joints(4, 7, 0, 2, 9, 0x11, 0);
        }
        if (g_playerEntity.animation_frame_id == 0x0c) {
            g_playerEntity.action_state = 2;
            g_playerEntity.attackDirection = 0x5a;
            if (g_playerEntity.health < 0) {
                g_playerEntity.action_state = 8;
            }
        }
        Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        break;
    case 2: {
        short pressed = GetPlayerInputMasked();
        // Mashing a button shortens the time on the floor: 4 per frame instead of 1.
        g_playerEntity.attackDirection = (unsigned short)(g_playerEntity.attackDirection -
            ((unsigned short)(pressed != 0) * 3 + 1));
        if ((short)g_playerEntity.attackDirection < 0) {
            g_playerEntity.action_state = 3;
            return;
        }
        break;
    }
    case 3:
        g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state +
            (char)Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400));
        return;
    case 4:
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 7;
        return;
    case 5:
        g_playerEntity.action_state = 6;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 3;
        g_playerEntity.unk_8c = 7;
        g_playerEntity.attackDirection = 0;
        g_playerEntity.move_speed_current = 500;
        Play3DSnd(3, 1, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t[0]);
        Play3DSnd(2, 0x19, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t[0]);
        // fall through
    case 6:
        if (g_playerEntity.animation_frame_id == 5 || g_playerEntity.animation_frame_id == 7) {
            player_anim_effects_at_joints(5, 8, 0, 2, 9, 0x11, 0);
        }
        g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state +
            (char)Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x200));
        if (g_playerEntity.animation_frame_id < 0x0f) {
            // Slide along the knock-back facing the attacker stashed at
            // 0x004c2a0c, then restore the real facing.
            g_playerDisplacement = (int)g_playerEntity.directionAngle;
            g_playerEntity.directionAngle = (short)g_plant42CaptureMatrix.t[0];
            Add_speedXZ(0);
            short step = (short)g_playerEntity.attackDirection;
            g_playerEntity.attackDirection = (unsigned short)(g_playerEntity.attackDirection + 1);
            g_playerEntity.directionAngle = (short)g_playerDisplacement;
            g_playerEntity.move_speed_current =
                (unsigned short)(g_playerEntity.move_speed_current + step * -5);
            return;
        }
        break;
    case 7:
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 7;
        return;
    default:
        break;
    }
}

// 0x00469400 - `JMP [action_behavior*4 + 0x004c2ac8]`. Three entries; the
// original applies no bound, this one reports instead of jumping into the pulse
// table that follows.
void player_anim_set_attacked_flag(void) {
    // Original switch (0x004c2ac8 jumptable) cases are separate Ghidra
    // functions; the port folds each into the handlers below. The inner
    // grabbed/thrown sub-state machines also had their own case splits:
    //   grabbed:  caseD_0 0x00469859  caseD_2 0x0046989d
    //             caseD_4 0x004698d9  caseD_6 0x00469919
    //   thrown:   caseD_0 0x004699e9  caseD_2 0x00469b0b  caseD_3 0x00469b37
    //             caseD_4 0x00469b5c  caseD_5 0x00469b68
    switch (g_playerEntity.action_behavior) {
    case 0: player_anim_knockdown_recover(); break;   // 0x00469410
    case 1: player_anim_grabbed();           break;   // 0x00469840
    case 2: player_anim_thrown();            break;   // 0x004699d0
    default: {
        static int lastReported = -1;
        if ((int)g_playerEntity.action_behavior != lastReported) {
            lastReported = (int)g_playerEntity.action_behavior;
            dbg_printf("[player] action_behavior=%u past the 3-entry 0x004c2ac8 jumptable\n",
                       (unsigned int)g_playerEntity.action_behavior);
        }
        break;
    }
    }
}
// 0x00468e10 — Limb physics with bouncing (7 states)
void player_anim_dispatch_4ba360(void) {
    JointStruct* pJVar5;
    char cVar6;
    short sVar7;
    short sVar8;

    pJVar5 = g_playerEntity.jointsStructs;
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.action_state = 1;
        g_playerEntity.attackAnim = 2;
        g_playerEntity.unk_8c = 3;
        pJVar5[0].velX = 0;
        pJVar5[0].velY = 0;
        pJVar5[0].velZ = 0;
        pJVar5[2].velX = 0;
        pJVar5[2].velY = 0;
        pJVar5[2].velZ = 0;
        pJVar5[2].rotation.x = 0;
        pJVar5[2].rotation.y = 0;
        pJVar5[2].rotation.z = 0;
        JointApplyColorTint(pJVar5, 0x30, 0x80820, &DAT_00606060);
        JointApplyColorTint(pJVar5 + 2, 0x30, 0x80820, &DAT_00606060);
        JointApplyColorTint(pJVar5 + 1, 0x30, 0x80820, &DAT_00606060);
        {
            int* deadData = (int*)g_deadMoveValue;
            g_playerPosScratch.x = *(int*)(deadData + 5);
            g_playerPosScratch.y = *(int*)(deadData + 6);
            g_playerPosScratch.z = *(int*)(deadData + 7);
            g_playerPosScratch.pad = *(int*)(deadData + 8);
        }
        Effect_CreateBillboard(0, 3, 0, &pJVar5[0].world, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 3, 0, &pJVar5[2].world, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0, 3, 0, &pJVar5[1].world, &g_playerPosScratch, 0);
        Play3DSnd(3, 3, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        Play3DSnd(4, 0, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        g_playerEntity.flags |= 4;
        g_playerEntity.attackDirection = 0xf;
        // fall through
    case 1:
        if (g_playerEntity.attackDirection != 0) {
            g_playerEntity.attackDirection--;
            Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        }
        pJVar5 = g_playerEntity.jointsStructs;
        sVar8 = pJVar5[0].rotation.z;
        if (-0x400 < sVar8) {
            pJVar5[0].rotation.z = sVar8 - pJVar5[0].velX;
            pJVar5[0].velX = pJVar5[0].velX + 8;
            RotMatrix(&pJVar5[0].rotation, &pJVar5[0].transform);
        }
        sVar8 = pJVar5[2].rotation.y;
        if ((sVar8 < 0x100) && ((((unsigned char*)&pJVar5[2].velX)[1] & 0x80) == 0)) {
            pJVar5[2].rotation.y = sVar8 + 0x20;
            pJVar5[2].rotation.x = pJVar5[2].rotation.x - pJVar5[2].velX;
            pJVar5[2].velX = pJVar5[2].velX + 2;
            RotMatrix(&pJVar5[2].rotation, &pJVar5[2].transform);
            MulMatrixInPlace(&pJVar5[2].transform, &pJVar5[2].world);
            if (0xff < pJVar5[2].rotation.y) {
                pJVar5[2].velX = (short)0x8002;
                {
                    int* deadData = (int*)g_deadMoveValue;
                    g_playerPosScratch.x = deadData[5];
                    g_playerPosScratch.y = deadData[6];
                    g_playerPosScratch.z = deadData[7];
                    g_playerPosScratch.pad = deadData[8];
                }
                Effect_CreateBillboard(0, 3, 0, &pJVar5[2].world, &g_playerPosScratch, 0);
            }
        }
        break;
    case 2:
        pJVar5 = g_playerEntity.jointsStructs;
        pJVar5[0].velX = (short)0x8002;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 0;
        g_playerEntity.action_state = 3;
        g_playerEntity.unk_8c = 0xf;
        {
            int* deadData = (int*)g_deadMoveValue;
            g_playerPosScratch.x = deadData[5];
            g_playerPosScratch.y = deadData[6];
            g_playerPosScratch.z = deadData[7];
            g_playerPosScratch.pad = deadData[8];
        }
        Effect_CreateBillboard(0, 3, 0, &pJVar5[2].world, &g_playerPosScratch, 0);
        g_playerPosScratch.y = 500;
        Effect_CreateBillboard(0, 3, 0, &pJVar5[0].world, &g_playerPosScratch, 0);
        // fall through
    case 3:
        Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x100);
        break;
    case 4:
        g_playerEntity.action_state = 5;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        // fall through
    case 5:
        cVar6 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        if (cVar6 != 0) {
            g_playerEntity.unk_bf = 0;
            g_playerEntity.attackAnim = 0;
            g_playerEntity.action_state = 6;
            g_playerEntity.unk_8c = 3;
        }
        break;
    case 6:
        Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x400);
        g_deathAnimationFlag = 1;
        break;
    }

    // Post-switch: physics for bouncing joints (runs every frame)
    pJVar5 = g_playerEntity.jointsStructs;

    // Joint 0 bounce check (when in-air flag set and position.y < 700)
    if (((((unsigned char*)&pJVar5[0].velX)[1] & 0x80) != 0) &&
        (g_playerEntity.scaMatrixData.localMatrix.t[1] < 700)) {
        g_playerEntity.health = -1;
        g_playerEntity.scaMatrixData.localMatrix.t[1] = g_playerEntity.scaMatrixData.localMatrix.t[1] + pJVar5[0].velY;
        sVar8 = pJVar5[0].velY;
        sVar7 = sVar8 + 2;
        pJVar5[0].velY = sVar7;
        if (0x20 < sVar7) {
            pJVar5[0].velY = sVar8 + 12;
        }
        if ((700 < g_playerEntity.scaMatrixData.localMatrix.t[1]) && ((pJVar5[0].velX & 7) != 0)) {
            g_playerEntity.scaMatrixData.localMatrix.t[1] = 0x28a;
            sVar8 = -pJVar5[0].velY;
            pJVar5[0].velY = sVar8;
            pJVar5[0].velY = (short)((int)((int)sVar8 + ((int)sVar8 >> 31 & 7U)) >> 3);
            pJVar5[0].velX--;
            g_playerEntity.action_state = 4;
            {
                int* deadData = (int*)g_deadMoveValue;
                g_playerPosScratch.x = deadData[5];
                g_playerPosScratch.z = deadData[7];
                g_playerPosScratch.pad = deadData[8];
                g_playerPosScratch.y = 500;
            }
            Effect_CreateBillboard(0, 3, 0, &pJVar5[0].world, &g_playerPosScratch, 0);
        }
        if ((((unsigned char)pJVar5[0].velX) & 7) != 2) {
            g_playerEntity.move_speed_current = 30;
            Add_speedXZ(0x800);
        }
        sVar8 = pJVar5[0].rotation.z;
        if (sVar8 < -0x3ff) {
            g_playerEntity.position.pad = 0;
            g_playerEntity.directionAngle = 0;
            g_playerEntity.speed.x = 0;
            pJVar5[0].rotation.x = 0;
            pJVar5[0].rotation.y = 0;
            pJVar5[0].rotation.z = -0x400;
        } else {
            pJVar5[0].rotation.z = sVar8 - pJVar5[0].velZ;
            pJVar5[0].velZ = pJVar5[0].velZ + 1;
            RotMatrix(&pJVar5[0].rotation, &pJVar5[0].transform);
        }
    }

    // Joint 2 bounce check (when in-air flag set and world.t[1] < -400)
    if (((((unsigned char*)&pJVar5[2].velX)[1] & 0x80) != 0) &&
        (pJVar5[2].world.t[1] < -400)) {
        pJVar5[2].world.t[1] = pJVar5[2].velY + pJVar5[2].world.t[1];
        sVar8 = pJVar5[2].velY;
        sVar7 = sVar8 + 2;
        pJVar5[2].velY = sVar7;
        if (0x20 < sVar7) {
            pJVar5[2].velY = sVar8 + 8;
            pJVar5[2].rotation.x = 0;
            pJVar5[2].rotation.y = 0;
        }
        if ((-400 < pJVar5[2].world.t[1]) && ((pJVar5[2].velX & 7) != 0)) {
            pJVar5[2].world.t[1] = -0x1c2;
            sVar8 = -pJVar5[2].velY;
            pJVar5[2].velY = sVar8;
            pJVar5[2].velY = (short)((int)((int)sVar8 + ((int)sVar8 >> 31 & 7U)) >> 3);
            pJVar5[2].velX--;
            {
                int* deadData = (int*)g_deadMoveValue;
                g_playerPosScratch.x = deadData[5];
                g_playerPosScratch.y = deadData[6];
                g_playerPosScratch.z = deadData[7];
                g_playerPosScratch.pad = deadData[8];
            }
            Effect_CreateBillboard(0, 3, 0, &pJVar5[2].world, &g_playerPosScratch, 0);
        }
        if (-0x400 < pJVar5[2].rotation.z) {
            pJVar5[2].rotation.x = 0;
            pJVar5[2].rotation.y = 0;
            pJVar5[2].rotation.z = pJVar5[2].rotation.z - pJVar5[2].velZ;
            pJVar5[2].velZ = pJVar5[2].velZ + 1;
            RotMatrix(&pJVar5[2].rotation, &pJVar5[2].world);
            return;
        }
        pJVar5[2].rotation.z = -0x400;
    }
}
// 0x0043b980 — 10-state crawl/death handler
void player_anim_dispatch_4c10b0(void) {
    char cVar1;
    short sVar2;
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.action_state = 1;
        g_playerEntity.attackAnim = 2;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.move_speed_current = 200;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        // fall through
    case 1:
        if (g_playerEntity.animation_frame_id < 0x24) {
            entity_apply_anim_vertex((Entity*)&g_playerEntity, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2);
        } else {
            Add_speedXZ(0);
        }
        cVar1 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        if (cVar1 != 0) {
            g_playerEntity.action_state = 2;
            return;
        }
        break;
    case 2:
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.isBeingAttackedFlag = 2;
        g_playerEntity.action_state = 3;
        g_playerEntity.attackAnim = 3;
        g_playerEntity.unk_8c = 3;
        // fall through
    case 3:
        if ((1 < g_playerEntity.isBeingAttackedFlag) && (3 < g_playerEntity.animation_frame_id)) {
            Play3DSnd(3, 2, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
            PlayEntitySnd(2);
            g_playerEntity.isBeingAttackedFlag = 1;
        }
        if ((g_playerEntity.health < 0) && (10 < g_playerEntity.animation_frame_id)) {
            PlayEntitySnd(2);
            g_playerEntity.action_state = 8;
            return;
        }
        sVar2 = GetPlayerInputMasked();
        g_playerEntity.attackDirection = g_playerEntity.attackDirection + (unsigned short)(sVar2 != 0) * (unsigned short)-3;
        if ((g_playerEntity.attackDirection < 0) &&
            ((cVar1 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400)), cVar1 != 0)) {
            g_playerEntity.action_state = 4;
            return;
        }
        cVar1 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        if (cVar1 != 0) {
            g_playerEntity.action_state = 4;
            return;
        }
        break;
    case 4:
        g_playerEntity.unk_8c = 3;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.action_state = 5;
        g_playerEntity.attackAnim = 5;
        // fall through
    case 5:
        sVar2 = GetPlayerInputMasked();
        g_playerEntity.attackDirection = g_playerEntity.attackDirection + (unsigned short)(sVar2 != 0) * (unsigned short)-3;
        if ((g_playerEntity.attackDirection < 0) &&
            ((cVar1 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400)), cVar1 != 0)) {
            g_playerEntity.action_state = 6;
            return;
        }
        cVar1 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        if (cVar1 != 0) {
            g_playerEntity.action_state = 6;
            return;
        }
        break;
    case 6:
        g_playerEntity.action_state = 7;
        g_playerEntity.attackAnim = 4;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        // fall through
    case 7:
        cVar1 = Joint_move(1, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
        if (cVar1 != 0) {
            if (-1 < g_playerEntity.health) {
                g_playerEntity.action_behavior = 0;
                g_playerEntity.action_state = 0;
                g_playerEntity.isBeingAttackedFlag = 0;
                g_playerEntity.animationId = 1;
                g_playerEntity.animFrameId = 0;
                return;
            }
            g_playerEntity.action_state = 8;
            return;
        }
        break;
    case 8:
        Play3DSnd(3, 3, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        g_playerEntity.action_state = 9;
        g_playerEntity.attackDirection = 0x5a;
        BillboardSetColor(&g_playerEntity.pushVelocity, 1, 2, 0x00ffff50);
        BillboardSetSize(&g_playerEntity.pushVelocity, 0, 0);
        // fall through
    case 9:
        BillboardAdjSize(&g_playerEntity.pushVelocity, 0x14, 0x14);
        sVar2 = g_playerEntity.attackDirection;
        g_playerEntity.attackDirection = g_playerEntity.attackDirection - 1;
        if (sVar2 == 0) {
            g_playerEntity.action_state = 10;
        }
        break;
    }
}
// 0x004401c0
void player_anim_poison_death(void) {
    if (g_playerEntity.action_state == 0) {
        g_playerEntity.action_state = 1;
        g_playerEntity.attackAnim = 0;
        g_playerEntity.move_speed_current = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.isBeingAttackedFlag = 1;
        Play3DSnd(3, 0, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
    }
    Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
}
// 0x00440230
void player_anim_death_billboard(void) {
    if (g_playerEntity.action_state == 0) {
        g_playerEntity.action_state = 1;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 3;
        g_playerEntity.isBeingAttackedFlag = 1;
        g_playerEntity.unk_8c = 0;
    } else if (g_playerEntity.action_state == 1) {
        Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        if (g_playerEntity.animation_frame_id == 14) {
            g_playerEntity.animation_frame_id = 13;
        }
    } else if (g_playerEntity.action_state == 2) {
        g_playerEntity.zoneFlags |= 0x80;
        // 0x00be6484 = g_EnemiesList[0].scaMatrixData.localMatrix (enemy base 0x00be6464 + 0x20)
        // 0x00be6304 = g_playerEntity.scaMatrixData.localMatrix  (player base 0x00be62e4 + 0x20)
        extern MATRIX neptune_capture_matrix;   // 0x004bc988 (Neptune.cpp)
        RotMatrix(reinterpret_cast<SVECTOR*>(&g_EnemiesList[0].position.pad),
                  &g_EnemiesList[0].scaMatrixData.localMatrix);
        ApplyLVAndMul0Matrix(&g_EnemiesList[0].scaMatrixData.localMatrix,
                             &g_EnemiesList[0].jointsStructs->transform,
                             &g_matrixScratch);
        ApplyLVAndMul0Matrix(&g_matrixScratch, &neptune_capture_matrix,
                             &g_playerEntity.scaMatrixData.localMatrix);
    }
}
void player_anim_limb_physics(void) {         // 0x00424fb0 - dispatch via DAT_004ba360[action_behavior]
    // The Tyrant player-hit reactions; the table is defined in Tyrant.cpp next
    // to the rest of the Tyrant's 0x004ba2xx-0x004ba3xx data block. Four slots,
    // the last NULL - the original indexes it unbounded, so bound it here.
    extern void* DAT_004ba360[4];
    if (g_playerEntity.action_behavior >= 4) {
        static int lastReported = -1;
        if ((int)g_playerEntity.action_behavior != lastReported) {
            lastReported = (int)g_playerEntity.action_behavior;
            dbg_printf("[player] DAT_004ba360[%u] out of range (Tyrant hit reaction)\n",
                       (unsigned int)g_playerEntity.action_behavior);
        }
        return;
    }
    void (*func)(void) = (void(*)(void))DAT_004ba360[g_playerEntity.action_behavior];
    if (func) func();
}
// 0x00424de0
void player_anim_enemy_interact(void) {
    if (g_playerEntity.action_state == 0) {
        g_playerEntity.action_state = 1;
        g_playerEntity.attackAnim = 2;
        g_playerEntity.isBeingAttackedFlag = 0x80;
        g_playerEntity.flags |= 6;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.unk_8c = 0;
    } else if (g_playerEntity.action_state == 1) {
        if (g_playerEntity.animation_frame_id == 8) {
            Play3DSnd(3, 3, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
            JointStruct* joints = g_playerEntity.jointsStructs;
            JointApplyColorTint(joints,      0x30, 0x80820, &DAT_00606060);
            JointApplyColorTint(joints + 1,  0x30, 0x80820, &DAT_00606060);
            JointApplyColorTint(joints + 2,  0x30, 0x80820, &DAT_00606060);
            JointApplyColorTint(joints + 9,  0x30, 0x80820, &DAT_00606060);
            JointApplyColorTint(joints + 12, 0x30, 0x80820, &DAT_00606060);
        }
        {
            // g_deadMoveValue fields: 0x14/0x18/0x1c/0x20 = deadData[5..8]
            int* deadData = (int*)g_deadMoveValue;
            if (g_playerEntity.animation_frame_id < 9) {
                g_playerPosScratch.x = deadData[5];
                g_playerPosScratch.z = deadData[7];
                g_playerPosScratch.pad = deadData[8];
                g_playerPosScratch.y = -0x5dc;
                Effect_CreateBillboard(0, 0, 0,
                                       &g_playerEntity.scaMatrixData.localMatrix,
                                       &g_playerPosScratch, 0);
            }
            if (g_playerEntity.animation_frame_id > 0x5f) {
                g_playerPosScratch.x = deadData[5];
                g_playerPosScratch.y = deadData[6];
                g_playerPosScratch.z = deadData[7];
                g_playerPosScratch.pad = deadData[8];
                Effect_CreateBillboard(0, 0, 0,
                                       &g_playerEntity.scaMatrixData.localMatrix,
                                       &g_playerPosScratch, 0);
            }
        }
        entity_apply_anim_vertex((Entity*)&g_playerEntity, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2);
        char cVar2 = Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        g_playerEntity.action_state += cVar2;
    } else if (g_playerEntity.action_state == 2) {
        g_playerEntity.health = -1;
    }
}
// 0x00408900 - the body of player_anim_death_alt.  The original reached it
// through DAT_004b1a90, a one-entry jump table at 0x004b1a90 sitting between
// s_yawnRepositionPath and s_yawnDustOffset in Yawn's data block
// (JMP dword ptr [action_behavior*4 + 0x4b1a90], single entry -> 0x00408900),
// so no array is needed here.
//
// This is the swallowed player's own state machine.
//
// States, keyed on action_state (0x87):
//   0: init (carried pose set), fall into the animation
//   1: advance the carried-in-mouth animation (index = attackAnim) over the
//      damage pointers emdScratchPtr1/emdScratchPtr2; releases the player back
//      to animationId 1 if Yawn (g_EnemiesList[0], the head slot) is dead
//   2: hold the player in the jaws - recompose the player matrix from Yawn's
//      head-joint world matrix and the shared capture matrix every frame
static void player_anim_death_alt_body(void)  // 0x00408900
{
    // 0x004b19c0 - shared with Yawn.cpp (yawn_action_swallow)
    extern MATRIX g_yawnCaptureMatrix;

    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.action_state        = 1;
        g_playerEntity.animation_frame_id  = 0;
        g_playerEntity.unk_bf              = 0;
        g_playerEntity.isBeingAttackedFlag = 1;
        g_playerEntity.attackAnim          = 2;
        g_playerEntity.unk_8c              = 3;
        // fall through
    case 1:
        g_playerEntity.action_state += Joint_move(0, g_playerEntity.emdScratchPtr1,
                                                  g_playerEntity.emdScratchPtr2, 0x400);
        if (g_EnemiesList[0].health < 0) {
            // One DWORD store at 0x00be6368: animationId=1 and clears
            // animFrameId / action_behavior / action_state.
            g_playerEntity.animationId     = 1;
            g_playerEntity.animFrameId     = 0;
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state    = 0;
            g_playerEntity.flags &= 0xf9;
            g_playerEntity.isBeingAttackedFlag = 0;
        }
        break;

    case 2:
        g_playerEntity.zoneFlags |= 0x80;
        ApplyLVAndMul0Matrix(&g_EnemiesList[0].jointsStructs[0].world,
                             &g_yawnCaptureMatrix,
                             &g_playerEntity.scaMatrixData.localMatrix);
        break;
    }
}
void player_anim_death_alt(void) {            // 0x004088f0 - JMP [action_behavior*4 + 0x004b1a90]
    if (g_playerEntity.action_behavior == 0) { // the table's single entry (0x00408900)
        player_anim_death_alt_body();
    }
}
void player_anim_dispatch_4b1a90(void) {      // 0x0045c460 - dispatch via DAT_004c10b0[action_state]
    // DAT_004c10b0 is defined in MonsterPlant.cpp and has FOUR slots (three
    // real handlers plus the original's trailing NULL). The original JMPs
    // through it with an unchecked byte; bound it here so a stray action_state
    // is a dropped frame rather than a wild jump.
    extern void* DAT_004c10b0[];
    unsigned int idx = g_playerEntity.action_state;
    if (idx >= 4) return;
    void (*func)(void) = (void(*)(void))DAT_004c10b0[idx];
    if (func) func();
}

// ============================================================================
// entity_extract_anim_vertex - Extract animation frame vertex position into gSVector
// 0x0048bb60
// Reads animation data from EMD scratch pointers, advances animation timing
// on the ENTITY global, and extracts a vertex position (X,Y,Z) into
// g_svecScratch (gSVector).
// ============================================================================
void entity_extract_anim_vertex(Entity* entity, unsigned int emdScratch1, unsigned int emdScratch2, char reverse)
{
    // emdScratch1 points to animation header: [+2] = vertex stride, [+6] = vertex count
    // emdScratch2 points to animation frame table (indexed by animationId)
    short headerStride = *(short*)(emdScratch1 + 2);
    short headerCount  = *(short*)(emdScratch1 + 6);

    unsigned char* frameIdPtr = &ENTITY->animation_frame_id;
    unsigned short* animSlot = (unsigned short*)(emdScratch2 + (unsigned int)entity->animationId * 4);

    // Save current frame id (will be restored at the end)
    g_animFrameIdSave = (unsigned int)*frameIdPtr;

    // Advance animation timing if timing_control > 1
    if (1 < (unsigned char)ENTITY->timing_control) {
        *frameIdPtr = *frameIdPtr - 1;
        if (ENTITY->animation_frame_id == 0xFF) {
            ENTITY->animation_frame_id = (char)*animSlot - 1;
        }
    }

    // Calculate base of frame data
    unsigned int frameDataBase = emdScratch2 + (animSlot[1] & 0xFFFFFFFC);

    // Get frame entry pointer based on direction
    unsigned short* frameEntry;
    if (reverse == 0) {
        frameEntry = (unsigned short*)(frameDataBase + (unsigned int)entity->animation_frame_id * 4);
    } else {
        frameEntry = (unsigned short*)(frameDataBase - 4 + ((unsigned int)*animSlot - (unsigned int)entity->animation_frame_id) * 4);
    }

    // Calculate vertex address in animation data
    // align4(headerStride) * 4 = round headerStride down to multiple of 4
    short aligned = (short)(((int)headerStride + ((int)headerStride >> 31 & 3)) >> 2);
    unsigned int vertexAddr = emdScratch1 + (int)aligned * 4 +
                              (unsigned int)*frameEntry * ((int)headerCount / 2 & 0xFFFF) * 2;

    // Extract vertex position (offsets +6, +8, +10 from vertex data)
    g_svecScratch.x = *(short*)(vertexAddr + 6);
    g_svecScratch.y = *(short*)(vertexAddr + 8);
    g_svecScratch.z = *(short*)(vertexAddr + 10);

    // Restore saved frame id
    ENTITY->animation_frame_id = (unsigned char)g_animFrameIdSave;
}

// ============================================================================
// entity_apply_anim_vertex - Update entity transform from animation vertex offset
// 0x00489fa0
// Extracts the current animation vertex, rotates it by the entity's Y angle,
// and sets the entity's transform translation (X, Z) from the result plus
// the entity's base position offsets.
// ============================================================================
void entity_apply_anim_vertex(Entity* entity, unsigned int emdScratch1, unsigned int emdScratch2)
{
    // 0x00489fbe - Extract animation vertex position into g_svecScratch
    entity_extract_anim_vertex(entity, emdScratch1, emdScratch2, 0);

    // 0x00489fc6 - Copy identity matrix to scratch matrix
    g_matrixScratch = g_identityMatrixData;

    // 0x00489fd7 - Rotate scratch matrix by entity Y angle
    RotMatrixY((int)(short)entity->angle, &g_matrixScratch);

    // 0x00489fee - Apply rotation to vertex position
    ApplyMatrixSV(&g_matrixScratch, &g_svecScratch, &g_svecScratch);

    // 0x00489fff - Set entity transform translation from base offset + rotated vertex
    entity->scaMatrixData.localMatrix.t[0] = (unsigned int)entity->unk_c6 + (int)g_svecScratch.x;
    entity->scaMatrixData.localMatrix.t[2] = (unsigned int)entity->unk_c8 + (int)g_svecScratch.z;
}

// ============================================================================
// JointSetColorTint (0x00485ac0)
// Sets vertex color tint on model data for a joint. Iterates through model
// vertex entries and applies an RGB color value scaled by a constant factor.
// ============================================================================
void JointSetColorTint(int modelObjPtr, unsigned int packedColor)
{
    unsigned char r = (unsigned char)(packedColor & 0xFF);
    unsigned char g = (unsigned char)((packedColor >> 8) & 0xFF);
    unsigned char b = (unsigned char)((packedColor >> 16) & 0xFF);

    if (*(int*)(modelObjPtr + 0x10) == 0) {
        int vertexDataPtr = *(int*)(modelObjPtr + 0x20);
        int vertexCount = *(int*)(vertexDataPtr + 0x4c0);
        if ((vertexCount & 0x7FFFFFFF) == 0) return;

        int entry = vertexDataPtr + 0x4d0;
        unsigned int idx = 0;
        do {
            *(unsigned int*)(entry + 0x80) |= 2;
            // 1/128, not 1/255. The original multiplies by the float 0.0078125
            // (0x3C000000) at all three sites - `FMUL float ptr [0x004af2f8]`
            // is a DIFFERENT constant (255.0f, used by the 0x00485c60 pack-back).
            // Scaling by 1/255 made every SCD-driven tint come out at roughly
            // half the intended intensity, and a tint of 0x80 - which the
            // original saturates to 1.0 - landed at 0.5.
            *(float*)(entry + 0x5c) = (float)r * (1.0f / 128.0f);
            *(float*)(entry + 0x60) = (float)g * (1.0f / 128.0f);
            *(float*)(entry + 0x64) = (float)b * (1.0f / 128.0f);
            *(float*)(entry + 0x6c) = *(float*)(entry + 0x5c);
            *(int*)(entry + 0x70) = *(int*)(entry + 0x60);
            *(int*)(entry + 0x74) = *(int*)(entry + 0x64);
            *(int*)(entry + 0x78) = *(int*)(entry + 0x68);
            entry += 0x84;
            idx++;
        } while (idx < (unsigned int)(vertexCount * 2));
    }
}

// ============================================================================
// JointApplyColorTint (0x0048a190)
// Applies color tinting to a joint's model data and optionally to its paired
// weapon joint. Sets the 0x80 flag on processed joints and updates
// g_playerDisplacement from animation slot data.
// ============================================================================
void JointApplyColorTint(JointStruct* joint, int param2, int param3, void* data)
{
    joint->flags |= 0x80;
    g_playerDisplacement = *(int*)(joint->anim_slot_ptr + 0x14) * 2;
    JointSetColorTint((int)joint->anim_object, (unsigned int)param2);

    if ((g_main_state_flags & MSF_MIRROR_ENABLE) != 0) {
        int offset = ENTITY->weaponJointsPtr - (int)ENTITY->jointsStructs;
        JointStruct* weaponJoint = (JointStruct*)((unsigned char*)joint + offset);
        g_tempVar = weaponJoint;
        weaponJoint->flags |= 0x80;
        g_playerDisplacement = *(int*)(weaponJoint->anim_slot_ptr + 0x14) * 2;
        JointSetColorTint((int)weaponJoint->anim_object, (unsigned int)param2);
    }
}

// ============================================================================
// Animation/effect helper function stubs (called by player_anim_* functions)
// ============================================================================
// ============================================================================
// Joint_move (0x0048b700)
// Core joint animation playback. Advances animation timing, reads joint
// rotation data from EMD animation frames, and applies either direct or
// blended rotations to each joint. Returns 1 when animation loops, 0 otherwise.
// ============================================================================
unsigned int Joint_move(char reverse, unsigned int animHeader, unsigned int animBase, short blendStep)
{
    // Wait if timing hasn't expired
    unsigned char* timingPtr = &ENTITY->timing_control;
    if (1 < *timingPtr) {
        *timingPtr = *timingPtr - 1;
        return 0;
    }

    // Calculate vertex count per frame from animHeader
    g_playerDisplacement = (int)(*(short*)(animHeader + 6) / 2);

    // Get animation slot for current animationId
    unsigned short* animSlot = (unsigned short*)(animBase + (unsigned int)ENTITY->animationId * 4);

    // Get frame entry pointer
    unsigned short* frameEntry = (unsigned short*)((animSlot[1] & 0xFFFFFFFC) +
        (unsigned int)ENTITY->animation_frame_id * 4 + animBase);
    if (reverse != 0) {
        frameEntry = frameEntry + ((unsigned int)*animSlot + (unsigned int)ENTITY->animation_frame_id * (unsigned int)-2) * 2 + (unsigned int)-2;
    }

    // Calculate base of animation vertex data for this frame
    short headerStride = *(short*)(animHeader + 2);
    short aligned = (short)(((int)headerStride + ((int)headerStride >> 31 & 3)) >> 2);
    short* animData = (short*)(animHeader + (int)aligned * 4 +
        (unsigned int)*frameEntry * g_playerDisplacement * 2);

    JointStruct* joint = ENTITY->jointsStructs;
    unsigned char blendCounter = ENTITY->blend_counter;
    char jointCount = ENTITY->jointCount;

    // Set root joint translation from first 3 shorts of animation data
    joint->transform.t[0] = (int)animData[0];

    if (blendCounter == 0) {
        // Direct mode: apply rotations without blending
        joint->transform.t[1] = (int)animData[1];
        joint->transform.t[2] = (int)animData[2];
        short* rotData = animData + 6;

        while (jointCount != 0) {
            jointCount--;
            if ((joint->flags & 0x10) == 0) {
                joint->rotation.x = rotData[0];
                joint->rotation.y = rotData[1];
                joint->rotation.z = rotData[2];
                RotMatrix(&joint->rotation, &joint->transform);
            }
            rotData += 3;
            joint++;
        }
    } else {
        // Blending mode: interpolate rotations
        int step = (int)blendStep;
        unsigned int blend = (unsigned int)blendCounter;
        int invBlend = (int)(0x1000 / (long long)step) - blend;
        short* rotData = animData + 6;

        // Interpolate root Y translation
        int ty = (int)animData[1] * invBlend * step;
        int cy = joint->transform.t[1] * blend * step;
        joint->transform.t[1] = ((ty + (ty >> 31 & 0xFFF)) >> 12) +
                                ((cy + (cy >> 31 & 0xFFF)) >> 12);
        joint->transform.t[2] = (int)animData[2];

        while (jointCount != 0) {
            jointCount--;
            if ((joint->flags & 0x10) == 0) {
                g_svecScratch.x = rotData[0];
                g_svecScratch.y = rotData[1];
                g_svecScratch.z = rotData[2];

                // Angle wrapping fix when invBlend == 1 (last blend step)
                if (invBlend == 1) {
                    for (int i = 2; i >= 0; i--) {
                        short* curRot = &joint->rotation.x + i;
                        short curVal = *curRot;
                        unsigned short diff = ((&g_svecScratch.x)[i] - curVal) + 0x800;
                        if (0x1000 < diff) {
                            *curRot = (unsigned short)(((diff & 0x8000) == 0) * 0x2000) + curVal + (short)-0x1000;
                        }
                    }
                }

                fp_lerp(&joint->rotation, &g_svecScratch, blend * step, invBlend * step, &joint->rotation);
                RotMatrix(&joint->rotation, &joint->transform);
            }
            rotData += 3;
            joint++;
        }
        ENTITY->blend_counter = ENTITY->blend_counter - 1;
    }

    // Update timing and advance frame
    ENTITY->timing_control = (char)frameEntry[1];
    ENTITY->animation_frame_id = ENTITY->animation_frame_id + 1;

    // Check for animation loop
    if ((int)(*animSlot - 1) < (int)(unsigned int)ENTITY->animation_frame_id) {
        ENTITY->animation_frame_id = 0;
        return 1;
    }
    return 0;
}

// ============================================================================
// Effect_CreateBillboard (0x0047be30)
// Allocates one or more effect slots from the 64-slot pool and initializes
// a billboard sprite effect. When the animation has multiple frames, allocates
// one slot per frame (each gets a progressively earlier frame).
// Returns: slot index (0-63) on success, 0xFF on failure.
// ============================================================================
unsigned char Effect_CreateBillboard(
    unsigned char type, unsigned char depthGroup, short yaw,
    void* spriteInfo, void* pos, char lightFactor)
{
    if (g_freeEffectSlots == 0) return type;

    bool needAnimLookup = true;
    unsigned int* frameArrayPtr = NULL;
    unsigned int frameCount = 0;

    while (true) {
        // Search for a free slot (from 63 down to 0)
        bool noFreeSlot = true;
        unsigned char slotIdx = 64;
        do {
            slotIdx--;
            if (g_effectPool[slotIdx].animId == 0) {
                noFreeSlot = false;
                g_freeEffectSlots--;
            }
        } while (slotIdx != 0 && noFreeSlot);

        if (noFreeSlot) break;

        Effect* eff = &g_effectPool[slotIdx];
        VECTOR* vPos = (VECTOR*)pos;

        // Clear velocity, position, transform, and sprite offset fields
        eff->rotSpeedZ = 0;
        eff->rotSpeedY = 0;
        eff->rotSpeedX = 0;
        eff->posZ = 0;
        eff->posY = 0;
        eff->posX = 0;
        for (int i = 0; i < 9; i++) eff->transform[i] = 0;
        eff->spriteOffsetZ = 0;
        eff->spriteOffsetY = 0;
        eff->spriteOffsetX = 0;
        eff->depthScaled = 0;
        eff->projDepth = 0;

        // Set spawn parameters
        eff->effectType = type;
        eff->depthGroup = depthGroup;
        eff->localOffsetX = (short)vPos->x;
        eff->localOffsetY = (short)vPos->y;
        eff->localOffsetZ = (short)vPos->z;
        eff->spriteInfo = (int)spriteInfo;

        // Copy full-precision spawn position (VECTOR with pad)
        eff->spawnPosX = vPos->x;
        eff->spawnPosY = vPos->y;
        eff->spawnPosZ = vPos->z;
        eff->spawnPosW = vPos->pad;

        // Set up texture pointers from sprite info table
        unsigned int typeIdx = (unsigned int)type;
        DWORD* spriteInfoBase = (DWORD*)g_effectSpriteInfo[typeIdx];

        // g_effectSpriteInfo is populated per-room by load_effect_sprite_data from
        // the RDT's effect-animation index table, so only the effect types the
        // CURRENT room declares are valid. An unregistered type leaves a 0 here and
        // the `+2` read below faults at address 0x00000002 - which is what
        // room_transition_load hit, running the destination room's init SCD before
        // that room's RDT effect table had been loaded.
        //
        // The original does not guard this either; it would fault the same way. The
        // guard is port-only so a data-ordering bug reports instead of crashing.
        if (spriteInfoBase == NULL || (DWORD)spriteInfoBase == 0xFFFFFFFF) {
            // Hand the slot BACK. The search loop above already claimed it with
            // `g_freeEffectSlots--`, but this path never sets eff->animId, so the
            // slot stays free in the pool while the counter says it is taken.
            // Without this the counter leaks one slot per skipped billboard, and
            // a room that spawns an undeclared type repeatedly (ROOM1000 fires
            // opcode 0x2A five times in one cutscene) drives g_freeEffectSlots to
            // 0 - after which the `if (g_freeEffectSlots == 0) return type;` at
            // the top of this function rejects EVERY effect for the rest of the
            // session, including the ones whose sprites are perfectly valid.
            // That turns one missing sprite into "no FX anywhere, permanently".
            g_freeEffectSlots++;
            dbg_printf("[effect] g_effectSpriteInfo[%u] not loaded for this room"
                       " - billboard skipped (depthGroup=%u free=%u)\n",
                       typeIdx, (unsigned int)depthGroup,
                       (unsigned int)g_freeEffectSlots);
            return 0;
        }

        eff->clutInfo = (int)spriteInfoBase;
        eff->vramInfo = (int)(spriteInfoBase + 2);      // +8 bytes
        eff->vramInfoBackup = (int)(spriteInfoBase + 2);

        // UV data starts after sprite entries: base + 8 + count * 4
        unsigned short uvCount = *(unsigned short*)((char*)spriteInfoBase + 2);
        int uvAddr = (int)((char*)spriteInfoBase + 8 + uvCount * 4);
        eff->uvData = uvAddr;
        eff->uvDataBackup = uvAddr;

        // Read initial frame delay and index from VRAM info
        unsigned char* vramPtr = (unsigned char*)eff->vramInfo;
        eff->frameDelay = vramPtr[1];
        eff->frameIndex = vramPtr[0];

        // Look up animation data (only on first iteration)
        if (needAnimLookup) {
            needAnimLookup = false;
            unsigned char* animBase = (unsigned char*)g_effectAnimData[typeIdx];
            unsigned char depthIdx = depthGroup & 0x07;
            unsigned int tableIndex = (unsigned int)animBase[depthIdx];
            frameArrayPtr = (unsigned int*)(animBase + tableIndex * 4);
            frameCount = *frameArrayPtr;
        }

        // Skip to the correct animation frame (skip frameCount-1 frames)
        unsigned int* pFrame = frameArrayPtr + 1;
        if (frameCount > 1) {
            unsigned int remaining = frameCount - 1;
            do {
                unsigned int entryCount = *pFrame;
                pFrame = pFrame + entryCount * 6 + 1;
                remaining--;
            } while (remaining != 0);
        }

        // Set animation frame data pointers (past the 4-byte header)
        eff->animDataFrame = (int)(pFrame + 1);
        eff->animDataBase = (int)(pFrame + 1);

        // Copy 24-byte animation header into the slot (6 DWORDs)
        unsigned int* src = (unsigned int*)(pFrame + 1);
        unsigned int* dst = (unsigned int*)&eff->animId;
        for (int i = 0; i < 6; i++) dst[i] = src[i];

        // Set light factor if provided
        if (lightFactor != 0) {
            eff->lightFactor = lightFactor;
        }

        // Decrement frame count, apply yaw, mark active
        frameCount--;
        eff->yaw += yaw;
        eff->type = 1;

        // Use identity matrix if no sprite info was provided
        if (spriteInfo == NULL) {
            eff->spriteInfo = (int)&g_identityMatrixData;
        }

        // If no more frames remain, return this slot
        if (frameCount == 0) return slotIdx;
    }

    return 0xFF;
}

// ============================================================================
// BillboardSetColor (0x00456710)
// Sets tpage and vertex color on a billboard quad structure.
// ============================================================================
void BillboardSetColor(void* quad, int unused1, int unused2, unsigned int color)
{
    unsigned short tpage = GteTpageBuild(1, 2, 384, 256);
    *(unsigned short*)((char*)quad + 0x1E) = tpage;
    *(unsigned short*)((char*)quad + 0x46) = tpage;
    unsigned int c0 = *(unsigned int*)((char*)quad + 0x0C);
    *(unsigned int*)((char*)quad + 0x0C) = (c0 & 0xFF000000) | (color & 0x00FFFFFF);
    unsigned int c1 = *(unsigned int*)((char*)quad + 0x34);
    *(unsigned int*)((char*)quad + 0x34) = (c1 & 0xFF000000) | (color & 0x00FFFFFF);
}

// ============================================================================
// BillboardAdjSize (0x00456760)
// Adjusts billboard quad vertex positions by the given half-extents.
// ============================================================================
void BillboardAdjSize(void* quad, short halfW, short halfH)
{
    *(short*)((char*)quad + 0x58) -= halfW;
    *(short*)((char*)quad + 0x5C) += halfH;
    *(short*)((char*)quad + 0x60) += halfW;
    *(short*)((char*)quad + 0x64) += halfH;
    *(short*)((char*)quad + 0x68) -= halfW;
    *(short*)((char*)quad + 0x6C) -= halfH;
    *(short*)((char*)quad + 0x70) += halfW;
    *(short*)((char*)quad + 0x74) -= halfH;
}

// ============================================================================
// BillboardSetSize (0x00456790)
// Sets billboard quad vertex positions from half-extents (4 corners).
// ============================================================================
void BillboardSetSize(void* quad, short halfW, short halfH)
{
    *(short*)((char*)quad + 0x58) = -halfW;
    *(short*)((char*)quad + 0x5C) =  halfH;
    *(short*)((char*)quad + 0x60) =  halfW;
    *(short*)((char*)quad + 0x64) =  halfH;
    *(short*)((char*)quad + 0x68) = -halfW;
    *(short*)((char*)quad + 0x6C) = -halfH;
    *(short*)((char*)quad + 0x70) =  halfW;
    *(short*)((char*)quad + 0x74) = -halfH;
}

// ============================================================================
// BillboardSetRect (0x004567d0)
// The asymmetric sibling of BillboardSetSize: instead of one half-extent per
// axis it takes all four edges, so the quad can be off-centre. Used by the door
// sequence, where the shadow has to reach further forward than back.
//
// `left` and `back` are stored negated, which is what makes the argument order
// read oddly at the call sites: the original pushes (right, left, front, back)
// and writes -left / -back. The four corners land at quad+0x58/0x60/0x68/0x70
// with x at +0 and z at +4 - the same layout BillboardAdjSize patches and
// FUN_004565f0 builds.
// ============================================================================
void BillboardSetRect(void* quad, short right, short left, short front, short back)
{
    *(short*)((char*)quad + 0x58) = -left;
    *(short*)((char*)quad + 0x5C) =  front;
    *(short*)((char*)quad + 0x60) =  right;
    *(short*)((char*)quad + 0x64) =  front;
    *(short*)((char*)quad + 0x68) = -left;
    *(short*)((char*)quad + 0x6C) = -back;
    *(short*)((char*)quad + 0x70) =  right;
    *(short*)((char*)quad + 0x74) = -back;
}

// ============================================================================
// GetPlayerInputMasked (0x0048a030)
// Returns player pad held state masked to d-pad + face buttons (0xF0F0).
// ============================================================================
short GetPlayerInputMasked(void)
{
    return (short)(g_PlayerPadHeld & 0xF0F0);
}

// ============================================================================
// EntityUpdateWeaponJoint (0x00459de0)
// Calculates weapon joint world position by composing joint transforms and
// adjusting the entity's translation to match.
// ============================================================================
void EntityUpdateWeaponJoint(int weaponIdx)
{
    JointStruct* joints = ENTITY->jointsStructs;

    // Build rotation from entity's facing angle
    RotMatrix((SVECTOR*)&ENTITY->position.pad, &ENTITY->scaMatrixData.localMatrix);

    // Compose: scratch = entity_transform * joint[0].transform
    ApplyLVAndMul0Matrix(&ENTITY->scaMatrixData.localMatrix, &joints[0].transform, &g_matrixScratch);

    // Compose: scratch = scratch * joint[2].transform
    ApplyLVAndMulMatrix(&g_matrixScratch, &joints[2].transform);

    // Compose through 3 weapon chain joints
    unsigned int idx = (unsigned int)weaponIdx;
    for (int i = 3; i > 0; i--) {
        ApplyLVAndMulMatrix(&g_matrixScratch, &joints[idx * 3 + (6 - i)].transform);
    }

    // Extract translation offset from last weapon joint's world matrix
    g_matrixScratch.t[1] = 0;
    g_matrixScratch.t[0] -= joints[idx * 3 + 5].world.t[0];
    g_matrixScratch.t[2] -= joints[idx * 3 + 5].world.t[2];

    // Adjust entity translation
    ENTITY->scaMatrixData.localMatrix.t[0] -= g_matrixScratch.t[0];
    ENTITY->scaMatrixData.localMatrix.t[2] -= g_matrixScratch.t[2];
}

// ============================================================================
// MulMatrixInPlace (0x0040a170)
// In-place matrix multiply: m1 = m0 * m1
// ============================================================================
MATRIX* MulMatrixInPlace(MATRIX* m0, MATRIX* m1)
{
    MulMatrix0(m0, m1, m1);
    return m1;
}

// ============================================================================
// Add_speedXZ (0x0048a590)
// Adds movement speed to entity transform in the entity's facing direction
// plus an angular offset. Moves the entity by its move_speed_current speed value.
// ============================================================================
void Add_speedXZ(int angleOffset)
{
    // Set speed vector from entity's base speed value
    g_svecScratch.x = ENTITY->move_speed_current;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;

    // Copy identity matrix to scratch
    g_matrixScratch = g_identityMatrixData;

    // Rotate by entity facing + offset
    RotMatrixY((int)(short)ENTITY->angle + (int)(short)angleOffset, &g_matrixScratch);

    // Transform speed vector by rotation
    ApplyMatrixSV(&g_matrixScratch, &g_svecScratch, &ENTITY->speed);

    // Apply speed to entity transform translation
    ENTITY->scaMatrixData.localMatrix.t[0] += (int)ENTITY->speed.x;
    ENTITY->scaMatrixData.localMatrix.t[1] += (int)ENTITY->speed.y;
    ENTITY->scaMatrixData.localMatrix.t[2] += (int)ENTITY->speed.z;
}

// ============================================================================
// ApplyLVAndMul0Matrix (0x0040a0b0)
// Full matrix composition: m_out = m0 * m1 (rotation + translation)
// ============================================================================
void ApplyLVAndMul0Matrix(void* m0, void* m1, void* mOut)
{
    CompMatrix((MATRIX*)m0, (MATRIX*)m1, (MATRIX*)mOut);
}

// RotMatrix is implemented in GteMatrix.cpp. Its address is 0x00409df0 -
// 0x004406a0 is GteRotationMatrixCalc, the helper it calls.

// ============================================================================
// Player state machine — update_player_anim and the 0x004d4550 dispatch table
//
// update_player_anim is the per-frame entry point for the player, called from
// game_loop (0x00480ebd). It dispatches on g_playerEntity.animationId through a
// 16-entry table of function pointers statically initialized in .data at
// 0x004d4550. That table had no counterpart in the port at all, and
// update_player_anim itself was an empty stub — so the player never animated,
// never moved and never posed its skeleton, which is why no character model
// appeared and cutscene scripts waiting on player animation stalled forever.
//
// The table extent is 16 entries: 0x004d4590 onward is a different jumptable
// (the action_behavior switch inside state 1), whose targets match that switch's
// cases exactly. Entries 9 and 15 are NULL in the original.
// ============================================================================

extern void SetEntityScaHitData(Entity* ent);                                     // 0x0041b2c0
extern unsigned int HandleEnemyPlayerCollisions(void);                            // 0x00489e10
extern unsigned char check_room_collision(VECTOR* pos, short radius);             // 0x0047d310
extern void FUN_004565f0(SVECTOR* a, SVECTOR* b, int c, int d);                   // 0x004565f0
extern unsigned char mirror_point_visible(void* light, unsigned char param2, int param3); // 0x0048bd00
extern void entity_draw_mirror_reflection(void);                                  // 0x0048bda0
extern void ClearAnimTiming(void);                                                // 0x00429d30
extern void MovePlayerXZ(int angle, SVECTOR* offset, SVECTOR* out);
extern int is_entity_in_switch_zone(VECTOR* pos, void* zoneData);                 // 0x00462d90

// ============================================================================
// EntityUpdateLookAtAngles (0x00459eb0)
// Slews the tracking joint's yaw/pitch toward the look-at target, clamped to a
// +/-0x2C8 yaw (~62 deg) and +/-0x138 pitch (~27 deg) cone.
//
// Gated on lookAtFlags & 0x10 (slew enable). Target position lives in
// scd_pos_x/y/z (+0xCC/+0xD0/+0xD4); with bit 0x80 it is reloaded every call
// from *(scd_target_ptr)+0x34/0x38/0x3C (the -0xA28 on Y aims at head height).
//
//   yaw   (joint->rotDeltaX) = getAngleTowardsTarget(target.x, target.z)
//                              - entity facing angle;      enabled by bit 0x01
//   pitch (joint->rotDeltaY) = GetAngleQuadrantValue((dy << 12)
//                              / sqrt(dx^2+dz^2)) against the joint's world
//                              translation;               enabled by bit 0x02
// With bit 0x20 the target fields ARE the absolute angles (scd_pos_x = yaw,
// scd_pos_y = pitch), skipping both derivations.
//
// Both angles step toward their target by at most lookAtYawStep / 
// lookAtPitchStep per call and clamp to the cone; out-of-cone results snap to
// +/-0x2C8 / +/-0x138 (=0xD38). Bit 0x40 marks a one-shot "aim then freeze"
// mode: once reached it rewrites scd_pos_x/z and clears flag bits so the
// angles stay parked (see the two mid-function 0x40 blocks).
//
// The result is consumed by EntityApplyLookAtRotation (0x0045a2e0), which
// composes joint->world * RotMatrixYXZ(0, yaw, pitch) after
// EntityComputeJointWorldMatrices has built the hierarchy.
// ============================================================================
void EntityUpdateLookAtAngles(void)
{
    Entity* ent = ENTITY;
    if (ent == NULL) return;
    JointStruct* joints = ent->jointsStructs;
    // The original indexes jointsStructs with no checks, relying on every
    // writer of lookAtJointIdx to keep it valid (same guard as
    // EntityApplyLookAtRotation - a bad index here corrupts joint data).
    if (joints == NULL || ent->lookAtJointIdx >= ent->jointCount) return;

    if ((ent->lookAtFlags & 0x10) == 0) return;

    // ---- one-shot aim mode (bit 0x40) bookkeeping on scd_pos_z ----
    if ((ent->lookAtFlags & 0x40) != 0) {
        if (((unsigned int)ent->scd_pos_z & 0x10000) == 0) {
            ent->scd_pos_z = ent->scd_pos_z * 2;
            ent->scd_pos_z = ent->scd_pos_z | 0x10000;
            ent->lookAtFlags |= 0x20;          // switch to absolute-angle mode
        }
    }
    if ((ent->lookAtFlags & 0x40) != 0 &&
        ((unsigned int)ent->scd_pos_z & 0xFFFF) == 0) {
        ent->lookAtFlags &= 0x1C;              // park: keep only 0x10|0x08|0x04
    }

    // ---- reload live target from SCD event pointer ----
    if ((ent->lookAtFlags & 0x80) != 0) {
        unsigned int tgt = ent->scd_target_ptr;
        ent->scd_pos_x = *(int*)(tgt + 0x34);
        ent->scd_pos_y = *(int*)(tgt + 0x38) + -0xA28;   // head-height bias
        ent->scd_pos_z = *(int*)(tgt + 0x3C);
    }

    unsigned char yawStep   = ent->lookAtYawStep;     // +0xD9
    unsigned char pitchStep = ent->lookAtPitchStep;   // +0xDA
    JointStruct* joint = joints + ent->lookAtJointIdx;

    // ========================================================================
    // Yaw -> rotDeltaX (joint+0x76)
    // ========================================================================
    int targetYaw;
    if ((ent->lookAtFlags & 0x20) == 0) {
        short toTarget = getAngleTowardsTarget(ent->scd_pos_x, ent->scd_pos_z);
        targetYaw = (toTarget - ent->angle) & 0xFFF;  // relative to own facing
    } else {
        targetYaw = (unsigned short)(unsigned int)ent->scd_pos_x; // absolute
    }
    if ((ent->lookAtFlags & 0x01) == 0) targetYaw = 0;

    unsigned short curYaw = (unsigned short)joint->rotDeltaX;
    unsigned int yawDiff = ((unsigned int)yawStep
                            - (unsigned int)(int)(short)curYaw
                            + (unsigned int)targetYaw) & 0xFFF;

    if (yawDiff < (unsigned int)yawStep * 2) {
        // Within one step of the target: snap...
        joint->rotDeltaX = (short)targetYaw;
        // ...but reject if the snapped value falls outside the +/-0x2C8 cone
        if ((((unsigned int)targetYaw + 0x2C8) & 0xFFF) > 0x590) {
            joint->rotDeltaX = curYaw;
        }
        // One-shot mode, yaw enabled: rewrite target for the frozen pose
        if ((ent->lookAtFlags & 0x40) != 0 && (ent->lookAtFlags & 0x01) != 0) {
            ent->scd_pos_x = 0x1000 - ent->scd_pos_x;
            ent->scd_pos_z = ent->scd_pos_z - 1;
        }
    } else if (targetYaw == 0) {
        // Already aimed: decay toward zero from whichever side we are on
        unsigned short v;
        if ((curYaw & 0xFFF) < 0x801) v = (curYaw - yawStep) & 0xFFF;
        else                          v = (curYaw + yawStep) & 0xFFF;
        joint->rotDeltaX = v;
    } else if ((((unsigned int)targetYaw - (unsigned int)curYaw) & 0xFFF) < 0x800) {
        // Target less than half a turn clockwise: increase, clamped to +0x2C8
        unsigned short v = (curYaw + yawStep) & 0xFFF;
        joint->rotDeltaX = v;
        if ((((int)(short)v - 0x2C8) & 0xFFF) < 0xA70) {
            joint->rotDeltaX = 0x2C8;
        }
    } else {
        // Decrease, clamped to -0x2C8 (=0xD38)
        unsigned short v = (curYaw - yawStep) & 0xFFF;
        joint->rotDeltaX = v;
        if ((((int)(short)v + 0x2C8) & 0xFFF) > 0x590) {
            joint->rotDeltaX = 0xD38;
        }
    }

    // ========================================================================
    // Pitch -> rotDeltaY (joint+0x78). Mirrors the yaw slew with a +/-0x138
    // window; direction sense is inverted relative to yaw because the pitch
    // axis points the opposite way.
    // ========================================================================
    int targetPitch;
    if ((ent->lookAtFlags & 0x20) == 0) {
        int dx = ent->scd_pos_x - joint->world.t[0];
        int dz = ent->scd_pos_z - joint->world.t[2];
        int dist = SquareRoot0(dx * dx + dz * dz);
        short dy = (short)ent->scd_pos_y - (short)joint->world.t[1];
        unsigned char pitchEn = (ent->lookAtFlags & 0x02) >> 1;
        if (dist != 0) {
            targetPitch = GetAngleQuadrantValue((dy << 12) / dist) * pitchEn;
        } else {
            targetPitch = ((0 < dy ? 0x800 : 0) + 0x400) * pitchEn;
        }
    } else {
        targetPitch = ((ent->lookAtFlags & 0x02) >> 1) * (short)ent->scd_pos_y;
    }

    short curPitch = joint->rotDeltaY;
    unsigned int pitchDiff = ((unsigned int)pitchStep
                              - (unsigned int)(int)curPitch
                              + (unsigned int)targetPitch) & 0xFFF;

    if (pitchDiff < (unsigned int)pitchStep * 2) {
        joint->rotDeltaY = (short)targetPitch;
        // Reject snaps outside the +/-0x138 cone
        if ((((unsigned int)targetPitch + 0x138) & 0xFFF) > 0x270) {
            joint->rotDeltaY = curPitch;
        }
        // One-shot mode, yaw disabled: clear pitch-enable and freeze
        unsigned char f = ent->lookAtFlags;
        if ((f & 0x40) != 0 && (f & 0x01) == 0) {
            ent->lookAtFlags = f ^ 0x02;
            ent->scd_pos_z = ent->scd_pos_z - 1;
            return;
        }
    } else if ((((unsigned int)(int)curPitch - (unsigned int)targetPitch) & 0xFFF) < 0x801) {
        unsigned short v = ((unsigned short)curPitch - pitchStep) & 0xFFF;
        joint->rotDeltaY = v;
        if ((((int)(short)v + 0x138) & 0xFFF) > 0x938) {
            joint->rotDeltaY = (unsigned short)(v + pitchStep);
        }
    } else {
        unsigned short v = ((unsigned short)curPitch + pitchStep) & 0xFFF;
        joint->rotDeltaY = v;
        if ((((int)(short)v - 0x138) & 0xFFF) < 0x6C8) {
            joint->rotDeltaY = v - pitchStep;
            return;
        }
    }
}

// ----------------------------------------------------------------------------
// FUN_00456a10 (0x00456a10) — DEFERRED, 785 bytes.
// Builds the player ground shadow / fade sprite (0x00456a10). Queues the
// player's quad at entity+0xE4 (the shadow, or the death blood-puddle after
// the death anim re-colours and resizes it) through the FadeSpr system, the
// same path entity_add_fade_sprite uses for the enemies and NPCs. This was
// an empty stub blocked on RotAverage4, which the port now implements.
// ----------------------------------------------------------------------------
static void player_update_shadow_sprite(int posPtr, int sprPtr, int height, int angle)
{
    entity_add_fade_sprite((VECTOR*)posPtr, (short*)sprPtr, (short)height, (short)angle);
}

// ----------------------------------------------------------------------------
// The ejected magazine - FUN_00429d50 (0x00429d50) plus the three-state table
// at 0x004ba950.
//
// LoadEntityModel bumps jointCount by one across a single SetupJointStructures
// call, so the player skeleton owns a SIXTEENTH joint (index 15) that no
// animation drives: it carries the empty clip's own TMD. The beretta's reload
// routine (0x00458ff0, frame 0xa) arms it through 0x0042a000, and this runs its
// physics and draws it every frame while jointsStructs[15].velZ != 0. That
// joint's spare shorts are the entire state:
//
//   velX      (0x70) vertical velocity   rotDeltaX (0x76) state index (0-2)
//   velY      (0x72) Y position tracker  rotDeltaY (0x78) bounce/dust latch
//   velZ      (0x74) active flag         rotDeltaZ (0x7A) floor Y at eject time
//
// Y grows DOWNWARD, so gravity adds to velX and "hit the floor" is velY passing
// rotDeltaZ. ClearAnimTiming (0x00429d30) zeroes velZ/rotDeltaX, which is why a
// normally initialised player never enters any of this.
//
// The port had the gate but a deferred body, so the beretta armed the clip and
// then nothing ever integrated or drew it - the magazine never fell out.
// ----------------------------------------------------------------------------

extern void update_entity_lighting(VECTOR* entityPos);   // 0x00481660 (TmdRenderer.cpp)

// 0x00429e60 - state 0: spawn. Seats the clip on the weapon joint's world pose
// and gives it a fixed z-rotation of 0x400; the copies from the hand joint's
// rotation are immediately overwritten, which the original does too.
static void player_clip_state_0_spawn(JointStruct* clip)
{
    JointStruct* hand = &g_playerEntity.jointsStructs[0xe];   // jointsStructs + 0x6c8

    clip->rotDeltaX++;                       // -> state 1
    clip->world = hand->world;               // REP MOVSD, 8 dwords

    clip->transform.t[0] = 0;
    clip->transform.t[1] = 0;
    clip->transform.t[2] = 0;

    clip->rotation   = hand->rotation;
    clip->velX       = 0x50;
    clip->velY       = (short)clip->world.t[1];
    clip->rotation.z = 0x400;
    clip->rotation.x = 0;
    clip->rotation.y = 0;
    clip->rotDeltaY  = 0;
    clip->rotDeltaZ  = (short)g_playerEntity.scaMatrixData.localMatrix.t[1];
}

// 0x00429ed0 - state 1: the fall. Gravity plus a tumble on X, one dust puff the
// first time the clip drops past the room's reference height (the same
// g_omodel_table[0]+0x38 the knife swing tests), then the floor hit: bounce the
// velocity, snap Y to the floor and play the impact.
static void player_clip_state_1_fall(JointStruct* clip)
{
    const short bounced = clip->rotDeltaY;

    clip->velX       = (short)(clip->velX + (short)((1 - bounced) * 0x14));
    clip->rotation.x = (short)(clip->rotation.x + (short)((bounced + 1) * 0x10));

    if (bounced == 0 && (g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0
        && *(int*)((char*)g_omodel_table[0] + 0x38) < (int)clip->velY) {
        g_playerPosScratch.x   = *(int*)((char*)g_deadMoveValue + 0x14);
        g_playerPosScratch.y   = *(int*)((char*)g_deadMoveValue + 0x18);
        g_playerPosScratch.z   = *(int*)((char*)g_deadMoveValue + 0x1c);
        g_playerPosScratch.pad = *(int*)((char*)g_deadMoveValue + 0x20);
        Effect_CreateBillboard(0x17, 8, 0, (void*)g_deadMoveValue, clip->world.t, 0);
        clip->rotDeltaY = 1;
        return;
    }

    const short floorY = clip->rotDeltaZ;
    if (clip->velY > floorY) {
        clip->velX = -0x50;
        clip->rotDeltaX++;                   // -> state 2
        clip->velY = floorY;
        clip->world.t[1] = (int)floorY - clip->transform.t[1];

        g_playerPosScratch.x = clip->world.t[0] + clip->transform.t[0];
        g_playerPosScratch.y = clip->transform.t[1] + clip->world.t[1];
        g_playerPosScratch.z = clip->world.t[2] + clip->transform.t[2];
        Play3DSnd(2, 0x15, 0, (int)&g_playerPosScratch);
    }
}

// 0x00429fb0 - state 2: the bounce. Skitters away on X/Z while tumbling on both
// axes; once it drops back past the floor the clip switches itself off.
static void player_clip_state_2_bounce(JointStruct* clip)
{
    const short bounced = clip->rotDeltaY;
    const short swing   = (short)(2 - bounced);

    clip->world.t[2] += 0xa;
    clip->world.t[0] += 0x14;

    clip->rotation.x = (short)(clip->rotation.x + (short)(swing << 7));
    const short y    = clip->velY;
    clip->rotation.y = (short)(clip->rotation.y + (short)(swing << 6));
    clip->velX       = (short)(clip->velX + (short)((bounced + 1) * 0xf));

    if (clip->rotDeltaZ < y) {
        clip->velZ = 0;
    }
}

static void player_update_detached_joint(void)   // 0x00429d50
{
    JointStruct* joints = g_playerEntity.jointsStructs;
    if (joints == NULL) return;
    if ((g_playerEntity.zoneFlags & 0x7f) == 0) return;

    JointStruct* clip = &joints[0xf];
    if (clip->velZ == 0) return;

    // 0x00429d7a: the caller integrates; the state routines only set velocities.
    clip->world.t[1] += (int)clip->velX;
    clip->velY = (short)(clip->velY + clip->velX);

    // 0x00429d8e: CALL [rotDeltaX*4 + 0x004ba950]. Only 0/1/2 are ever stored.
    switch (clip->rotDeltaX) {
    case 0: player_clip_state_0_spawn(clip);  break;
    case 1: player_clip_state_1_fall(clip);   break;
    case 2: player_clip_state_2_bounce(clip); break;
    default: break;
    }

    // The clip is then drawn as its own TMD off joint 15's slot, exactly the way
    // tyrant_draw_heart draws the detached heart.
    if (clip->anim_slot_ptr == 0) return;   // the original dereferences this unchecked

    update_entity_lighting((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t);

    MATRIX local;
    RotMatrix(&clip->rotation, &clip->transform);
    ApplyLVAndMul0Matrix(&clip->world, &clip->transform, &local);
    CompMatrix((MATRIX*)g_RoomCameraDataCopy, &local, &g_matrixScratch);
    MulMatrix0((MATRIX*)g_lightMatrixPtr, &clip->world, &local);

    g_entityJointPosX = clip->anim_slot_ptr;
    SetLightMatrix(&local);
    SetRotAndTransMatrix(&g_matrixScratch);

    const int* slot = (const int*)clip->anim_slot_ptr;
    FUN_00483250(slot[4], slot[0], slot[2], (int)clip->anim_object, slot[5], 4,
                 (char*)&g_spriteAnimSlots[2] + (unsigned int)g_spriteAnimActive * 0x14);
}

// ----------------------------------------------------------------------------
// Player state 0 (FUN_00494eb0) — spawn / re-init.
// Runs for exactly one frame: poses the skeleton from the animation data via
// Joint_move, syncs position from the transform matrix, clears combat state,
// then hands over to state 1.
// ----------------------------------------------------------------------------
static void player_state_init(void) // 0x00494eb0
{
    g_playerEntity.animationId     = 1;
    g_playerEntity.animFrameId     = 0;
    g_playerEntity.action_behavior = 0;
    g_playerEntity.action_state    = 0;

    // 0x00494ec6: the original stores the constant 0x00808080 here. Ghidra
    // renders it as &DAT_00808080 because the value looks like an address; it is
    // a packed grey triple, not a pointer.
    g_animFrameIdSave = 0x00808080;

    g_PlayerDpadHeld = 0;

    g_svecScratch.x = 0;
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    FUN_004565f0(&g_svecScratch, &g_playerEntity.pushVelocity, 500, 700);

    // 0x00494f0b: written through the global ENTITY pointer in the original,
    // which update_player_anim has already aimed at g_playerEntity.
    g_playerEntity.unk_8c             = 0;
    g_playerEntity.attackAnim         = 0;
    g_playerEntity.animation_frame_id = 0;
    g_playerEntity.unk_bf             = 0;

    Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x400);

    // 0x00494f60: reset the hit box; the width comes from the SCA info block
    *(unsigned short*)(g_playerEntity.pSca_hit_data + 0) = 0;
    *(unsigned short*)(g_playerEntity.pSca_hit_data + 4) = 0;
    *(unsigned short*)(g_playerEntity.pSca_hit_data + 2) =
        *(unsigned short*)(g_playerEntity.Sca_info + 4);

    g_playerEntity.position.y = (short)g_playerEntity.scaMatrixData.localMatrix.t[1];
    g_playerEntity.position.x = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
    g_playerEntity.position.z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];

    g_playerEntity.zoneFlags = 0;
    g_playerEntity.healthStatusFlags |= 0x10;
    g_playerEntity.attackTimer = 0;
    g_playerEntity.isBeingAttackedFlag = 0;

    ClearAnimTiming();
}

// ----------------------------------------------------------------------------
// Player states 5, 6 and 7 (0x00495290, 0x004952d0, 0x00495310).
// Each forwards to g_playerAnimFunctions[animFrameId + window], where the three
// windows are 0, 0x13 and 0x26 across that 52-entry table.
//
// PORT NOTE: the original g_playerAnimFunctions is fully populated —
// set_player_animations_functions overwrites only 14 of its entries and the rest
// come from static .data that has not been extracted yet. Dispatching a NULL
// entry would fault with no clue which index was missing, so these log the index
// and return instead. Drop the guard once the table is complete.
// ----------------------------------------------------------------------------
static void player_dispatch_anim_fn(unsigned int index)
{
    if (index >= 52 || g_playerAnimFunctions[index] == NULL) {
        static int lastReported = -1;
        if ((int)index != lastReported) {
            lastReported = (int)index;
            dbg_printf("[player] g_playerAnimFunctions[%u] is NULL (animFrameId=%u)\n",
                   index, (unsigned int)g_playerEntity.animFrameId);
        }
        return;
    }
    ((void(*)(void))g_playerAnimFunctions[index])();
}

static void player_state_anim_window0(void) // 0x00495290
{
    if (g_playerEntity.action_state == 0) {
        if (((g_main_state_flags & MSF_OBJECT_PUSH) != 0) ||
            ((g_playerEntity.healthStatusFlags & 0x80) != 0)) {
            g_message_flags = (unsigned short)(g_message_flags | 0x40);
        }
        g_playerEntity.healthStatusFlags &= 0x7f;
    }
    player_dispatch_anim_fn(g_playerEntity.animFrameId);
}

static void player_state_anim_window1(void) // 0x004952d0
{
    if (g_playerEntity.action_state == 0) {
        if (((g_main_state_flags & MSF_OBJECT_PUSH) != 0) ||
            ((g_playerEntity.healthStatusFlags & 0x80) != 0)) {
            g_message_flags = (unsigned short)(g_message_flags | 0x40);
        }
        g_playerEntity.healthStatusFlags &= 0x7f;
    }
    player_dispatch_anim_fn((unsigned int)g_playerEntity.animFrameId + 0x13);
}

static void player_state_anim_window2(void) // 0x00495310
{
    player_dispatch_anim_fn((unsigned int)g_playerEntity.animFrameId + 0x26);
}

// ----------------------------------------------------------------------------
// Player state 4 (0x00495280) — suppress all message/input flags.
// ----------------------------------------------------------------------------
static void player_state_block_input(void) // 0x00495280
{
    g_message_flags = 0;
}

// ----------------------------------------------------------------------------
// Unimplemented states, reached by animationId values not yet transcribed.
// Logging the index rather than leaving a NULL entry that faults is what tells
// us which state a stalled cutscene is asking for.
// ----------------------------------------------------------------------------
static void player_state_report_missing(const char* addr)
{
    static const char* lastAddr = NULL;
    if (addr != lastAddr) {
        lastAddr = addr;
        dbg_printf("[player] unimplemented state animationId=%u -> %s "
               "(behavior=0x%02X actionState=%u animFrameId=%u msgFlags=%04X)\n",
               (unsigned int)g_playerEntity.animationId, addr,
               (unsigned int)g_playerEntity.action_behavior,
               (unsigned int)g_playerEntity.action_state,
               (unsigned int)g_playerEntity.animFrameId,
               (unsigned int)(WORD)g_message_flags);
    }
}

// ----------------------------------------------------------------------------
// Player state 2 (0x00495250 -> 0x004955f0) — the generic hit reaction.
//
// Was a report-only stub, which froze the player outright: every one of the five
// action_behavior cases below is a self-terminating animation, and their common
// tail is the ONLY thing that returns animationId to 1 and clears
// isBeingAttackedFlag. Plant 42's acid spit drops the player straight in here
// (it writes animationId=2 / animFrameId=0 / action_behavior=0x64 as one DWORD),
// so being hit while aiming locked the game up.
//
// 0x64/0x65 animate from the ordinary animHeader/animBase; 0x66/0x67/0x68 use the
// damage pointers at +0x16C/+0x170 and pick their animation from
// isBeingAttackedFlag - 1. As everywhere on the player, the animation index
// Joint_move reads is attackAnim (+0xBD).
// ----------------------------------------------------------------------------
static void player_hit_react_common(bool damageAnimSet, unsigned char sndId, short startSpeed)
{
    if (g_playerEntity.action_state == 0) {
        if (damageAnimSet) {
            g_playerEntity.attackAnim = (unsigned char)(g_playerEntity.isBeingAttackedFlag - 1);
        } else {
            g_playerEntity.attackAnim = 1;
        }
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.move_speed_current = (unsigned short)startSpeed;
        g_playerEntity.action_state = 1;
        g_playerEntity.unk_8c = 3;
        Play3DSnd(3, sndId, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t[0]);
    } else if (g_playerEntity.action_state != 1) {
        return;
    }

    unsigned int header = damageAnimSet ? g_playerEntity.emdScratchPtr1 : g_playerEntity.animHeader;
    unsigned int base   = damageAnimSet ? g_playerEntity.emdScratchPtr2 : g_playerEntity.animBase;
    if (Joint_move(0, header, base, 0x400) != 0) {
        g_playerEntity.animationId = 1;
        g_playerEntity.animFrameId = 0;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 0;
        g_playerEntity.isBeingAttackedFlag = 0;
    }
}

static void player_state_02(void)         // 0x00495250
{
    if (g_playerEntity.animFrameId != 0) return;

    // 0x004955f0
    if (g_playerEntity.action_state == 0) {
        if ((g_main_state_flags & MSF_OBJECT_PUSH) != 0 ||
            (g_playerEntity.healthStatusFlags & 0x80) != 0) {
            g_message_flags = (unsigned short)((g_message_flags & 0xff00) |
                                               (((unsigned char)g_message_flags) | 0x40));
        }
        g_playerEntity.healthStatusFlags &= 0x7f;
    }

    switch (g_playerEntity.action_behavior) {
    case 0x64:                                   // 0x00457090
    case 0x65:
        player_hit_react_common(false, 0, 0);
        return;
    case 0x66:                                   // 0x00457110
        player_hit_react_common(true, 0, 0xfa);
        if (3 < g_playerEntity.animation_frame_id) g_playerEntity.move_speed_current = 0x1e;
        break;
    case 0x67:                                   // 0x00457110 + Add_speedXZ(0)
        player_hit_react_common(true, 0, 0xfa);
        if (3 < g_playerEntity.animation_frame_id) g_playerEntity.move_speed_current = 0x1e;
        Add_speedXZ(0);
        return;
    case 0x68:                                   // 0x004571a0
        player_hit_react_common(true, 3, 0);
        return;
    default:
        return;
    }
    Add_speedXZ(0x800);
}
static void player_state_null(void)       { player_state_report_missing("NULL in original"); }

// ============================================================================
// Player state 3 — death fall (0x00495270 -> FUN_00459be0)
//
// Set up by player_state_01_control when health < 0 (animationId 3). Runs the
// death sequence: scream, the fall motion via Joint_move, the sliding
// corpse, then the blood billboard (or the scripted-death rooms skip it and
// jump straight to state 4). State 3 counts attackDirection down from 0xB4
// to 0x20 while the billboard grows, then hands to animationId 4 (state 4 =
// player_state_block_input - the death screen takes over from there).
//
// The word compare at 0x00be9820 spans g_stageId (low byte) and g_roomId
// (high byte), the same pattern as the zombie revive check in Zombie.cpp.
// ============================================================================
static void player_state_03(void)
{
    // (0x00459be3): [scream frame, slide speed] per player id 0/1
    static const unsigned short s_deathFallTable[4] = { 0x19, 0, 0xF, 0x400 };

    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.action_state = 1;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.move_speed_current = s_deathFallTable[(g_playerEntity.id & 1) * 2];
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 4;
        Play3DSnd(3, 3, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);  // death scream
        g_playerEntity.isBeingAttackedFlag = 1;
        g_playerEntity.unk_8c = 0;
        g_playerEntity.attackDirection = 0xB4;
        g_message_flags = (WORD)(g_message_flags & 0xFFBF);
        // fall through
    case 1:
        // Body thud at motion frame 0x19 (checks before Joint_move advances it).
        if ((g_playerEntity.animation_frame_id == 0x19) && (g_playerEntity.unk_bf == 1)) {
            PlayEntitySnd(2);
        }
        {
            char cVar1 = Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x400);
            g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state + cVar1);
            Add_speedXZ(s_deathFallTable[(g_playerEntity.id & 1) * 2 + 1]);
        }
        break;
    case 2:
        // Scripted-death rooms (armor room, drug storehouse, morgue)
        // skip the blood billboard and go straight to state 4.
        if ((*(unsigned short*)&g_stageId != (STAGE_MANSION_2F | (ROOM_ARMOR_ROOM << 8))) &&
            (*(unsigned short*)&g_stageId != (STAGE_GUARDHOUSE | (ROOM_DRUG_STOREHOUSE << 8))) &&
            (*(unsigned short*)&g_stageId != (STAGE_LABORATORY | (ROOM_MORGUE << 8)))) {
            BillboardSetColor(&g_playerEntity.pushVelocity, 1, 2, 0xFFFF50);
            BillboardAdjSize(&g_playerEntity.pushVelocity, -0x64, -0x64);
            g_playerEntity.action_state = 3;
            g_playerEntity.isBeingAttackedFlag = 0x80;
            return;
        }
        g_playerEntity.animationId = 4;
        return;
    case 3:
        BillboardAdjSize(&g_playerEntity.pushVelocity, 0x10, 0x10);
        g_playerEntity.attackDirection = (unsigned short)(g_playerEntity.attackDirection - 1);
        if (g_playerEntity.attackDirection == 0x20) {
            g_playerEntity.animationId = 4;
            return;
        }
        break;
    }
}

// ============================================================================
// Player state 1 — normal player control (0x00495180)
//
// This is the state the intro cutscene hands back to, and while it was a stub the
// player froze: no input mapping, no idle animation, and no route to the door
// transition. See docs/SCD_WORK_PLAN.md.
//
// Ghidra's decompilation of this function is NOT usable - it reports
// "Sanity check requires truncation of jumptable" and "Could not find normalized
// switch variable", and it folds the animFrameId=0 handler into the outer switch,
// producing a bogus "case 0 falls through to case 2". The disassembly is
// unambiguous:
//
//   0049523f: MOV AL,[0x00be6369]                 ; animFrameId (entity+0x85)
//   00495244: JMP dword ptr [EAX*0x4 + 0x4d4578]  ; jump table, NO bounds check
//
// so animFrameId selects a handler directly out of a five-entry table that begins
// at 0x004d4578 - immediately after g_playerStateFunctions ends at 0x004d4577.
// ============================================================================

// Forward declarations for the animFrameId handlers.
static void player_ctrl_frame0(void);   // 0x00495320
static void player_ctrl_frame1(void);   // 0x00495520
static void player_ctrl_frame2(void);   // 0x00495330
static void player_ctrl_frame3(void);   // 0x00495530
static void player_ctrl_frame4(void);   // 0x004955e0

// 0x004d4578 — indexed by animFrameId (entity+0x85). The original applies no mask
// and no bound; animFrameId is only ever 0-4 on this path, and the port's bound
// check below is additive so an out-of-range value reports instead of jumping into
// whatever follows the table.
static void* const g_playerCtrlFrameFunctions[5] = {
    (void*)player_ctrl_frame0,   // 0x00495320
    (void*)player_ctrl_frame1,   // 0x00495520
    (void*)player_ctrl_frame2,   // 0x00495330
    (void*)player_ctrl_frame3,   // 0x00495530
    (void*)player_ctrl_frame4,   // 0x004955e0
};

static void player_state_01_control(void)
{
    // 0x00495180: dead - fall into the death animation and stop.
    if ((short)g_playerEntity.health < 0) {
        // 0x0049518a: attackDirection == 0x7FFF flips the facing 180 degrees, so a
        // back-shot death plays turned around.
        if (g_playerEntity.attackDirection == 0x7FFF) {
            g_playerEntity.directionAngle += 0x800;
        }
        // 0x0049519e writes a dword over animationId..action_state at once, then
        // overrides action_behavior with the death index.
        g_playerEntity.animationId    = 3;
        g_playerEntity.animFrameId    = 0;
        g_playerEntity.action_state   = 0;
        g_playerEntity.action_behavior = 200;
        g_playerEntity.isBeingAttackedFlag = 1;
        // 0x004951b4: msf2 bit 0 is the "cannot die" debug/scripted guard.
        if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
            g_playerEntity.health = 1;
        }
        return;
    }

    // 0x004951cc: taking a hit pre-empts control and runs the damage state.
    if ((g_playerEntity.isBeingAttackedFlag & 0x3f) != 0) {
        g_playerEntity.action_state = 0;
        g_playerEntity.animationId  = 2;
        g_playerEntity.animFrameId  = 0;
        return;
    }

    // 0x004951e6: poison style status drain. The timer byte at
    // entity+0x174 is read BEFORE it is decremented, so the tick fires on the frame
    // the old value was already 0 (the decrement having wrapped it to 0xFF).
    if ((g_playerEntity.healthStatusFlags & 0x62) != 0) {
        unsigned char prev = g_playerEntity.pad_174;
        g_playerEntity.pad_174--;
        if (prev == 0) {
            unsigned char fast = (unsigned char)(g_playerEntity.healthStatusFlags & 0x40);
            g_playerEntity.pad_174 = fast ? 7 : 120;
            g_playerEntity.health -= 2;
            // Only the 0x40 variant is allowed to drive health negative (that is
            // the one that actually kills); everything else floors at 1.
            if ((short)g_playerEntity.health < 0 && fast == 0) {
                g_playerEntity.health = 1;
            }
        }
    }

    // 0x0049522c
    if (g_playerEntity.attackTimer != 0) {
        g_playerEntity.attackTimer--;
    }

    // 0x0049523d: dispatch on animFrameId.
    unsigned char frame = g_playerEntity.animFrameId;
    if (frame >= 5) {
        player_state_report_missing("animFrameId out of range for 0x004d4578");
        return;
    }
    ((void(*)(void))g_playerCtrlFrameFunctions[frame])();
}

// ----------------------------------------------------------------------------
// animFrameId handlers 1-4. Still to transcribe; Ghidra has no function defined at
// any of these addresses yet. Named for what selects them rather than for the
// animationId they were previously mislabelled with.
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// ============================================================================
// ChkPlReachEntity (0x00474a20)
// Returns 1 if the player, after walking 470 units straight ahead, would still be
// inside the entity's bounding box. Side-effect: zeroes the failing axis in
// g_svecScratch so the caller can decide which direction to slide the player.
//
// The bbox test uses the unsigned trick: (probeX - entX + extX) as uint must be
// <= 2*extX, which fails both for probes far left (negative wraps to huge) and
// far right (exceeds 2*extX) while accepting everything within the box.
// ============================================================================
int ChkPlReachEntity(int obj)
{
    g_svecScratch.x = 470;
    g_svecScratch.z = 0;
    MovePlayerXZ(g_playerEntity.directionAngle, &g_svecScratch, &g_svecScratch);
    g_svecScratch.x = g_svecScratch.x + (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
    g_svecScratch.z = g_svecScratch.z + (short)g_playerEntity.scaMatrixData.localMatrix.t[2];

    short extX = *(short*)(obj + 0x8a);
    short extZ = *(short*)(obj + 0x8e);

    // The offset is (probe - objectCentre + ext), NOT (objectCentre - ext +
    // probe): 0x00474a8c/0x00474a8f both SUB the object position out of the
    // extent, they do not add it in. With the operands the wrong way round the
    // sum is roughly (probe + centre), which for any object away from the world
    // origin dwarfs 2*ext, so this returned 0 for EVERY object in EVERY room -
    // silently disabling climbing and, through update_room_objects' hold
    // counter, pushing as well.
    if ((unsigned int)(extX * 2) <
        (unsigned int)((int)extX - *(int*)(obj + 0x34) + (int)g_svecScratch.x)) {
        return 0;
    }
    if ((unsigned int)(extZ * 2) <
        (unsigned int)((int)g_svecScratch.z - *(int*)(obj + 0x3c) + (int)extZ)) {
        return 0;
    }
    if (abs(g_svecScratch.z) < abs(g_svecScratch.x)) {
        g_svecScratch.z = 0;
    } else {
        g_svecScratch.x = 0;
    }
    return 1;
}

// 16-bit truncating absolute value, as in the original's (ushort) arithmetic.
static unsigned short abs16(int d)
{
    unsigned short sign = (unsigned short)(d >> 0x1f);
    return (unsigned short)(((unsigned short)d ^ sign) - sign);
}

// Defined with update_player_position below; the action probes need it first.
static int is_point_in_action_zone(VECTOR* pos, unsigned short* zone); // 0x0041b3c0

// ============================================================================
// check_climb_object (0x00474930)
// Action-key probe for climbable/pushable room objects (crates, boxes). Walks
// g_omodel_table from g_omodelCount-1 down to 0, keeping the first
// object whose first byte has flag 0x40 (climbable), that ChkPlReachEntity
// accepts, and whose facing angle is within ~26 degrees (299/4096) of the
// player's, on either wrap-around side.
//
// With msf bit 7 (0x80) already raised (mid-climb), it instead verifies the
// player still faces the remembered object - drifting away cancels, staying
// clears the bit and raises zoneFlags bit 0x10. The tail picks attackDirection
// (side) from the facing, which player_input_to_behavior turns into
// action_behavior 10.
// ============================================================================
int check_climb_object(void)
{
    if ((g_main_state_flags & MSF_DOOR_TRANSITION) == 0) {
        void** p = &g_omodel_table[(unsigned char)g_omodelCount];
        unsigned char* obj;
        do {
            // Original compares the raw byte address against &table + 1; on a
            // pointer-aligned walk that is exactly "scanned past element 0".
            if ((char*)p < (char*)g_omodel_table + 1) {
                return 0;
            }
            obj = (unsigned char*)p[-1];
            p--;

            if ((obj[0] & 0x40) == 0) continue;
            if (ChkPlReachEntity((int)obj) == 0) continue;

            int angleDiff = ((unsigned int)g_playerEntity.directionAngle + 0x800U & 0xfff) -
                            (int)*(short*)(obj + 0x74);
            unsigned short diff = abs16(angleDiff);
            if (299 < diff && diff < 0xed5) continue;

            g_main_state_flags |= MSF_DOOR_TRANSITION;
            g_playerEntity.zoneFlags &= 0xef;
            DAT_00ae9ef0 = (unsigned int)obj;
            break;
        } while (true);
    } else {
        // Mid-climb: cancel when the player has turned away from the object.
        int angleDiff = (int)g_playerEntity.directionAngle -
                        (int)*(short*)(DAT_00ae9ef0 + 0x74);
        unsigned short diff = abs16(angleDiff);
        if (299 < diff && diff < 0xed5) {
            return 0;
        }
        g_main_state_flags &= ~MSF_DOOR_TRANSITION;
        g_playerEntity.zoneFlags |= 0x10;
    }

    if ((((unsigned int)g_playerEntity.directionAngle + 0x200U) & 0x800) == 0) {
        g_playerEntity.attackDirection = 0xffff;    // -1
    } else {
        g_playerEntity.attackDirection = 1;
    }
    return 1;
}

// ============================================================================
// check_action_object (0x0041c150)
// The action-key probe of the room item/door event table. Runs the same reach
// probe as update_player_position, but only fires entries whose flag byte has
// BOTH bit 0 (mask bit, game_loop mask 1) and bit 0x80 set - the entries
// update_player_position deliberately skips (it requires 0x80 clear). That is
// the split: doors probe every frame, objects/items only on the action press.
//
// Unlike update_player_position it returns the first matching handler's result
// immediately - set_key_flag / set_room_event_flag return 1 when the entry's +2
// field is nonzero, and that nonzero is what makes player_input_to_behavior
// select the action_behavior 0xc interaction animation.
// ============================================================================
int check_action_object(void)
{
    g_svecScratch.x = 600;
    g_svecScratch.z = 0;
    MovePlayerXZ(g_playerEntity.directionAngle, &g_svecScratch, &g_svecScratch);
    g_playerPosScratch.x = g_svecScratch.x + g_playerEntity.scaMatrixData.localMatrix.t[0];
    g_playerPosScratch.z = g_svecScratch.z + g_playerEntity.scaMatrixData.localMatrix.t[2];

    unsigned char* entry = g_RoomActionTable;
    if ((unsigned char*)g_RoomActionTable - 1 < (unsigned char*)g_RoomActionTail) {
        char index = 0;
        do {
            if (*entry != 0) {
                unsigned char flags = entry[1];
                if ((flags & 1) != 0 && (flags & 0x80) != 0) {
                    if ((flags & 0x40) == 0) {
                        if (is_point_in_action_zone((VECTOR*)&g_playerPosScratch,
                                                    *(unsigned short**)(entry + 8)) != 0) {
                            g_fwdPosActionId = (unsigned char)(index + 1);
                            return ((int(*)(unsigned char*))room_check_actions[*entry])(entry);
                        }
                    } else {
                        if (is_point_in_action_zone((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t,
                                                    *(unsigned short**)(entry + 8)) != 0) {
                            g_entPosActionId = (unsigned char)(index + 1);
                            return ((int(*)(unsigned char*))room_check_actions[*entry])(entry);
                        }
                    }
                }
            }
            entry += 12;
            index++;
        } while (entry <= (unsigned char*)g_RoomActionTail);
    }
    return 0;
}

// ============================================================================
// door_transition_update (0x00495d70)
// Runs while msf bit 7 (0x80) is raised - the player is inside the door/climb
// transition. All it does is let the walk-back (behavior 4/5) override the
// locked-in action while the animation is playing; everything else is ignored.
// ============================================================================
void door_transition_update(void)
{
    switch (g_PlayerDpadHeld & 0xf) {
    case 2:   // back
        if (g_playerEntity.action_behavior != 4) {
            g_playerEntity.action_state = 0;
        }
        g_playerEntity.action_behavior = 4;
        return;
    case 8:   // left
        if (g_playerEntity.action_behavior != 5) {
            g_playerEntity.action_state = 0;
        }
        g_playerEntity.action_behavior = 5;
        return;
    }
}

// ----------------------------------------------------------------------------
// Footstep sounds.
//
// NOTE: the original calls PlayEntitySnd with TWO arguments - (0, 0) and
// (0, 0xFFFFFFFC) - but the port declares it as PlayEntitySnd(unsigned char).
// The second argument selects a footstep variant, so the port currently plays the
// same sound for both. Fixing that means widening the signature at 0x0047fbf0 and
// auditing its existing call sites; deliberately left alone here rather than
// silently dropping the parameter. Cosmetic only - it does not affect movement.
// ----------------------------------------------------------------------------
static void player_footstep_snd(int variant)
{
    (void)variant;
    PlayEntitySnd(0);
}

// ============================================================================
// player_ctrl_behavior_walk (0x00495a70)
// action_behavior 1/2/3 — walking forward. Three-step state machine: start, run,
// then release back to idle when the forward bit is let go.
//
// The 8-byte local table is per-character (Chris/Jill, `id & 1` picks the half):
//   +0 / +1  animation-frame windows that trigger a speed reduction
//   +2 / +3  how much speed to subtract in each window
// This is what makes the walk cycle slow down on footfalls.
// ============================================================================
static void player_ctrl_behavior_walk(void)
{
    static const unsigned char kWalkSpeedTable[8] = {
        0x15, 0x17, 0x0D, 0x0E,   // character 0
        0x14, 0x16, 0x0F, 0x0F,   // character 1
    };

    // 0x00495a8a: forward released -> go to the release step.
    if ((g_PlayerDpadHeld & 1) == 0) {
        g_playerEntity.action_state = 2;
    }

    if (g_playerEntity.action_state == 0) {
        g_playerEntity.attackAnim         = 2;
        g_playerEntity.action_state       = 1;
        g_playerEntity.unk_8c             = 3;
        g_playerEntity.attackDirection    = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
            player_footstep_snd(0);
        }
    } else if (g_playerEntity.action_state != 1) {
        if (g_playerEntity.action_state == 2) {
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state    = 0;
            player_footstep_snd(0);
        }
        Add_speedXZ(0);
        return;
    }

    // 0x00495b0a: dpad bit 9 (0x200) upgrades the walk into the aim-walk behaviour.
    if ((g_PlayerDpadHeld & 0x200) != 0) {
        g_playerEntity.animFrameId     = 1;
        g_playerEntity.action_behavior = 0xd;
        g_playerEntity.action_state    = 0;
    }

    unsigned char frame = g_playerEntity.animation_frame_id;
    if (g_playerEntity.attackDirection == 0) {
        if (frame == 0x08) { player_footstep_snd(0); }
        if (frame == 0x16) { player_footstep_snd(-4); }
    }

    short prevCounter = (short)g_playerEntity.attackDirection;
    unsigned int t = (unsigned int)(g_playerEntity.id & 1) * 4;

    g_playerEntity.move_speed_current = 0x5d;
    if ((unsigned char)(frame - kWalkSpeedTable[t]) < 7) {
        g_playerEntity.move_speed_current = (unsigned short)(0x5d - kWalkSpeedTable[t + 2]);
    }
    if ((unsigned char)(frame - 7) < 7) {
        g_playerEntity.move_speed_current -= kWalkSpeedTable[t + 2];
    }
    if ((unsigned char)(frame - kWalkSpeedTable[t + 1]) < 3) {
        g_playerEntity.move_speed_current -= kWalkSpeedTable[t + 3];
    }
    if ((unsigned char)(frame - 9) < 3) {
        g_playerEntity.move_speed_current -= kWalkSpeedTable[t + 3];
    }

    // msf2 bit 0 halves the speed and advances the animation only every other
    // frame - the slow-motion variant.
    if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) == 0) {
        Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
        g_playerEntity.attackDirection = 0;
    } else {
        g_playerEntity.move_speed_current /= 2;
        g_playerEntity.attackDirection--;
        if (prevCounter == 0) {
            Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
            g_playerEntity.attackDirection = 1;
        }
    }

    Add_speedXZ(0);
}

// ============================================================================
// player_ctrl_behavior_back (0x00495c90)
// action_behavior 4/5 — turning in place. The rotation itself is applied by the
// caller (player_ctrl_frame0 adds +/-0x60 to directionAngle); this drives the
// animation and, critically, releases the behaviour when the input stops.
//
// The 0x0A mask is both trigger bits at once (0x02 and 0x08), because one handler
// serves behaviours 4 and 5.
// ============================================================================
static void player_ctrl_behavior_back(void)
{
    // 0x00495c90: neither turn direction held -> release.
    if ((g_PlayerDpadHeld & 0x0A) == 0) {
        g_playerEntity.action_state = 2;
    }

    if (g_playerEntity.action_state == 0) {
        g_playerEntity.animation_frame_id  = 0;
        g_playerEntity.unk_bf              = 0;
        g_playerEntity.move_speed_current  = 1;
        g_playerEntity.action_state        = 1;
        g_playerEntity.attackAnim          = 2;
        g_playerEntity.unk_8c              = 3;
        g_playerEntity.isBeingAttackedFlag = 0;
        if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
            player_footstep_snd(0);
        }
    } else if (g_playerEntity.action_state != 1) {
        if (g_playerEntity.action_state != 2) {
            return;
        }
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state    = 0;
        player_footstep_snd(0);
        return;
    }

    if (g_playerEntity.animation_frame_id == 0x08) { player_footstep_snd(0); }
    if (g_playerEntity.animation_frame_id == 0x16) { player_footstep_snd(-4); }

    Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
}

// Declared the same way in entities/EntityCommon.h, which this file does not include.
extern unsigned char checkAngularViewAndDistance(short fovHalfAngle, short maxDistance,
                                                VECTOR* targetPos);

// ============================================================================
// player_ctrl_behavior_run (0x00495ed0)
// action_behavior 6/7/8 — running. Same three-step shape, plus an enemy-proximity
// scan: with any live, non-suppressed enemy inside a 0x200 half-angle and 8000
// units, the player keeps the guarded walk animation (attackAnim 2) instead of the
// full run (attackAnim 3).
// ============================================================================
static void player_ctrl_behavior_run(void)
{
    if ((g_PlayerDpadHeld & 4) == 0) {
        g_playerEntity.action_state = 2;
    }

    if (g_playerEntity.action_state == 0) {
        g_playerEntity.action_state        = 1;
        g_playerEntity.unk_8c             = 3;
        g_playerEntity.attackDirection    = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.isBeingAttackedFlag = 0;
        if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
            player_footstep_snd(0);
        }
    } else if (g_playerEntity.action_state != 1) {
        if (g_playerEntity.action_state == 2) {
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state    = 0;
            player_footstep_snd(0);
        }
        Add_speedXZ(0x800);
        return;
    }

    // 0x00495f1e: is any enemy in view? The original walks g_EnemiesList and only
    // decrements its remaining-count when the slot is active, so inactive slots do
    // not consume an iteration.
    // The original (0x00495f37) has NO upper bound on the walk: it advances the
    // pointer on every slot but only decrements the counter on *active* ones, so it
    // relies on g_enemy_count never exceeding the number of slots with
    // status_flags bit 0 set. If that invariant does not hold in the port the loop
    // runs off the end of the 30-slot array and never terminates. Bounded to the
    // real array size.
    unsigned char enemyInView = 0;
    {
        Entity* e = g_EnemiesList;
        char remaining = (char)g_enemy_count;
        int slot = 0;
        while (remaining != 0 && slot < 30) {
            if ((e->status_flags & 1) != 0) {
                if ((short)e->health >= 0 && (e->behavior_flags & 0x80) == 0) {
                    enemyInView |= checkAngularViewAndDistance(
                        0x200, 8000, (VECTOR*)e->scaMatrixData.localMatrix.t);
                }
                remaining--;
            }
            e++;
            slot++;
        }
    }

    unsigned char frame = g_playerEntity.animation_frame_id;
    if (enemyInView == 0) {
        if (g_playerEntity.attackAnim != 3) {
            g_playerEntity.attackAnim         = 3;
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.unk_bf             = 0;
            g_playerEntity.unk_8c             = 3;
        }
        if (g_playerEntity.attackDirection == 0 && (frame == 0x08 || frame == 0x16)) {
            player_footstep_snd(0);
        }
        // NOTE: the original's second test is `(frame > 4) || (frame < 8)`, which is
        // always true, so move_speed_current is unconditionally 0x40 here. Kept as
        // written rather than "corrected" - see the always-true-condition entries in
        // docs/SCD_WORK_PLAN.md.
        g_playerEntity.move_speed_current = 0x3c;
        if (frame > 4 || frame < 8) {
            g_playerEntity.move_speed_current = 0x40;
        }
    } else {
        if (g_playerEntity.attackAnim != 2) {
            g_playerEntity.attackAnim         = 2;
            g_playerEntity.unk_8c             = 3;
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.unk_bf             = 0;
        }
        if ((frame == 0x07 || frame == 0x1b) && g_playerEntity.unk_bf == 1 &&
            g_playerEntity.attackDirection == 0) {
            player_footstep_snd(-4);
        }
        g_playerEntity.move_speed_current = 0x3c;
    }

    short prevCounter = (short)g_playerEntity.attackDirection;
    if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) == 0) {
        Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x400);
    } else {
        g_playerEntity.move_speed_current /= 2;
        g_playerEntity.attackDirection--;
        if (prevCounter == 0) {
            Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x400);
            g_playerEntity.attackDirection = 1;
        }
    }

    Add_speedXZ(0x800);
}

// ============================================================================
// player_door_open_sequence (0x00457390)
// action_behavior 10 / 0x11 - open a door and carry the player through it. This
// is the last link in the door chain: cmd_door_set registers the zone,
// update_player_position fires check_door, check_door raises zoneFlags bit 0x20,
// player_input_to_behavior turns that into action_behavior 0x11, and this runs
// the animation and the warp. While it was a stub the player reached the door and
// simply stood in the zone forever.
//
// Four steps on action_state (entity+0x87). The original selects them with
// `CMP EAX,3 / JA default`, so the range really is 0-3 with no table:
//
//   0  turn to face the door, then advance once the residual angle is inside
//      0x3e0
//   1  pick the open animation and size the shadow quad, then FALL THROUGH to 2
//   2  advance the opening animation, firing the door SFX at fixed frames; the
//      Joint_move completion return is what advances action_state
//   3  teleport the player through the doorway, raise g_message_flags bit 0x40
//      for the room-change path, and reset to normal control
//
// Faithfulness notes, all checked against the disassembly rather than Ghidra's C:
//
//  - Case 1 falls through into case 2 (there is no jump at 0x00457504).
//  - The turn step is `(angle & 0x3fc) >> 2`, a proportional ease-in rather than a
//    fixed rate, and every angle access is 16-bit.
//  - Case 0's second turn block writes ENTITY->angle through the *global* ENTITY
//    pointer (0x00bebcd4), not &g_playerEntity. update_player_anim points ENTITY
//    at the player before dispatching so they are the same object here; written
//    the original's way rather than "corrected" onto the named field.
//  - move_speed_current (0xC2) is reused as the SFX step counter in cases 2 and 3,
//    not as a speed. The frame tests are `15*counter - animFrame == -0xc` and
//    `9*counter - animFrame == 1`, and the first is computed once from the
//    pre-increment value, before the 0x80 branch.
//  - `g_main_state_flags & 0x80` distinguishes the climb/vault entry (behaviour 10,
//    set by player_input_to_behavior) from a plain door (0x11); it selects SFX
//    0x23 over 0x2d and a different, negated displacement.
//  - zoneFlags bit 0x10 is check_door's "door swings the other way" flag: it picks
//    animation 0x35 over 0x33 and mirrors every Z displacement.
//  - g_message_flags |= 0x40 is a byte OR in the original.
//  - Case 3's reset is one dword store to 0x00be6368, covering animationId /
//    animFrameId / action_behavior / action_state together.
// ============================================================================
extern int player_distance_z;    // 0x00be0de4 - scratch, shared with the zombie code
extern int g_scaled_down_dist;   // 0x00be0de8 - scratch, shared with the zombie code

static void player_door_open_sequence(void)      // 0x00457390
{
    // 0x00457399: latched at entry, before the switch, and still the entry value
    // when case 1 falls through into case 2.
    JointStruct* joints = g_playerEntity.jointsStructs;

    switch (g_playerEntity.action_state) {
    case 0: {
        g_playerEntity.move_speed_current = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.attackAnim         = 2;
        g_playerEntity.unk_8c             = 3;
        Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);

        // 0x004573ed: the climb/vault entry turns the player using his own facing.
        if ((g_main_state_flags & MSF_DOOR_TRANSITION) != 0) {
            unsigned short a    = (unsigned short)g_playerEntity.directionAngle;
            unsigned short step = (unsigned short)((a & 0x3fc) >> 2);
            if ((a & 0x200) != 0) {
                g_playerEntity.directionAngle = (short)(unsigned short)(a + step);
            } else {
                g_playerEntity.directionAngle = (short)(unsigned short)(a - step);
            }
            g_main_state_flags2 |= MSF2_DOOR_ANGLE_STEP;
        }

        // 0x0045742f: check_door set 0x400000; consume it to pick the turn
        // direction. Bit 0x40 of zoneFlags and bit 0x400 of the angle together decide
        // whether to add or subtract - the two branches are exact mirrors.
        if ((g_main_state_flags2 & MSF2_DOOR_TURN_PENDING) != 0) {
            short*         pAngle = (short*)((unsigned char*)ENTITY + 0x74);
            unsigned short a      = (unsigned short)*pAngle;
            unsigned short step   = (unsigned short)((a & 0x3fc) >> 2);
            bool turnUp;
            if ((g_playerEntity.zoneFlags & 0x40) != 0) {
                turnUp = ((a & 0x400) == 0);
            } else {
                turnUp = ((a & 0x400) != 0);
            }
            *pAngle = (short)(unsigned short)(turnUp ? (a + step) : (a - step));
        }

        // 0x00457493: still turning - hold here until the angle settles.
        if ((g_playerEntity.directionAngle & 0x3e0) != 0) {
            return;
        }
        g_playerEntity.action_state = 1;
        return;
    }

    case 1:
        // 0x004574ac
        g_playerEntity.attackAnim         = 0x33;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        if ((g_playerEntity.zoneFlags & 0x10) != 0) {
            g_playerEntity.attackAnim = 0x35;
        }
        g_playerEntity.action_state       = 2;
        g_playerEntity.unk_8c             = 3;
        g_playerEntity.move_speed_current = 0;
        BillboardSetRect(&g_playerEntity.pushVelocity, 800, 700, 700, 700);
        // fall through

    case 2: {
        // 0x00457507: computed once, from move_speed_current before any increment.
        int frameDelta = (int)(short)g_playerEntity.move_speed_current * 15
                       - (int)g_playerEntity.animation_frame_id;

        if ((g_main_state_flags & MSF_DOOR_TRANSITION) == 0) {
            // 0x004577f6: plain door - one latch sound.
            if (frameDelta == -0xc) {
                Play3DSnd(2, 0x2d, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
                g_playerEntity.move_speed_current++;
            }
        } else {
            // 0x00457521: climb/vault - a longer cue sequence.
            if (frameDelta == -0xc) {
                Play3DSnd(2, 0x23, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
                g_playerEntity.move_speed_current++;
            }
            if (g_playerEntity.move_speed_current == 3) {
                g_playerEntity.move_speed_current = 7;
            }
            if ((g_playerEntity.move_speed_current == 7) &&
                (g_playerEntity.animation_frame_id == 0x35)) {
                Play3DSnd(2, 0x23, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
            }
            if (((g_playerEntity.zoneFlags & 0x10) != 0) &&
                (g_playerEntity.move_speed_current == 2)) {
                g_playerEntity.move_speed_current = 5;
            }
            // 0x0045759f: signed compare in the original (CMP word,4 / JLE).
            if ((short)g_playerEntity.move_speed_current > 4) {
                int stepDelta = (int)(short)g_playerEntity.move_speed_current * 9
                              - (int)g_playerEntity.animation_frame_id;
                if (stepDelta == 1) {
                    g_playerEntity.move_speed_current = 6;
                    PlayEntitySnd(0);
                }
            }
        }

        // 0x0045781f: shared tail. g_svecScratch.y is the per-frame vertical creep
        // that walks the player through the doorway as the door swings.
        g_svecScratch.x = 0;
        g_svecScratch.y = -0x29;
        g_svecScratch.z = 0;
        if ((g_playerEntity.zoneFlags & 0x10) != 0) {
            g_svecScratch.y = 0x29;
        }
        // The original tests the raw room id with NO stage guard, so this hits
        // id 5 (dining room) and id 0xE (keeper's bedroom) on the mansion 1F
        // stages AND the same ids on the other stages.
        if ((g_roomId == ROOM_KEEPERS_BEDROOM) || (g_roomId == ROOM_DINING_ROOM)) {
            g_svecScratch.y = -0x24;
            if ((g_playerEntity.zoneFlags & 0x10) != 0) {
                g_svecScratch.y = 0x24;
            }
        }
        if ((g_main_state_flags & MSF_DOOR_TRANSITION) != 0) {
            g_playerEntity.unk_e0 |= 0x40;
        }

        // 0x00457889: both subtractions are 16-bit in the original.
        g_playerEntity.pushVelocity.x =
            (short)((short)joints->world.t[0] -
                    (short)g_playerEntity.scaMatrixData.localMatrix.t[0]);
        g_playerEntity.posY = (unsigned short)(g_playerEntity.posY + g_svecScratch.y);
        g_playerEntity.pushVelocity.z =
            (short)((short)joints->world.t[2] -
                    (short)g_playerEntity.scaMatrixData.localMatrix.t[2]);

        // 0x004578cd: ADD byte ptr [action_state],AL - the animation's own
        // completion return is what moves the machine to case 3.
        g_playerEntity.action_state = (unsigned char)
            (g_playerEntity.action_state +
             (unsigned char)Joint_move(0, g_playerEntity.jointMoveData2,
                                       g_playerEntity.jointMoveData3, 0x400));
        break;
    }

    case 3: {
        // 0x004575e4: the warp.
        g_playerEntity.move_speed_current = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.unk_8c             = 0;
        g_playerEntity.attackAnim++;
        Joint_move(0, g_playerEntity.jointMoveData2, g_playerEntity.jointMoveData3, 0x400);

        // check_door stored the approach side here as +1 / -1 (signed 16-bit).
        int            dir      = (int)(short)g_playerEntity.attackDirection;
        unsigned short sideways = (unsigned short)(g_playerEntity.directionAngle & 0x400);
        unsigned char  otherWay = (unsigned char)(g_playerEntity.zoneFlags & 0x10);

        // Angle bit 0x400 means the doorway runs along Z rather than X, so the
        // displacement swaps axes.
        g_scaled_down_dist   = 0;
        g_playerDisplacement = dir * 0x10fe;
        if (sideways != 0) {
            g_playerDisplacement = 0;
            g_scaled_down_dist   = dir * 0x10fe;
        }
        player_distance_z = -0xb45;
        if (otherWay != 0) {
            player_distance_z = 0xb45;
        }

        // Rooms 5 and 0xE have shallower doorways (same unguarded raw room id
        // test as above - dining room / keeper's bedroom ids across stages).
        if ((g_roomId == ROOM_DINING_ROOM) || (g_roomId == ROOM_KEEPERS_BEDROOM)) {
            g_scaled_down_dist   = 0;
            g_playerDisplacement = dir * 0x842;
            if (sideways != 0) {
                g_playerDisplacement = 0;
                g_scaled_down_dist   = dir * 0x842;
            }
            player_distance_z = -0x57a;
            if (otherWay != 0) {
                player_distance_z = 0x57a;
            }
        }

        // 0x004576d4: the climb/vault case. Note the X displacement is negated
        // while the Z one is not - that asymmetry is in the original.
        if ((g_main_state_flags & MSF_DOOR_TRANSITION) != 0) {
            g_playerDisplacement = -(dir * 0x73a);
            g_scaled_down_dist   = 0;
            if (sideways != 0) {
                g_playerDisplacement = 0;
                g_scaled_down_dist   = dir * 0x73a;
            }
            player_distance_z = -0x708;
            if (otherWay != 0) {
                g_main_state_flags   &= ~MSF_DOOR_TRANSITION;
                g_playerEntity.unk_e0 = (unsigned short)(g_playerEntity.unk_e0 & 0xffbf);
                player_distance_z     = 0x708;
            }
            g_main_state_flags2 &= ~MSF2_DOOR_ANGLE_STEP;
        }

        g_playerEntity.scaMatrixData.localMatrix.t[0] += g_playerDisplacement;
        g_playerEntity.position.x = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
        g_playerEntity.scaMatrixData.localMatrix.t[1] += player_distance_z;
        g_playerEntity.position.y = (short)g_playerEntity.scaMatrixData.localMatrix.t[1];
        g_playerEntity.scaMatrixData.localMatrix.t[2] += g_scaled_down_dist;
        g_playerEntity.position.z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];

        BillboardSetRect(&g_playerEntity.pushVelocity, 500, 500, 700, 700);

        // 0x004577b7: byte OR. Bit 0x40 is what door_try_enter / the room-change
        // path waits for, so this is the handoff out of the door animation.
        ((unsigned char*)&g_message_flags)[0] |= 0x40;

        g_playerEntity.pushVelocity.x = 0;
        g_playerEntity.posY =
            (unsigned short)(short)g_playerEntity.scaMatrixData.localMatrix.t[1];
        g_main_state_flags2 &= ~MSF2_DOOR_TURN_PENDING;   // consume what check_door raised
        g_playerEntity.pushVelocity.z = 0;

        // 0x004577e5: one dword store back to normal control.
        g_playerEntity.animationId         = 1;
        g_playerEntity.animFrameId         = 0;
        g_playerEntity.action_behavior     = 0;
        g_playerEntity.action_state        = 0;
        g_playerEntity.isBeingAttackedFlag = 0;
        return;
    }

    default:
        break;
    }
}

// ============================================================================
// player_input_to_behavior (0x004956a0)
// Reads the D-pad and picks the next action_behavior. This is the function that
// starts the door transition: with the action button down and a door in reach it
// sets action_behavior = 10, which player_ctrl_frame0 routes to the door animation.
//
// D-pad bit layout used here: 0x80 = action/confirm, 0x100 = aim, low nibble =
// direction. The 0xC0 test accepts either 0x80 or 0xC0, i.e. action pressed with or
// without the second modifier bit.
// ============================================================================
static void player_input_to_behavior(void)
{
    unsigned int held = (unsigned int)g_PlayerDpadHeld;

    // 0x004956a0: action button newly pressed
    if ((((held & 0xc0) == 0x80) || ((held & 0xc0) == 0xc0)) &&
        ((g_PlayerDpadPressed & 0x80) != 0)) {

        // 0x004956c9: is there a door in front of the player? Sets msf bit 7,
        // which the door animation reads to pick its variant.
        if (check_climb_object() != 0) {
            g_main_state_flags |= MSF_DOOR_TRANSITION;
            g_message_flags &= 0xffbf;
            g_playerEntity.action_behavior = 10;
            g_playerEntity.action_state    = 0;
            g_playerEntity.isBeingAttackedFlag = 0x80;
            g_playerEntity.animFrameId     = 1;
            return;
        }

        // 0x004956f8: an examinable/usable object instead
        if (check_action_object() != 0) {
            g_playerEntity.healthStatusFlags |= 0x80;
            g_message_flags &= 0xffbf;
            g_playerEntity.action_behavior = 0xc;
            g_playerEntity.action_state    = 0;
            g_playerEntity.animFrameId     = 1;
            return;
        }

        // 0x00495727: standing in a stairs/ladder zone
        if ((g_playerEntity.zoneFlags & 0x20) != 0) {
            g_playerEntity.isBeingAttackedFlag = 0x80;
            g_playerEntity.animFrameId = 1;
            g_message_flags &= 0xffbf;
            if ((g_main_state_flags & MSF_LADDER_DOWN) == 0) {
                g_playerEntity.action_behavior = 0x11;
                g_playerEntity.action_state    = 0;
                g_playerEntity.isBeingAttackedFlag = 0x80;
                return;
            }
            g_playerEntity.action_behavior = 0xb;
            g_playerEntity.action_state    = 0;
            return;
        }
    }

    // 0x0049578d: already inside a door transition
    if ((g_main_state_flags & MSF_DOOR_TRANSITION) != 0) {
        door_transition_update();
        return;
    }

    // 0x0049579d: msf bit 6 forces the climb/vault behaviour
    if ((g_main_state_flags & MSF_OBJECT_PUSH) != 0) {
        g_playerEntity.animFrameId = 1;
        g_message_flags &= 0xffbf;
        g_playerEntity.action_behavior = 0x10;
        g_playerEntity.action_state    = 0;
        return;
    }

    if ((g_playerEntity.zoneFlags & 0x20) != 0) {
        g_playerEntity.isBeingAttackedFlag = 0x80;
        g_playerEntity.animFrameId = 1;
        g_message_flags &= 0xffbf;
        g_playerEntity.action_behavior = 0x11;
        g_playerEntity.action_state    = 0;
        if ((g_main_state_flags & MSF_LADDER_DOWN) != 0) {
            g_playerEntity.action_behavior = 0xb;
            g_playerEntity.action_state    = 0;
        }
        return;
    }

    // 0x004957c6: aim button. Two ranges of equippedWeaponId select the same
    // aim behaviour; the knife (id 1) uses a different animFrameId.
    if ((held & 0x100) != 0 &&
        (g_playerEntity.equippedWeaponId > 0x6e ||
         (g_playerEntity.equippedWeaponId != 0 && g_playerEntity.equippedWeaponId < 0xb))) {
        g_playerEntity.animFrameId = (g_playerEntity.equippedWeaponId == 1) ? 4 : 3;
        g_playerEntity.action_behavior = 0x12;
        g_playerEntity.action_state    = 0;
        return;
    }

    // 0x00495849: direction. The `prev` test makes a fresh press reset
    // action_state while a held direction keeps the current animation running.
    unsigned char prevLow = (unsigned char)g_PlayerDpadHeldPrev;
    switch (held & 0xf) {
    case 1:   // forward
        g_playerEntity.action_behavior = 1;
        if ((prevLow & 1) == 0) { g_playerEntity.action_state = 0; }
        break;
    case 2:   // back
        if (g_playerEntity.action_behavior != 4) {
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state    = 0;
        }
        g_playerEntity.action_behavior = 4;
        break;
    case 3:   // forward + left
        g_playerEntity.action_behavior = 2;
        if ((prevLow & 1) == 0) { g_playerEntity.action_state = 0; }
        break;
    case 4:   // right
        g_playerEntity.action_behavior = 8;
        if ((prevLow & 4) == 0) { g_playerEntity.action_state = 0; }
        break;
    case 6:
        g_playerEntity.action_behavior = 6;
        if ((prevLow & 4) == 0) { g_playerEntity.action_state = 0; }
        break;
    case 8:   // left
        if (g_playerEntity.action_behavior != 5) {
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state    = 0;
        }
        g_playerEntity.action_behavior = 5;
        break;
    case 9:
        g_playerEntity.action_behavior = 3;
        if ((prevLow & 1) == 0) { g_playerEntity.action_state = 0; }
        break;
    case 0xc:
        g_playerEntity.action_behavior = 7;
        if ((prevLow & 4) == 0) { g_playerEntity.action_state = 0; }
        break;
    default:
        break;
    }
}

// ============================================================================
// player_behavior_00_idle (0x00495960)
// action_behavior 0 — standing idle. A four-step sequence on action_state: settle
// into the idle pose, hold it for 100 frames, then blend into the looping breathe
// animation. This is what makes a standing Chris look alive rather than frozen.
//
// attackDirection is reused here as the countdown, matching the original.
// ============================================================================
static void player_behavior_00_idle(void)
{
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.action_state++;
        g_playerEntity.attackDirection      = 100;
        g_playerEntity.animation_frame_id   = 0;
        g_playerEntity.unk_bf               = 0;
        g_playerEntity.move_speed_current   = 0;
        g_playerEntity.attackAnim           = 0;
        g_playerEntity.unk_8c               = 3;
        g_playerEntity.isBeingAttackedFlag  = 0;
        // fall through
    case 1:
        Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x400);
        g_playerEntity.attackDirection--;
        // g_message_flags bit 8 (byte 1 bit 0) gates the transition into the
        // breathe loop, so it does not start mid-message.
        if (g_playerEntity.attackDirection == 0 &&
            ((((unsigned char*)&g_message_flags)[1] & 1) != 0)) {
            g_playerEntity.action_state++;
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.unk_bf             = 0;
            g_playerEntity.attackAnim         = 0;
            g_playerEntity.unk_8c             = 3;
        }
        break;
    case 2:
        if (Joint_move(0, g_playerEntity.jointMoveData0,
                       g_playerEntity.jointMoveData1, 0x400) != 0) {
            g_playerEntity.action_state++;
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.attackAnim         = 1;
            g_playerEntity.unk_8c             = 3;
            g_playerEntity.unk_bf             = 0;
        }
        break;
    case 3:
        Joint_move(0, g_playerEntity.jointMoveData0,
                   g_playerEntity.jointMoveData1, 0x400);
        break;
    default:
        break;
    }
}

// ============================================================================
// player_ctrl_frame0 (0x00495320) — animFrameId 0
// Read input, then run the handler for the resulting action_behavior via the
// full frame-2 dispatch table (player_ctrl_frame2, 0x00495330). This matters:
// player_input_to_behavior can flip animFrameId to 1 AND set a locked-in
// behavior (10 door, 0xB ladder, 0xC interact, 0x10 push) in the same frame,
// and the original runs that behavior's handler immediately - it does NOT
// re-dispatch under frame-0 semantics.
// ============================================================================
static void player_ctrl_frame0(void)
{
    player_input_to_behavior();
    player_ctrl_frame2();
}

// ============================================================================
// player_behavior_0d_run (0x00496110)
// action_behavior 0x0d — the real run, reached by holding the run modifier (D-pad
// 0x200) while walking. player_ctrl_behavior_walk hands over to it by setting
// animFrameId = 1, so it runs under player_ctrl_frame1 rather than frame0 and does
// its own input reading.
//
// Steering here is a single expression rather than the caller-applied angle tweak
// frame0 uses: 0x02 turns right by 0x30, 0x08 turns left by 0x30.
//
// action_state 3 is the exit: after four frames of the stopping animation it puts
// animFrameId and action_behavior back to 0, returning to normal control. Without
// this handler the player was stuck in animFrameId 1 forever, and because input is
// only read under frame0 that presented as a total freeze.
// ============================================================================
static void player_behavior_0d_run(void)
{
    // 0x00496110: the climb/vault flag wins over running.
    if ((g_main_state_flags & MSF_OBJECT_PUSH) != 0) {
        g_playerEntity.animFrameId     = 1;
        g_playerEntity.action_behavior = 0x10;
        g_playerEntity.action_state    = 0;
        return;
    }

    // 0x00496134: the action button still works while running.
    if ((g_PlayerDpadPressed & 0x80) != 0) {
        if (check_climb_object() != 0) {
            g_playerEntity.isBeingAttackedFlag = 0x80;
            g_main_state_flags |= MSF_DOOR_TRANSITION;
            g_message_flags &= 0xffbf;
            g_playerEntity.action_behavior = 10;
            g_playerEntity.action_state    = 0;
            g_playerEntity.animFrameId     = 1;
            return;
        }
        if (check_action_object() != 0) {
            g_playerEntity.animFrameId     = 1;
            g_message_flags &= 0xffbf;
            g_playerEntity.action_behavior = 0xc;
            g_playerEntity.action_state    = 0;
            return;
        }
        if ((g_playerEntity.zoneFlags & 0x20) != 0) {
            g_playerEntity.isBeingAttackedFlag = 0x80;
            g_playerEntity.animFrameId = 1;
            g_message_flags &= 0xffbf;
            if ((g_main_state_flags & MSF_LADDER_DOWN) == 0) {
                g_playerEntity.action_behavior = 0x11;
                g_playerEntity.action_state    = 0;
                g_playerEntity.isBeingAttackedFlag = 0x80;
                return;
            }
            g_playerEntity.action_behavior = 0xb;
            g_playerEntity.action_state    = 0;
            return;
        }
    }

    // 0x004961e6: forward released, or aiming with a weapon equipped, begins the stop.
    if (g_playerEntity.action_state < 2) {
        if ((g_PlayerDpadHeld & 1) == 0) {
            g_playerEntity.action_state = 2;
        }
        if ((g_PlayerDpadHeld & 0x100) != 0 && g_playerEntity.equippedWeaponId != 0) {
            g_playerEntity.action_state = 2;
        }
    }

    // 0x00496215: steering. Note the intermediate mask, which the original applies
    // between the two turn terms.
    {
        unsigned int a = ((unsigned int)(g_PlayerDpadHeld & 2) * 0x18
                          + (unsigned int)g_playerEntity.directionAngle) & 0xfff;
        a = (a + (unsigned int)((int)(g_PlayerDpadHeld & 8) * -6)) & 0xfff;
        g_playerEntity.directionAngle = (short)a;
    }

    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.unk_bf              = 1;
        g_playerEntity.attackAnim          = 3;
        g_playerEntity.action_state        = 1;
        g_playerEntity.unk_8c              = 3;
        g_playerEntity.isBeingAttackedFlag = 0;
        g_playerEntity.move_speed_current  = 0xd2;
        // The original parks the old frame id in the g_playerDisplacement scratch
        // global; kept as-is because it is a shared temp.
        g_playerDisplacement = (int)g_playerEntity.animation_frame_id;
        g_playerEntity.animation_frame_id = 1;
        if (g_playerDisplacement > 9 && g_playerDisplacement < 0x19) {
            g_playerEntity.animation_frame_id = 0xc;
        }
        g_playerEntity.attackDirection = 0;
        if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
            PlayEntitySnd(1);
        }
        // fall through
    case 1: {
        if (g_playerEntity.attackDirection == 0) {
            if (g_playerEntity.animation_frame_id == 0x00) { PlayEntitySnd(1); }
            if (g_playerEntity.animation_frame_id == 0x0a) { PlayEntitySnd(1); }
        }
        short prevCounter = (short)g_playerEntity.attackDirection;
        g_playerEntity.move_speed_current = 0xd2;
        if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) == 0) {
            Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
            g_playerEntity.attackDirection = 0;
        } else {
            g_playerEntity.move_speed_current = 0x69;
            g_playerEntity.attackDirection--;
            if (prevCounter == 0) {
                Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
                g_playerEntity.attackDirection = 1;
            }
        }
        // 0x004962fa: run modifier released -> hand back to the walk behaviour under
        // frame0, picking the walk frame that continues this stride.
        unsigned char frameBefore = g_playerEntity.animation_frame_id;
        if ((g_PlayerDpadHeld & 0x200) == 0) {
            g_playerEntity.animFrameId     = 0;
            g_playerEntity.action_behavior = 1;
            g_playerEntity.action_state    = 1;
            g_playerDisplacement = (int)g_playerEntity.animation_frame_id;
            g_playerEntity.animation_frame_id = 10;
            if (g_playerDisplacement != 0 && frameBefore < 0xc) {
                g_playerEntity.animation_frame_id = 0x19;
            }
            g_playerEntity.attackAnim = 2;
            g_playerEntity.unk_8c     = 3;
        }
        break;
    }
    case 2:
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.attackAnim         = 0;
        g_playerEntity.action_state       = 3;
        g_playerEntity.unk_8c             = 3;
        g_playerEntity.unk_bc             = 0;
        // fall through
    case 3:
        Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x400);
        g_playerEntity.unk_bc++;
        if (g_playerEntity.unk_bc > 3) {
            g_playerEntity.action_behavior    = 0;
            g_playerEntity.action_state       = 0;
            g_playerEntity.move_speed_current = 0;
            g_playerEntity.animFrameId        = 0;   // back to normal control
            PlayEntitySnd(1);
        }
        g_playerEntity.move_speed_current -= 0x1e;
        break;
    default:
        break;
    }

    Add_speedXZ(0);
}

// ============================================================================
// Action-key interaction behaviors (0x00495e00 - 0x00496480)
//
// These run under animFrameId 1/2 once player_input_to_behavior has locked the
// player into an interaction. 0x0c is the generic "use/examine object" reach
// animation; 0x0b is the full ladder climb (walk up, turn, climb, descend);
// 0x10 is the push/climb-over object animation.
// ============================================================================

// 0x004d45cc - ladder step-sound frames, one byte per step. The original reads
// this UNBOUNDED (a byte-indexed CMP); the meaningful entries are 12, 29, 39,
// then 80, 100, 130, ... and the tail is zero-padded so a late read matches
// nothing, exactly like the surrounding .data does.
static const unsigned char g_ladderStepFrames[64] = {
    0x0c, 0x1d, 0x27, 0x00, 0x50, 0x64, 0x82, 0x64,
    0x6b, 0x68, 0x00, 0x64, 0x64, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x64, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// 0x004567d0 - write the screen-distortion effect struct at 0x00be63c8. The
// consumer (0x00456a10 camera scroll) is not ported yet, so this only writes
// the struct; the effect stays inert until that function lands.
static void set_screen_effect_struct(int effect, unsigned short p2, short p3,
                                     unsigned short p4, short p5)
{
    *(short*)(effect + 0x58) = -p3;
    *(unsigned short*)(effect + 0x5c) = p4;
    *(unsigned short*)(effect + 0x60) = p2;
    *(unsigned short*)(effect + 0x64) = p4;
    *(short*)(effect + 0x68) = -p3;
    *(short*)(effect + 0x6c) = -p5;
    *(unsigned short*)(effect + 0x70) = p2;
    *(short*)(effect + 0x74) = -p5;
}

// ============================================================================
// player_behavior_0c_interact (0x00495e00) — action_behavior 0x0c
// The generic object interaction animation (attackAnim 4). Plays the reach
// animation; when it completes, raises msf 0x100 for entry id 0x0d
// (set_room_event_flag) or msf 0x800 for anything else — those bits gate the
// message system into the follow-up prompt. Then the state sits in 2 until the
// frame machinery re-enters and releases back to idle.
// ============================================================================
static void player_behavior_0c_interact(void)
{
    if (g_playerEntity.action_state == 0) {
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.attackAnim = 4;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.action_state = 1;
    } else if (g_playerEntity.action_state == 1) {
        if (Joint_move(0, g_playerEntity.jointMoveData0,
                       g_playerEntity.jointMoveData1, 0x400) != 0) {
            if (*(char*)g_pRoomActionEntry == 0x0d) {
                g_main_state_flags |= MSF_PICKUP_SCREEN;
            } else {
                g_main_state_flags |= MSF_MENU_MODE_ITEM_VIEW;
            }
            g_playerEntity.action_state = 2;
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.attackAnim = 0;
            ((unsigned char*)&g_message_flags)[0] |= 0x40;
            g_playerEntity.healthStatusFlags &= 0x7f;
            g_playerEntity.unk_8c = 0;
        }
    } else if (g_playerEntity.action_state == 2) {
        Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x400);
        g_playerEntity.animFrameId = 0;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 0;
    }
}

// ============================================================================
// player_behavior_10_push (0x00457230) — action_behavior 0x10
// Push / climb-over animation (attackAnim 0x30). Walks the player forward while
// msf bit 6 (0x40, the forced-push flag) stays raised, plays a grunt on frame 1
// whose sound id depends on the pushed object's id byte, then releases control.
// ============================================================================
static void player_behavior_10_push(void)
{
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.attackAnim = 0x30;
        g_playerEntity.action_state = 1;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        // fall through
    case 1:
        if (Joint_move(0, g_playerEntity.jointMoveData2,
                       g_playerEntity.jointMoveData3, 0x400) != 0) {
            g_playerEntity.unk_bf = 0;
            g_playerEntity.action_state = 2;
            g_playerEntity.unk_8c = 3;
            g_playerEntity.move_speed_current = 0;
            g_playerEntity.attackAnim++;
        }
        break;
    case 2:
        Joint_move(0, g_playerEntity.jointMoveData2,
                   g_playerEntity.jointMoveData3, 0x400);
        if (g_playerEntity.animation_frame_id < 0x10) {
            g_playerEntity.move_speed_current = 0x32;
            Add_speedXZ(0);
        }
        if ((g_main_state_flags & MSF_OBJECT_PUSH) == 0) {
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.unk_bf = 0;
            g_playerEntity.move_speed_current = 0;
            g_playerEntity.attackAnim++;
            g_playerEntity.action_state = 3;
            g_playerEntity.unk_8c = 3;
        }
        if (g_playerEntity.animation_frame_id == 1) {
            // object id byte bit 0x40 picks the grunt variant (0x16/0x17).
            // DAT_00ae9ee8, the object update_room_objects is pushing - NOT
            // DAT_00ae9ef0, which is check_climb_object's separate scratch.
            unsigned char sndId = (unsigned char)(
                0x17 - ((*(unsigned char*)(DAT_00ae9ee8 + 1) & 0x40) == 0));
            Play3DSnd(2, sndId, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
            return;
        }
        break;
    case 3:
        if (Joint_move(0, g_playerEntity.jointMoveData2,
                       g_playerEntity.jointMoveData3, 0x400) != 0) {
            ((unsigned char*)&g_message_flags)[0] |= 0x40;
            g_playerEntity.animationId = 1;
            g_playerEntity.animFrameId = 0;
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state = 0;
            return;
        }
        break;
    }
}

// ============================================================================
// player_behavior_0b_ladder (0x00496480) — action_behavior 0x0b
// The ladder climb, selected when the player presses action inside a ladder
// zone (msf bit 4 set by set_stairs_zone). Eight states: walk up to the ladder
// (0/1), turn to face it (2), start the climb animation (3), climb with
// step-sounds and a camera effect (4), descend setup and walk back (5-7), then
// release control (8). zoneFlags bit 0x10 selects the ladder variant (0x35 anim)
// over the plain stairs/doors one (0x33).
// ============================================================================
extern void entity_rotate_toward_target(VECTOR* pos, unsigned short angleStep); // 0x004899b0

static void player_behavior_0b_ladder(void)
{
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.action_state = 1;
        g_playerEntity.move_speed_current = 0x5d;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 2;
        g_playerEntity.unk_8c = 3;
        // fall through
    case 1:
        {
            VECTOR target;
            target.x = (int)g_playerEntity.unk_c6;   // ladder base X
            target.z = (int)g_playerEntity.unk_c8;   // ladder base Z
            target.y = 0;
            entity_rotate_toward_target(&target, 0x40);
            Joint_move(0, g_playerEntity.jointMoveData0,
                       g_playerEntity.jointMoveData1, 0x200);
            Add_speedXZ(0);
            int dx = g_playerEntity.scaMatrixData.localMatrix.t[0] - (int)g_playerEntity.unk_c6;
            int dz = g_playerEntity.scaMatrixData.localMatrix.t[2] - (int)g_playerEntity.unk_c8;
            if (SquareRoot0(dz * dz + dx * dx) < 900) {
                g_playerEntity.action_state = 2;
                return;
            }
        }
        break;
    case 2:
        Joint_move(0, g_playerEntity.jointMoveData0,
                   g_playerEntity.jointMoveData1, 0x200);
        // Rotate the remaining angle difference (masked to 0x3fc) onto 0.
        {
            unsigned int turn;
            if ((g_playerEntity.directionAngle & 0x400U) == 0) {
                turn = (g_playerEntity.directionAngle & 0x3fcU) >> 2;
            } else {
                turn = (unsigned int)-((g_playerEntity.directionAngle & 0x3fcU) >> 2);
            }
            g_playerEntity.directionAngle =
                (short)((unsigned int)g_playerEntity.directionAngle + turn);
            if ((g_playerEntity.directionAngle & 0x3e0U) == 0) {
                g_playerEntity.action_state = 3;
                return;
            }
        }
        break;
    case 3:
        g_playerEntity.attackAnim = 0x33;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        if ((g_playerEntity.zoneFlags & 0x10) != 0) {
            g_playerEntity.attackAnim = 0x35;
        }
        g_playerEntity.action_state = 4;
        g_playerEntity.move_speed_current = 0;
        g_playerEntity.unk_8c = 3;
        set_screen_effect_struct((int)DAT_00be63c8, 800, 700, 700, 700);
        // fall through
    case 4:
        {
            unsigned char sndId = 0x2d;
            if ((g_playerEntity.zoneFlags & 0x10) == 0) {
                // step-sound frames on the plain climb
                if (g_ladderStepFrames[g_playerEntity.move_speed_current] ==
                    g_playerEntity.animation_frame_id) {
                    Play3DSnd(2, 0x23, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
                    g_playerEntity.move_speed_current++;
                }
                if (g_playerEntity.animation_frame_id != 0x32) {
                    goto ladder_step_done;
                }
                sndId = 0x2d;
            } else {
                // ladder variant: reposition on frame 0x0f, grunt at 0x1a
                if ((g_playerEntity.animation_frame_id == 0x0f) &&
                    ((g_playerEntity.zoneFlags & 0x10) != 0)) {
                    g_playerDisplacement = 0xfffff8f8;
                    g_playerEntity.posY = 0xa8c;
                    if (0x800 < g_playerEntity.directionAngle) {
                        g_playerDisplacement = 0x708;
                    }
                    g_playerEntity.pushVelocity.z += (short)g_playerDisplacement;
                }
                if (g_playerEntity.animation_frame_id != 0x1a) {
                    goto ladder_step_done;
                }
                sndId = 0x17;
            }
            Play3DSnd(2, sndId, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
ladder_step_done:
            g_playerEntity.action_state += Joint_move(
                0, g_playerEntity.jointMoveData2, g_playerEntity.jointMoveData3, 0x400);
            return;
        }
    case 5:
        // Descend: step back down the ladder, then walk away.
        g_playerEntity.move_speed_current = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.unk_8c = 0;
        g_playerEntity.attackAnim++;
        Joint_move(0, g_playerEntity.jointMoveData2,
                   g_playerEntity.jointMoveData3, 0x400);
        g_scaled_down_dist = -1000;
        if (0x800 < g_playerEntity.directionAngle) {
            g_scaled_down_dist = 1000;
        }
        g_playerEntity.scaMatrixData.localMatrix.t[1] = 0;
        if ((g_playerEntity.zoneFlags & 0x10) != 0) {
            g_scaled_down_dist = -2000;
            if (0x800 < g_playerEntity.directionAngle) {
                g_scaled_down_dist = 2000;
            }
            g_playerEntity.scaMatrixData.localMatrix.t[1] = 0xa8c;
        }
        g_playerEntity.position.x = (short)g_playerEntity.scaMatrixData.localMatrix.t[0];
        g_playerEntity.position.y = (short)g_playerEntity.scaMatrixData.localMatrix.t[1];
        g_playerEntity.scaMatrixData.localMatrix.t[2] += g_scaled_down_dist;
        g_playerEntity.position.z = (short)g_playerEntity.scaMatrixData.localMatrix.t[2];
        set_screen_effect_struct((int)DAT_00be63c8, 500, 500, 700, 700);
        g_playerEntity.pushVelocity.x = 0;
        g_playerEntity.posY = (unsigned short)g_playerEntity.scaMatrixData.localMatrix.t[1];
        g_playerEntity.pushVelocity.z = 0;
        if ((g_playerEntity.zoneFlags & 0x10) != 0) {
            g_playerEntity.action_state = 8;
            return;
        }
        // fall through
    case 6:
        g_playerEntity.action_state = 7;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.move_speed_current = 0x3c;
        g_playerEntity.attackAnim = 2;
        g_playerEntity.flags |= 4;
        g_playerEntity.attackDirection = 0xf;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        break;
    case 7:
        if (g_playerEntity.animation_frame_id == 8) {
            PlayEntitySnd(0);
        }
        Joint_move(0, g_playerEntity.jointMoveData0,
                   g_playerEntity.jointMoveData1, 0x400);
        Add_speedXZ(0);
        if (g_playerEntity.attackDirection == 0) {
            g_playerEntity.action_state = 8;
            g_playerEntity.flags &= 0xfb;
            PlayEntitySnd(0);
            return;
        }
        g_playerEntity.attackDirection--;
        break;
    case 8:
        g_playerEntity.zoneFlags &= 0xef;
        g_main_state_flags &= ~MSF_LADDER_DOWN;
        ((unsigned char*)&g_message_flags)[0] |= 0x40;
        g_playerEntity.isBeingAttackedFlag = 0;
        g_playerEntity.animationId = 1;
        g_playerEntity.animFrameId = 0;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 0;
        return;
    }
}

// ============================================================================
// player_ctrl_frame1 (0x00495520) — animFrameId 1
// A bare table dispatch on action_behavior:
//   00495522: MOV AL,[0x00be636a]                  ; action_behavior
//   00495527: JMP dword ptr [EAX*0x4 + 0x4d456c]
// Unlike frame0 there is no input read and no caller-applied angle tweak - these
// are the locked-in actions, and each handler reads input itself if it needs to.
//
// Table dump (dwords at 0x004d456c, base for this dispatcher):
//   0x09 -> 0x00495df0 (empty)   0x10 -> 0x00457230
//   0x0a -> 0x00457390           0x11 -> 0x00457390
//   0x0b -> 0x00496480           0x12 -> NULL
//   0x0c -> 0x00495e00           0x13 -> 0x00459370
//   0x0d -> 0x00496110           0x14 -> 0x004594c0
//   0x0e -> 0x00496470 (empty)   0x15 -> 0x004596c0
//   0x0f -> 0x00496470 (empty)   0x16 -> 0x00459960
//                                0x17 -> 0x004599f0
//
// Note the knife block: under frameId 1 the entries sit ONE index lower than
// under frameId 4's table (base 0x004d4570), so behavior 0x13 runs the AIM
// handler (0x459370) here, not the hold. This table shares dwords with the
// frame-4 table by design; each dispatcher reads it with its own base.
// ============================================================================
static void player_behavior_12_knife_aim(void);    // 0x00459370
static void player_behavior_13_knife_hold(void);   // 0x004594c0
static void player_behavior_14_knife_swing(void);  // 0x004596c0
static void player_behavior_15_knife_holster(void);// 0x00459960
static void player_behavior_16_knife_turn(void);   // 0x004599f0

static void player_ctrl_frame1(void)
{
    switch (g_playerEntity.action_behavior) {
    case 0x09:                       // 0x004d4590 -> 0x00495df0 (empty in the original)
        return;
    case 0x0a:                       // 0x004d4594 -> 0x00457390
    case 0x11:                       // 0x004d45b0 -> 0x00457390
        player_door_open_sequence();
        return;
    case 0x0b:                       // 0x004d4598 -> 0x00496480
        player_behavior_0b_ladder();
        return;
    case 0x0c:                       // 0x004d459c -> 0x00495e00
        player_behavior_0c_interact();
        return;
    case 0x0d:                       // 0x004d45a0 -> 0x00496110
        player_behavior_0d_run();
        return;
    case 0x0e:                       // 0x004d45a4 -> 0x00496470 (empty in the original)
    case 0x0f:                       // 0x004d45a8 -> 0x00496470 (empty in the original)
        return;
    case 0x10:                       // 0x004d45ac -> 0x00457230
        player_behavior_10_push();
        return;
    case 0x13:                       // 0x004d45b8 -> 0x00459370 (knife aim)
        player_behavior_12_knife_aim();
        return;
    case 0x14:                       // 0x004d45bc -> 0x004594c0 (knife hold)
        player_behavior_13_knife_hold();
        return;
    case 0x15:                       // 0x004d45c0 -> 0x004596c0 (knife swing)
        player_behavior_14_knife_swing();
        return;
    case 0x16:                       // 0x004d45c4 -> 0x00459960 (knife holster)
        player_behavior_15_knife_holster();
        return;
    case 0x17:                       // 0x004d45c8 -> 0x004599f0 (knife turn)
        player_behavior_16_knife_turn();
        return;
    default:
        // 0x004d4570/0x4d45b4 (behaviors 2, 8 and 0x12 from this base) are NULL:
        // the original would fault on a wild jump, so these indices never occur.
        // The bounds check in player_state_01_control covers indices past 0x17,
        // which land in non-pointer data. Report instead of jumping anywhere.
        player_state_report_missing("action_behavior under animFrameId 1 (0x004d456c)");
        return;
    }
}
// ============================================================================
// Aim / fire subsystem (0x004578f0 - 0x0045a650)
//
// animFrameId 3 (guns) and 4 (knife) host the whole weapon state machine. The
// original dispatches action_behavior 0x12-0x1A (guns, table at 0x004955b8)
// and 0x12-0x16 (knife, table at 0x004d4570) into the handlers below; every
// handler plays motions through Joint_move on the WEAPON buffers
// (jointMoveData0/1), never the body's animHeader/animBase.
//
// Two aim-direction copies exist, exactly as in the original: weaponAimFlags
// (entity+0x176) drives the raise/hold/fire motion selection, while the flags
// byte (entity+0x00) is what apply_weapon_damage's cone filter reads. The
// knife writes the flags byte directly; the guns write weaponAimFlags, and
// the auto-aim pitch (auto_aim_pitch_update) refreshes the flags byte from
// the target at fire time. The shared 0x00be0dfc scratch (g_animFrameIdSave)
// doubles as the manual-fire reverse flag and the lock-on turn angle, which
// never conflict because the frame dispatcher sets it immediately before use.
// ============================================================================

extern unsigned char apply_weapon_damage(unsigned int weapon_id);   // 0x0043c020
extern int  get_item_slot(unsigned char itemId);                    // 0x004516a0
extern int  turn_toward_target(VECTOR* target_pos, short angle_step);          // 0x00489960
extern void entity_rotate_toward_target(VECTOR* pos, unsigned short angleStep);// 0x004899b0

// 0x00456560 - special-weapon frame window clamp. Returns 1 when the current
// frame is outside the per-(character,motion) window at 0x004c0cc0, and
// clamps it: mode 0 holds at end-1, mode 1 rewinds to the window start.
static unsigned int weapon_special_frame_update(int step, int mode)
{
    unsigned int uVar3 = (unsigned int)(step + 1) & 3;
    int iVar1 = ((unsigned int)(g_playerEntity.id & 1) * 3 + (g_playerEntity.attackAnim - 1 & 3)) * 4;
    unsigned int frame = (unsigned int)g_playerEntity.animation_frame_id;

    if (frame < g_weaponSpecialFrameWindows[step + iVar1]
        || g_weaponSpecialFrameWindows[iVar1 + uVar3] <= frame) {
        if (mode == 0) {
            g_playerEntity.animation_frame_id =
                (unsigned char)(g_weaponSpecialFrameWindows[iVar1 + uVar3] - 1);
        } else if (mode == 1) {
            g_playerEntity.animation_frame_id =
                (unsigned char)g_weaponSpecialFrameWindows[step + iVar1];
        }
        return 1;
    }
    return 0;
}

// ============================================================================
// auto_aim_pitch_update @ 0x0045a370
// Composes the weapon/head joint chain (joints 0, 9, 10, 11) to find the aim
// target height, then sets the flags byte's aim direction (0x20 up, 0x40
// neutral, 0x80 down) from the per-character height table at 0x004c0fc0.
// Runs at fire time (weapon 6 via the frame-3 tail, all weapons at fire
// frame 2) so the hit cone matches the locked target's height.
// ============================================================================
static void auto_aim_pitch_update(void)
{
    JointStruct* joints = ENTITY->jointsStructs;

    RotMatrix((SVECTOR*)&ENTITY->position.pad, &ENTITY->scaMatrixData.localMatrix);
    ApplyLVAndMul0Matrix(&ENTITY->scaMatrixData.localMatrix, &joints[0].transform, &g_matrixScratch);
    ApplyLVAndMulMatrix(&g_matrixScratch, &joints[9].transform);
    ApplyLVAndMulMatrix(&g_matrixScratch, &joints[10].transform);
    ApplyLVAndMulMatrix(&g_matrixScratch, &joints[11].transform);

    unsigned char bVar2 = g_playerEntity.flags & 0x1f;
    g_playerEntity.flags = (g_playerEntity.flags & 0x1f) | 0x40;
    if (g_playerEntity.equippedWeaponId != 10) {
        int t = (g_playerEntity.id & 1) * 6;
        short low  = g_aimHeightTable[t + 0] + (short)g_playerEntity.scaMatrixData.localMatrix.t[1];
        short high = g_aimHeightTable[t + 1] + (short)g_playerEntity.scaMatrixData.localMatrix.t[1];
        if (g_playerEntity.equippedWeaponId < 6 && g_playerEntity.equippedWeaponId != 3) {
            low  = g_aimHeightTable[t + 2] + (short)g_playerEntity.scaMatrixData.localMatrix.t[1];
            high = g_aimHeightTable[t + 3] + (short)g_playerEntity.scaMatrixData.localMatrix.t[1];
        }
        if (g_playerEntity.equippedWeaponId > 0x6e) {
            low  = g_aimHeightTable[t + 4] + (short)g_playerEntity.scaMatrixData.localMatrix.t[1];
            high = g_aimHeightTable[t + 5] + (short)g_playerEntity.scaMatrixData.localMatrix.t[1];
        }
        if (g_matrixScratch.t[1] < low) {
            g_playerEntity.flags = bVar2 | 0x80;
        }
        if (high < g_matrixScratch.t[1]) {
            g_playerEntity.flags = (g_playerEntity.flags & 0x1f) | 0x20;
        }
    }
}

// ============================================================================
// weapon_autoaim_check @ 0x0045a4b0
// The auto-aim fire gate: returns the ammo count (masked to 0x7f) when the
// equipped slot still has rounds, and 0 when empty. The knife never passes.
// The special weapons (id >= 0x6f) and the infinite-ammo flag (player flag
// bit 0x7e, id 10) are topped back up to 4. The Director's Cut adds a second
// infinite branch (flag 0x7a, id 5 = the Colt Python Magnum) that refills to 6.
// Also read by the effect system's auto-aim flash (behavior 58, EffectSystem.cpp).
// ============================================================================
unsigned char weapon_autoaim_check(void)
{
    if (g_EquippedItemId == 0) return 0;

    unsigned char* slot = (unsigned char*)g_ItemSlotsPointer + (g_EquippedItemId - 1) * 2;
    unsigned char itemId = slot[0];
    unsigned char qty = slot[1];

    if (itemId == ITEM_KNIFE) return 0; // knife has no ammo
    if (itemId == ITEM_FLAMETHROWER) return qty; // flamethrower can have 255 ammo
    if ((qty & 0x7f) != 0) return qty & 0x7f; // limit ammo to 127

    if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_INF_R_LAUNCHER) != 0 && itemId == 10) {
        slot[1] = 4;
        return 4;
    }
    // Director's Cut (PS1 SLUS_005.51 0x8004228c): the ADVANCED best-ending
    // unlock refills the magnum's cylinder to its full 6 rounds when empty, so
    // it never runs out. The DC's copy returns here without the PC build's
    // special-weapon refill below; the port keeps that PC behaviour for every
    // other weapon and adds only this branch.
    if (dc_is_infinite_colt_python(itemId)) {
        slot[1] = 6;
        return 6;
    }
    if (itemId < ITEM_INGRAM) return 0;
    slot[1] = 4;
    return 4;
}

// 0x0045a530 - does the player hold the weapon's ammo item? The original
// looks up (weaponId + 9) in the inventory and returns slot+1 (0 = absent).
static char weapon_fire_check(void)
{
    return (char)(get_item_slot(weapon_ammo_item_id(g_playerEntity.equippedWeaponId)) + 1);
}

// ============================================================================
// player_aim_cone_test @ 0x00496be0
// Does the ray from the player through `delta` cross a sight-blocking room
// boundary? Walks ALL quadrant lists (group[0]..group[4]) - unlike
// room_check_sight_blocked, which takes one quadrant - and runs the same
// two-diagonal straddle test on every record whose flags mask to 0x300 (type
// 4/5 never block). No Yawn exception here: the player's id is never 13/18.
// ============================================================================
static unsigned int player_aim_cone_test(VECTOR* delta)
{
    if (g_RdtPointer == NULL || g_RdtPointer->boundaries == NULL) return 0;

    RDT_BoundaryHeader* hdr = (RDT_BoundaryHeader*)g_RdtPointer->boundaries;
    RDT_Boundary* first = hdr->group[0];
    RDT_Boundary* last  = hdr->group[4];

    int playerX = g_playerEntity.scaMatrixData.localMatrix.t[0] / 18;
    int playerZ = g_playerEntity.scaMatrixData.localMatrix.t[2] / 18;
    int dirX = delta->x / 18;
    int dirZ = delta->z / 18;

    for (RDT_Boundary* rec = first; rec < last; rec++) {
        unsigned short blocking = (unsigned short)(rec->flags & 0x300);
        rec->flags = blocking;            // the original writes the mask back
        if (blocking != 0x300) continue;
        if (rec->type == 4 || rec->type == 5) continue;

        int xMax = (int)(rec->xMax / 18u);
        int zMax = (int)(rec->zMax / 18u);
        int xMin = (int)(rec->xMin / 18u);
        int zMin = (int)(rec->zMin / 18u);

        // ---- diagonal 1 (xMax,zMin) -> (xMin,zMax), with the VectorNormal ----
        VECTOR edge, p0, p1;
        edge.x = xMin - xMax; edge.y = 0; edge.z = zMax - zMin;
        p1.x = (playerX + dirX) - xMax; p1.y = 0; p1.z = (playerZ + dirZ) - zMin;
        p0.x = playerX - xMax; p0.y = 0; p0.z = playerZ - zMin;
        vectorMul3(&edge, &p1, &p1);
        vectorMul3(&edge, &p0, &p0);
        if ((((unsigned int)p0.y ^ (unsigned int)p1.y) & 0x80000000u) != 0) {
            p1.x = xMin - playerX; p1.y = 0; p1.z = zMin - playerZ;
            vectorMul3(delta, &p1, &p1);
            p0.x = xMax - playerX; p0.y = 0; p0.z = zMax - playerZ;
            VectorNormal(&p0, &p0);
            vectorMul3(delta, &p0, &p0);
            if ((((unsigned int)p0.y ^ (unsigned int)p1.y) & 0x80000000u) != 0) {
                return 1;
            }
        }

        // ---- diagonal 2 (xMin,zMin) -> (xMax,zMax), no normalize ----
        edge.x = xMax - xMin; edge.y = 0; edge.z = zMax - zMin;
        p1.x = (playerX + dirX) - xMin; p1.y = 0; p1.z = (playerZ + dirZ) - zMin;
        p0.x = playerX - xMin; p0.y = 0; p0.z = playerZ - zMin;
        vectorMul3(&edge, &p1, &p1);
        vectorMul3(&edge, &p0, &p0);
        if ((((unsigned int)p0.y ^ (unsigned int)p1.y) & 0x80000000u) != 0) {
            p1.x = xMax - playerX; p1.y = 0; p1.z = zMax - playerZ;
            vectorMul3(delta, &p1, &p1);
            p0.x = xMin - playerX; p0.y = 0; p0.z = zMin - playerZ;
            vectorMul3(delta, &p0, &p0);
            if ((((unsigned int)p0.y ^ (unsigned int)p1.y) & 0x80000000u) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

// ============================================================================
// player_find_aim_target @ 0x00496ae0
// Walks the enemy list forward from the current lock (unk_b8) and locks the
// first alive, script-free enemy inside the aim cone. Returns 1 on lock.
// ============================================================================
static unsigned int player_find_aim_target(void)
{
    unsigned char idx = 0;
    Entity* ent = g_EnemiesList;
    while ((Entity*)g_playerEntity.unk_b8 != ent) {   // find the current lock
        idx++;
        ent = &g_EnemiesList[idx];
    }

    unsigned char next = (unsigned char)(idx + 1);
    ent = (Entity*)(g_playerEntity.unk_b8 + 0x18c);   // first candidate: after the lock
    char count = g_enemy_count;
    if (next == 30) {
        next = 0;
        ent = g_EnemiesList;
    }
    do {
        while (true) {
            if (count == 0) return 0;
            if ((ent->status_flags & 1) != 0) break;
            next++;
            ent++;
            if (next == 30) {
                next = 0;
                ent = g_EnemiesList;
            }
        }
        if (ent->has_enter_switch_zone != 0 && -1 < ent->health
            && (ent->behavior_flags & 0xc0) == 0 && ent->id < 0x13) {
            VECTOR delta;
            delta.x = ent->scaMatrixData.localMatrix.t[0]
                    - g_playerEntity.scaMatrixData.localMatrix.t[0];
            delta.y = 0;
            delta.z = ent->scaMatrixData.localMatrix.t[2]
                    - g_playerEntity.scaMatrixData.localMatrix.t[2];
            if (player_aim_cone_test(&delta) == 0) {
                g_playerEntity.unk_b8 = (unsigned int)ent;
                return 1;
            }
        }
        next++;
        ent++;
        count--;
    } while (true);
}

// ============================================================================
// player_reticle_enemy @ 0x00496910
// The aim reticle scan (live because g_aimReticleEnabled is 1 in the exe):
// finds the nearest enemy in front, tracking standing and low (head below 1)
// candidates separately. The knife locks the nearest; guns prefer the low
// candidate (the last write wins). Sets unk_b8 and returns 1 when a target
// was found.
// ============================================================================
static unsigned int player_reticle_enemy(void)
{
    char count = g_enemy_count;
    Entity* ent = g_EnemiesList;
    unsigned char cVar3 = 0;
    unsigned char nearestIdx = 0, lowIdx = 0, standingIdx = 0;
    unsigned int nearestDist = 0x7fffffff, lowDist = 0x7fffffff, standingDist = 0x7fffffff;

    while (count != 0) {
        if ((ent->status_flags & 1) != 0) {
            if (ent->has_enter_switch_zone != 0 && -1 < ent->health
                && (ent->behavior_flags & 0xc0) == 0 && ent->id < 0x13) {
                VECTOR delta;
                delta.x = ent->scaMatrixData.localMatrix.t[0]
                        - g_playerEntity.scaMatrixData.localMatrix.t[0];
                delta.y = 0;
                delta.z = ent->scaMatrixData.localMatrix.t[2]
                        - g_playerEntity.scaMatrixData.localMatrix.t[2];
                g_scaled_down_dist = SquareRoot0(delta.z * delta.z + delta.x * delta.x);
                if ((short)turn_toward_target((VECTOR*)ent->scaMatrixData.localMatrix.t, 0x800) == 0
                    && player_aim_cone_test(&delta) == 0) {
                    if (g_scaled_down_dist < nearestDist) {
                        nearestIdx = (unsigned char)(cVar3 + 1);
                        nearestDist = g_scaled_down_dist;
                    }
                    if (ent->scaMatrixData.localMatrix.t[1] < 1) {
                        if (g_scaled_down_dist < lowDist) {
                            lowIdx = (unsigned char)(cVar3 + 1);
                            lowDist = g_scaled_down_dist;
                        }
                    } else if (g_scaled_down_dist < standingDist) {
                        standingIdx = (unsigned char)(cVar3 + 1);
                        standingDist = g_scaled_down_dist;
                    }
                }
            }
            count--;
        }
        cVar3++;
        ent++;
    }

    if (nearestIdx == 0) {
        g_playerEntity.unk_b8 = (unsigned int)g_EnemiesList;
        return 0;
    }
    if (g_playerEntity.equippedWeaponId == 1) {
        g_playerEntity.unk_b8 = (unsigned int)(&g_EnemiesList[nearestIdx - 1]);
        return 1;
    }
    if (standingIdx != 0) {
        g_playerEntity.unk_b8 = (unsigned int)(&g_EnemiesList[standingIdx - 1]);
    }
    if (lowIdx != 0) {
        g_playerEntity.unk_b8 = (unsigned int)(&g_EnemiesList[lowIdx - 1]);
    }
    return 1;
}

// ============================================================================
// weapon_lockon_effect @ 0x0045a650
// Auto-aim lock-on visual: a fan of target billboards (type 5, depth 0x12,
// sprite g_deadMoveValue) jittered around the current camera position, aimed
// at the camera. The shotgun sprays four, everything else one. Runs when the
// auto-aim raise starts for weapons 2-5.
// ============================================================================
static void weapon_lockon_effect(void)
{
    if (g_playerEntity.equippedWeaponId > 5) return;
    unsigned char isShotgun = (g_playerEntity.equippedWeaponId == 3) ? 1 : 0;

    // Camera record for the current camera, 8 dwords at RDT+0x9C
    int* cam = (int*)((char*)g_RdtPointer + (unsigned int)g_roomCameraId * 0x2c + 0x9c);
    int v[8];
    for (int i = 0; i < 8; i++) v[i] = cam[i];

    int dz = v[3] - v[0];          // camera span Z
    int dx = v[5] - v[2];          // camera span X

    g_animFrameIdSave = ((dx > 0) ? 0x800 : 0) + 0x400;
    if (dz != 0) {
        g_animFrameIdSave = ((dz < 0) ? 2 : 1) * 0x800
                            - (unsigned int)GetAngleQuadrantValue((dx * 0x1000) / dz);
    }

    // Aiming fan: rotate (isShotgun+1)*0xa0 by the camera angle
    JointStruct* joints = g_playerEntity.jointsStructs;
    g_svecScratch.x = (short)((isShotgun + 1) * 0xa0);
    g_svecScratch.y = 0;
    g_svecScratch.z = 0;
    g_matrixScratch = g_identityMatrixData;
    RotMatrixY((int)g_animFrameIdSave + 0x400, &g_matrixScratch);
    ApplyMatrixSV(&g_matrixScratch, &g_svecScratch, &g_svecScratch);

    int wx = joints[0xe].world.t[0];
    int wy = joints[0xe].world.t[1];
    int wz = joints[0xe].world.t[2];

    VECTOR edgeL, edgeR, aimVec;
    edgeL.x = (v[3] - wx + g_svecScratch.x) / 0x12;
    edgeL.z = (v[2] - wz + g_svecScratch.z) / 0x12;
    edgeL.y = (v[1] - wy + (int)((isShotgun * 5 + 5) * 0x20)) / 0x12;
    edgeR.x = (v[0] - wx - g_svecScratch.x) / 0x12;
    edgeR.z = (v[2] - wz - g_svecScratch.z) / 0x12;
    edgeR.y = (v[1] - wy + (int)((isShotgun * 5 - 5) * 0x20)) / 0x12;

    g_svecScratch.x = (short)(isShotgun * 400);
    g_svecScratch.y = 1000;
    g_svecScratch.z = 0;
    ApplyMatrix(&joints[0xe].world, &g_svecScratch, &aimVec);
    vectorMul3(&aimVec, &edgeL, &edgeL);
    vectorMul3(&aimVec, &edgeR, &edgeR);

    // The fan only draws when the camera is inside the wedge
    if (((unsigned int)edgeL.y & 0x80000000u) == 0) return;
    if (((unsigned int)edgeR.y & 0x80000000u) != 0) return;
    if ((((unsigned int)edgeR.x ^ (unsigned int)edgeL.x) & 0x80000000u) == 0) return;
    if ((((unsigned int)edgeR.z ^ (unsigned int)edgeL.z) & 0x80000000u) == 0) return;

    // Jittered billboards around the camera position
    g_playerPosScratch.x = dz;
    g_playerPosScratch.y = v[5] - v[3];
    g_playerPosScratch.z = dx;
    VectorNormal(&g_playerPosScratch, &g_playerPosScratch);

    g_playerPosScratch.x = (g_playerPosScratch.x / 8 - (rand() & 0xff)) + v[0] + 0x80;
    g_playerPosScratch.y = (g_playerPosScratch.y / 8 - (rand() & 0xff)) + v[1] + 0x80;
    g_playerPosScratch.z = (g_playerPosScratch.z / 8 - (rand() & 0xff)) + v[2] + 0x80;
    Effect_CreateBillboard(0x05, 0x12, 0, (void*)g_deadMoveValue, &g_playerPosScratch, 0);

    if (isShotgun) {
        for (int i = 0; i < 3; i++) {
            g_playerPosScratch.x += 0x100 - (rand() & 0x1ff);
            g_playerPosScratch.y += 0x100 - (rand() & 0x1ff);
            g_playerPosScratch.z += 0x100 - (rand() & 0x1ff);
            Effect_CreateBillboard(0x05, 0x12, 0, (void*)g_deadMoveValue, &g_playerPosScratch, 0);
        }
    }
}

// ============================================================================
// player_behavior_12_gun_aim @ 0x004578f0 - action_behavior 0x12
// The gun aim pose. State 0 sets up the pose (motion 5), runs the reticle
// scan and arms weaponAimState (|2). The shared tail plays the raise motion,
// steers with the D-pad sides, and quick-fires (behavior 0x15, anim-only)
// with the up/down bits - clearing the flags byte's aim direction as it
// does. Special weapons clamp their frame window instead of quick-firing.
// ============================================================================
static void player_behavior_12_gun_aim(void)
{
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.action_state = 1;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.move_speed_current = 1;
        g_playerEntity.isBeingAttackedFlag = 0;
        g_playerEntity.attackAnim = 5;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.weaponAimFlags = 0;
        if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
            PlayEntitySnd(0);
        }
        if (g_aimReticleEnabled != 0) {
            g_playerEntity.weaponAimState = (unsigned short)player_reticle_enemy();
        }
        g_playerEntity.weaponAimState |= 2;
        break;
    case 1:
        break;
    case 2:
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state = 0;
        return;
    default:
        return;
    }

    // 0x0045797f: reticle follow - a fresh fire press drops the lock; a
    // locked target (state 3) makes the player turn toward it.
    if (g_aimReticleEnabled != 0) {
        if ((g_button_pressed_id & 4) != 0) {
            g_playerEntity.weaponAimState = 0;
        }
        if (g_playerEntity.weaponAimState == 3) {
            g_animFrameIdSave = (unsigned int)(g_playerEntity.id & 1) * 0x20 + 0xf0;
            if ((short)turn_toward_target((VECTOR*)(g_playerEntity.unk_b8 + 0x34), 0x200) == 0) {
                g_animFrameIdSave = ((g_playerEntity.id & 1) + 6) * 0x20;
            }
            if ((g_playerEntity.id & 1) == 0 && g_playerEntity.equippedWeaponId == 2) {
                g_animFrameIdSave += 0x20;
            }
            entity_rotate_toward_target((VECTOR*)(g_playerEntity.unk_b8 + 0x34),
                                        (unsigned short)g_animFrameIdSave);
        }
    }

    unsigned char ret = (unsigned char)Joint_move(0, g_playerEntity.jointMoveData0,
                                                  g_playerEntity.jointMoveData1, 0x400);
    g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state + ret);

    if ((g_PlayerDpadHeld & 2) != 0) {
        g_playerEntity.directionAngle += (short)((g_playerEntity.id & 1) * -0x10 + 0x48);
    }
    if ((g_PlayerDpadHeld & 8) != 0) {
        g_playerEntity.directionAngle += (short)((g_playerEntity.id & 1) * 0x10 - 0x48);
    }
    if (g_playerEntity.equippedWeaponId == 10) return;

    // Fire gate: Chris may quick-fire once the raise pose has passed frame 4
    // (weapons 2/4/5); everyone else after frame 9.
    if ((((g_playerEntity.id & 1) == 0 && 4 < g_playerEntity.animation_frame_id)
         && (g_playerEntity.equippedWeaponId == 2 || g_playerEntity.equippedWeaponId == 4
             || g_playerEntity.equippedWeaponId == 5))
        || 9 < g_playerEntity.animation_frame_id) {
        if ((g_PlayerDpadHeld & 4) != 0) {   // up quick-fire
            g_playerEntity.weaponAimFlags &= 0x1f;
            g_playerEntity.flags &= 0x1f;
            g_playerEntity.weaponAimFlags |= 0x20;
            g_playerEntity.action_behavior = 0x15;
            g_playerEntity.action_state = 0;
            g_playerEntity.attackAnim = (g_playerEntity.equippedWeaponId < 0x6f) ? 0xb : 7;
            return;
        }
        if ((g_PlayerDpadHeld & 1) != 0) {   // down quick-fire
            g_playerEntity.weaponAimFlags &= 0x1f;
            g_playerEntity.flags &= 0x1f;
            g_playerEntity.weaponAimFlags |= 0x80;
            g_playerEntity.action_behavior = 0x15;
            g_playerEntity.action_state = 0;
            g_playerEntity.attackAnim = (g_playerEntity.equippedWeaponId < 0x6f) ? 8 : 6;
            return;
        }
    }

    if (0x6e < g_playerEntity.equippedWeaponId && weapon_special_frame_update(0, 0) != 0) {
        g_playerEntity.action_state = 2;
    }
}

// ============================================================================
// player_behavior_13_gun_raise @ 0x00457b80 - action_behavior 0x13 (part 1)
// The raise/lower pose machine. State 0 starts the raise (motion dir*3+7),
// state 1 blends it, state 2 holds the weapon pose (motion weapon*3+dir+2),
// and state 3 blends back to the body idle (animHeader/animBase). Special
// weapons use dir+5 motions and clamp with weapon_special_frame_update.
// ============================================================================
static void player_behavior_13_gun_raise(void)
{
    if (g_playerEntity.action_state >= 4) return;

    char dir = (char)(g_playerEntity.weaponAimFlags >> 7);
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.action_state = 1;
        g_playerEntity.unk_bf = 0;
        // 0x00457bc1: the direction term ADDS the down bit (SHR/ADD in the
        // original). The Ghidra decompile shows "upbit - dir" only because it
        // treats bPad_176 as signed (0x80 >> 7 = -1); the raw assembly at
        // 0x00457bc1 is ADD DL,AL with an unsigned 0/1. The SUB form computes
        // motion 4 for aim-down - the pickup animation - instead of 10.
        g_playerEntity.attackAnim =
            (unsigned char)((((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir) * 3 + 7);
        if (g_playerEntity.equippedWeaponId > 0x6e) {
            g_playerEntity.animation_frame_id = 0xe;
            g_playerEntity.attackAnim =
                (unsigned char)(((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir + 5);
        }
        g_playerEntity.unk_8c = 3;
        g_playerEntity.move_speed_current = 0;
        // fall through: the raise starts this frame
    case 1:
        Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
        if (g_playerEntity.equippedWeaponId > 0x6e) {
            weapon_special_frame_update(0, 0);
        }
        break;
    case 2:
        if (g_playerEntity.unk_8c == 0) {
            g_playerEntity.action_state = 3;
            g_playerEntity.unk_bf = 0;
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.move_speed_current = 1;
            g_playerEntity.unk_8c = 7;
            // 0x00457c55: ADD as in state 0 - the down direction is +1, so
            // the hold-down motion is weapon*3+3 (9 for the handgun), not
            // weapon*3+1 (7, the neutral raise).
            g_playerEntity.attackAnim = (unsigned char)
                (g_playerEntity.equippedWeaponId * 3 + (((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir) + 2);
            if (g_playerEntity.equippedWeaponId > 0x6e) {
                g_playerEntity.attackAnim =
                    (unsigned char)(((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir + 5);
                g_playerEntity.animation_frame_id = 0xf;
                weapon_special_frame_update(0, 0);
            }
            if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
                PlayEntitySnd(0);
            }
        } else {
            Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x200);
            if (g_playerEntity.equippedWeaponId > 0x6e) {
                weapon_special_frame_update(0, 0);
            }
        }
        break;
    case 3:
        if (g_playerEntity.unk_8c != 0) {
            if (g_playerEntity.equippedWeaponId > 0x6e) {
                Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x200);
                g_playerEntity.animation_frame_id = 0xf;
                weapon_special_frame_update(0, 0);
            } else {
                Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x200);
            }
        } else {
            g_playerEntity.action_state = 2;
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.unk_bf = 0;
            g_playerEntity.move_speed_current = 1;
            g_playerEntity.unk_8c = 7;
            g_playerEntity.attackAnim =
                (unsigned char)((((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir) * 3 + 7);
            if (g_playerEntity.equippedWeaponId > 0x6e) {
                g_playerEntity.attackAnim =
                    (unsigned char)(((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir + 5);
                g_playerEntity.animation_frame_id = 0xf;
                weapon_special_frame_update(0, 0);
            }
        }
        break;
    }
}

// ============================================================================
// player_behavior_13_gun_hold_input @ 0x00457de0 - action_behavior 0x13 (part 2)
// The aim-hold input handler. Refreshes weaponAimFlags (0x40 neutral, 0x10/
// 0x20 bits aim down/up), quick-fires on a direction change (behavior 0x15)
// or direction release (0x16, reverse anim), holsters on aim release (0x17),
// starts the auto-aim fire (0x14) or the magnum-FX fire (0x18), locks a
// special-weapon target (0x1a), and steers with the D-pad sides.
// ============================================================================
static void player_behavior_13_gun_hold_input(void)
{
    unsigned char old = g_playerEntity.weaponAimFlags;
    g_animFrameIdSave = (unsigned int)old;
    g_playerEntity.weaponAimFlags = (g_playerEntity.weaponAimFlags & 0x1f) | 0x40;
    if ((g_PlayerDpadHeld & 0x10) != 0) {
        g_playerEntity.weaponAimFlags = (g_playerEntity.weaponAimFlags & 0x1f) | 0x80;
    }
    if ((g_PlayerDpadHeld & 0x20) != 0) {
        g_playerEntity.weaponAimFlags = (g_playerEntity.weaponAimFlags & 0x1f) | 0x20;
    }

    // A direction change quick-fires in the new direction. The direction term
    // ADDS the down bit: 0x00457e5d is ADD DL,AL, so down yields motion 8
    // (0+1)*3+5, up motion 11 (2+0)*3+5. An earlier port subtracted the bits
    // (the Ghidra decompile's "- dir" artifact of signed >>7), which made the
    // down quick-fire play motion 2 - the run animation.
    if (((g_playerEntity.weaponAimFlags ^ old) & 0xa0) != 0) {
        g_playerEntity.action_behavior = 0x15;
        g_playerEntity.action_state = 0;
        int aimDir = (int)((g_playerEntity.weaponAimFlags & 0x20) >> 4)
                   + (int)(g_playerEntity.weaponAimFlags >> 7);
        g_playerEntity.attackAnim = (g_playerEntity.equippedWeaponId < 0x6f)
            ? (unsigned char)(aimDir * 3 + 5) : (unsigned char)(aimDir + 5);
    }
    // Neutral -> aimed: fire; aimed -> neutral: reverse fire anim.
    if ((old & 0x40) != 0 && (g_playerEntity.weaponAimFlags & 0xa0) != 0) {
        g_playerEntity.action_behavior = 0x15;
        g_playerEntity.action_state = 0;
        int aimDir = (int)((g_playerEntity.weaponAimFlags & 0x20) >> 4)
                   + (int)(g_playerEntity.weaponAimFlags >> 7);
        g_playerEntity.attackAnim = (g_playerEntity.equippedWeaponId < 0x6f)
            ? (unsigned char)(aimDir * 3 + 5) : (unsigned char)(aimDir + 5);
    }
    if ((old & 0xa0) != 0 && (g_playerEntity.weaponAimFlags & 0x40) != 0) {
        g_playerEntity.action_behavior = 0x16;
        g_playerEntity.action_state = 0;
        // 0x00457f09: LEA [down + up*2] - ADD semantics, so the down release
        // also plays motion 8, not 2 (the run).
        int aimDir = (int)(old >> 7 & 1) + (int)(old >> 5 & 1) * 2;
        g_playerEntity.attackAnim = (g_playerEntity.equippedWeaponId < 0x6f)
            ? (unsigned char)(aimDir * 3 + 5)
            : (unsigned char)(((g_playerEntity.weaponAimFlags & 0x20) >> 4)
                              + (g_playerEntity.weaponAimFlags >> 7) + 5);
    }

    // Aim button released -> holster.
    if ((g_PlayerDpadHeld & 0x100) == 0) {
        g_playerEntity.action_behavior = 0x17;
        g_playerEntity.action_state = 0;
        return;
    }

    // Fire button: auto-aim (with ammo), else the magnum-family FX fire.
    if ((g_PlayerDpadHeld & 0x40) != 0) {
        if (weapon_autoaim_check() != 0) {
            g_playerEntity.action_behavior = 0x14;
            g_playerEntity.action_state =
                (g_playerEntity.equippedWeaponId == 6 || g_playerEntity.equippedWeaponId >= 0x6f) ? 3 : 0;
            return;
        }
        if ((g_PlayerDpadPressed & 0x40) != 0) {
            // DC (PS1 SLUS_005.51 0x8003f234): the empty click is dropped while
            // the magnum is the ADVANCED-unlocked infinite one.
            if (!dc_is_infinite_colt_python(
                    *(unsigned char*)((unsigned char*)g_ItemSlotsPointer
                                      + g_EquippedItemId * 2 - 2))) {
                Play3DSnd(1, 9, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
            }
            if (weapon_fire_check() != 0 && g_playerEntity.equippedWeaponId < 6) {
                g_playerEntity.action_behavior = 0x18;   // 0x00458ec0 - not yet transcribed
                g_playerEntity.action_state = 0;
                return;
            }
        }
    }
    // Raw pad bit 4 with a target lock starts the special lock-on fire.
    if ((g_PlayerPadHeld & 0x40) != 0 && player_find_aim_target() != 0) {
        g_playerEntity.action_behavior = 0x1a;           // 0x00459150 - not yet transcribed
        g_playerEntity.action_state = 0;
        return;
    }

    // Steering: the turn bits drop the raise blend back to state 2.
    if ((g_PlayerDpadHeld & 2) != 0) {
        g_playerEntity.directionAngle += (short)((g_playerEntity.id & 1) * -0x10 + 0x48);
        if (g_playerEntity.action_state < 2) {
            g_playerEntity.action_state = 2;
            g_playerEntity.unk_8c = 0;
        }
        return;
    }
    if ((g_PlayerDpadHeld & 8) != 0) {
        g_playerEntity.directionAngle += (short)((g_playerEntity.id & 1) * 0x10 - 0x48);
        if (g_playerEntity.action_state < 2) {
            g_playerEntity.action_state = 2;
            g_playerEntity.unk_8c = 0;
        }
        return;
    }
    if (g_playerEntity.action_state > 1) {
        g_playerEntity.action_state = 0;
    }
}

// ============================================================================
// player_behavior_14_autoaim_raise @ 0x00458090 - auto-aim fire, state 0
// Sets the auto-aim raise pose (motion (dir+2)*3), spawns the lock-on fan
// billboards, then runs the fire state in the same frame.
// ============================================================================
static void player_behavior_14_autoaim_fire(void);
static void player_behavior_14_autoaim_raise(void)
{
    g_playerEntity.action_state = 1;
    g_playerEntity.animation_frame_id = 0;
    g_playerEntity.move_speed_current = 1;
    g_playerEntity.unk_8c = 3;
    g_playerEntity.attackAnim = (unsigned char)
        ((((g_playerEntity.weaponAimFlags & 0x20) >> 4) + (g_playerEntity.weaponAimFlags >> 7) + 2) * 3);
    weapon_lockon_effect();
    if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
        PlayEntitySnd(1);
    }
    player_behavior_14_autoaim_fire();
}

// ============================================================================
// player_behavior_14_autoaim_fire @ 0x004580f0 - auto-aim fire, state 1
// The core firing state. Plays the fire motion; on the ammo frame decrements
// the equipped slot and spawns the muzzle billboard; on the fire frame calls
// apply_weapon_damage with the table weapon id and plays both fire sounds;
// spawns the big muzzle flash and second flash; the shotgun fires twice.
// Returns to the hold state after the end frame once the aim is released.
// ============================================================================
static void player_behavior_14_autoaim_fire(void)
{
    int weaponIdx = (int)g_playerEntity.equippedWeaponId - 2;   // table index, weapons 2..11

    unsigned char ret = (unsigned char)Joint_move(0, g_playerEntity.jointMoveData0,
                                                  g_playerEntity.jointMoveData1, 0x400);
    g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state + ret);
    if (g_playerEntity.animation_frame_id == 2) {
        auto_aim_pitch_update();
    }

    // ---- ammo decrement + muzzle billboard (g_weaponFireBillboard b0 = frame)
    if (g_weaponFireBillboard[weaponIdx].b0 == g_playerEntity.animation_frame_id) {
        ((unsigned char*)g_ItemSlotsPointer)[g_EquippedItemId * 2 - 1] -= 1;

        if (weaponIdx == 8) {   // special weapon: alternate billboard entry
            unsigned char ammo = weapon_autoaim_check();
            int e = (ammo & 3) + weaponIdx;
            g_collPushDepthZHi = e;
            g_playerPosScratch.x = g_weaponFireBillboard[e].x;
            g_playerPosScratch.y = g_weaponFireBillboard[e].y;
            g_playerPosScratch.z = g_weaponFireBillboard[e].z;
            Effect_CreateBillboard(g_weaponFireBillboard[e].type, g_weaponFireBillboard[e].data,
                                   0, &g_playerEntity.jointsStructs[0xe].world,
                                   &g_playerPosScratch, 0);
        } else {
            g_playerPosScratch.x = g_weaponFireBillboard[weaponIdx].x;
            g_playerPosScratch.y = g_weaponFireBillboard[weaponIdx].y;
            g_playerPosScratch.z = g_weaponFireBillboard[weaponIdx].z;
            Effect_CreateBillboard(g_weaponFireBillboard[weaponIdx].type,
                                   g_weaponFireBillboard[weaponIdx].data, 0,
                                   &g_playerEntity.jointsStructs[0xe].world,
                                   &g_playerPosScratch, 0);
            if (weaponIdx == 2) {   // python: extra spark
                g_playerPosScratch.x = 0x96;
                g_playerPosScratch.y = 0x17c;
                g_playerPosScratch.z = 0;
                Effect_CreateBillboard(0x11, 0x03, 0, &g_playerEntity.jointsStructs[0xe].world,
                                       &g_playerPosScratch, 0);
            }
            if (weaponIdx == 3) {   // magnum: big flash at the joint
                g_playerPosScratch.x = 0x96;
                g_playerPosScratch.y = 0x17c;
                g_playerPosScratch.z = 0;
                Effect_CreateBillboard(0x11, 0x0b, 0, &g_playerEntity.jointsStructs[0xe].world,
                                       &g_playerPosScratch, 0);
            }
        }
    }

    // ---- damage + fire sounds
    if (g_weaponFireData[weaponIdx].fireFrame == g_playerEntity.animation_frame_id) {
        if (weaponIdx < 6) {
            apply_weapon_damage(g_weaponFireData[weaponIdx].weaponId);
        }
        Play3DSnd(1, g_weaponFireData[weaponIdx].sfx1, 0,
                  (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        Play3DSnd(1, g_weaponFireData[weaponIdx].sfx2, 0,
                  (int)&g_playerEntity.scaMatrixData.localMatrix.t);
    }

    // ---- big muzzle flash
    if (g_weaponMuzzleFlash[weaponIdx].b0 == g_playerEntity.animation_frame_id) {
        g_playerPosScratch.x = g_weaponMuzzleFlash[weaponIdx].x;
        int yOff = (1 - weaponIdx) * (g_playerEntity.id & 1) * 300;
        g_playerPosScratch.z = g_weaponMuzzleFlash[weaponIdx].z;
        if (weaponIdx == 8) {
            yOff = (g_playerEntity.id & 1) * 500;
        }
        g_playerPosScratch.y = yOff + g_weaponMuzzleFlash[weaponIdx].y;

        // 0x0045837b: yaw = (weaponIdx-8) + CF(weaponIdx-8 < 1) - 1, masked 0x555
        int yaw = weaponIdx - 8;
        yaw += ((unsigned short)yaw < 1) ? 1 : 0;
        yaw -= 1;

        int fx = (int)(char)Effect_CreateBillboard(
            g_weaponMuzzleFlash[weaponIdx].type, g_weaponMuzzleFlash[weaponIdx].data,
            (short)(yaw & 0x555), &g_playerEntity.scaMatrixData.localMatrix,
            &g_playerPosScratch, 0);
        g_playerDisplacement = fx;
        if (weaponIdx == 8) {
            g_effectPool[fx].animHeader[0] = (unsigned char)weaponIdx;
        } else {
            g_effectPool[fx].animHeader[3] = (unsigned char)weaponIdx;
        }
    }

    // ---- second flash
    if (g_weaponFlash2[weaponIdx].b0 == g_playerEntity.animation_frame_id) {
        int fx;
        if (weaponIdx == 8) {
            unsigned char ammo = weapon_autoaim_check();
            int e = (ammo & 3) + weaponIdx;
            g_collPushDepthZHi = e;
            g_playerPosScratch.x = g_weaponFlash2[e].x;
            g_playerPosScratch.y = g_weaponFlash2[e].y;
            g_playerPosScratch.z = g_weaponFlash2[e].z;
            fx = (int)(char)Effect_CreateBillboard(g_weaponFlash2[e].type, g_weaponFlash2[e].data,
                                                   0, &g_playerEntity.jointsStructs[0xe].world,
                                                   &g_playerPosScratch, 0);
            g_collPushDepthZLo = fx;
        } else {
            g_playerPosScratch.x = g_weaponFlash2[weaponIdx].x;
            g_playerPosScratch.y = g_weaponFlash2[weaponIdx].y;
            g_playerPosScratch.z = g_weaponFlash2[weaponIdx].z;
            fx = (int)(char)Effect_CreateBillboard(g_weaponFlash2[weaponIdx].type,
                                                   g_weaponFlash2[weaponIdx].data, 0,
                                                   &g_playerEntity.jointsStructs[0xe].world,
                                                   &g_playerPosScratch, 0);
            g_collPushDepthZHi = fx;
        }
        g_effectPool[fx].animHeader[0] = (unsigned char)weaponIdx;
    }

    // ---- shotgun fires twice (frames 7 and 9)
    if (weaponIdx == 1
        && (g_playerEntity.animation_frame_id == 7 || g_playerEntity.animation_frame_id == 9)) {
        apply_weapon_damage(g_weaponFireData[weaponIdx].weaponId);
    }

    // ---- past the end frame with the aim released -> back to the hold
    if (g_weaponFireEndFrame[weaponIdx] < g_playerEntity.animation_frame_id
        && (g_PlayerDpadHeld & 0x100) == 0) {
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state = 0;
    }

    // ---- shotgun shell-rack sound two frames before the end
    if ((unsigned char)(g_weaponFireEndFrame[weaponIdx] - g_playerEntity.animation_frame_id) == 2
        && weaponIdx == 1) {
        Play3DSnd(1, 0x0b, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
    }

    // ---- shotgun: released fire direction -> neutral hold
    if (g_weaponFireEndFrame[weaponIdx] < g_playerEntity.animation_frame_id && weaponIdx == 1
        && (((g_playerEntity.weaponAimFlags & 0xa0) != 0 && (g_PlayerDpadHeld & 5) == 0)
            || ((g_playerEntity.weaponAimFlags & 0x80) != 0 && (g_PlayerDpadHeld & 1) == 0)
            || ((g_playerEntity.weaponAimFlags & 0x20) != 0 && (g_PlayerDpadHeld & 4) == 0))) {
        g_playerEntity.weaponAimFlags = (g_playerEntity.weaponAimFlags & 0x1f) | 0x40;
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state = 0;
    }
}

// FUN_00458680 - auto-aim state 4 body: play the re-raise motion. State 3 arms
// the raise pose then calls this; the motion loop advances state 4 -> 5.
static void player_behavior_14_autoaim_raise2(void)
{
    char ret = (char)Joint_move(0, g_playerEntity.jointMoveData0,
                                g_playerEntity.jointMoveData1, 0x400);
    g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state + ret);
    if (g_playerEntity.equippedWeaponId > 0x6e) {
        g_playerEntity.action_state++;
    }
}

// FUN_00458c90 - auto-aim state 9 body: play the fire motion REVERSED (the
// gun recovers to the raised pose after a quick-fire). State 8 arms the pose
// then calls this; weaponAimFlags bit 0 is the reverse marker written by the
// hold-fire direction logic (the Joint_move reverse arg is flags | 1, so the
// playback is always reversed here). The motion loop advances 9 -> 10.
static void player_behavior_14_autoaim_reverse(void)
{
    char ret = (char)Joint_move((char)(g_playerEntity.weaponAimFlags | 1),
                                g_playerEntity.jointMoveData0,
                                g_playerEntity.jointMoveData1, 0x400);
    g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state + ret);
    if (g_playerEntity.equippedWeaponId > 0x6e) {
        g_playerEntity.action_state += (unsigned char)weapon_special_frame_update(0, 2);
    }
    if ((g_PlayerDpadHeld & 5) != 0) {
        g_playerEntity.action_state = 5;
    }
}

// FUN_00458720 - auto-aim state 6 body: the hold-fire loop. Decrements ammo
// (click every 15 frames, small flash every 4 for normal weapons; interval
// fire for specials), then quick-fires on a direction change (state 8,
// forward) or plays the reversed recover (state 8, flags | 1), or keeps
// Joint_move'ing when nothing changed - releasing fire drops back to the hold.
static void player_behavior_14_autoaim_holdfire(void)
{
    unsigned char old = g_playerEntity.weaponAimFlags;
    g_animFrameIdSave = (unsigned int)old;
    g_playerEntity.weaponAimFlags = (g_playerEntity.weaponAimFlags & 0x1f) | 0x40;
    if ((g_PlayerDpadHeld & 0x10) != 0) {
        g_playerEntity.weaponAimFlags = (g_playerEntity.weaponAimFlags & 0x1f) | 0x80;
    }
    if ((g_PlayerDpadHeld & 0x20) != 0) {
        g_playerEntity.weaponAimFlags = (g_playerEntity.weaponAimFlags & 0x1f) | 0x20;
    }

    if (weapon_autoaim_check() == 0) {
        Play3DSnd(1, 3, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state = 0;
        return;
    }

    unsigned char frame = g_playerEntity.animation_frame_id;
    if (g_playerEntity.equippedWeaponId < 0x6f) {
        // normal weapons: click every 15 frames, small flash every 4, ammo--
        if (frame % 0xf == 0) {
            Play3DSnd(1, 3, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
            Play3DSnd(1, 4, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        }
        if ((frame & 3) == 0) {
            g_playerPosScratch.x = 0x21c;
            g_playerPosScratch.y = 0x4ec;
            g_playerPosScratch.z = 0;
            Effect_CreateBillboard(0xc, 0, 0, &g_playerEntity.jointsStructs[0xe].world,
                                   &g_playerPosScratch, 0);
        }
        ((unsigned char*)g_ItemSlotsPointer)[g_EquippedItemId * 2 - 1] -= 1;
    } else {
        // special weapons: interval fire while held. The table index is the
        // weapon id shifted by -99 (0x6f -> entry 12), which is why the FX
        // tables have 14 entries.
        int idx = (int)g_playerEntity.equippedWeaponId - 99;
        if (frame % g_weaponFireIntervals[0] == 0) {
            auto_aim_pitch_update();
            g_playerPosScratch.x = g_weaponFireBillboard[idx].x;
            g_playerPosScratch.y = g_weaponFireBillboard[idx].y;
            g_playerPosScratch.z = g_weaponFireBillboard[idx].z;
            Effect_CreateBillboard(g_weaponFireBillboard[idx].type,
                                   g_weaponFireBillboard[idx].data, 0,
                                   &g_playerEntity.jointsStructs[0xe].world,
                                   &g_playerPosScratch, 0);
            apply_weapon_damage(g_weaponFireData[idx].weaponId);
            Play3DSnd(1, g_weaponFireData[idx].sfx1, 0,
                      (int)&g_playerEntity.scaMatrixData.localMatrix.t);
            Play3DSnd(1, g_weaponFireData[idx].sfx2, 0,
                      (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        }
        if (frame % g_weaponFireIntervals[1] == 0) {
            g_playerPosScratch.x = g_weaponMuzzleFlash[idx].x;
            g_playerPosScratch.y = g_weaponMuzzleFlash[idx].y;
            g_playerPosScratch.z = g_weaponMuzzleFlash[idx].z;
            int fx = (int)(char)Effect_CreateBillboard(
                g_weaponMuzzleFlash[idx].type, g_weaponMuzzleFlash[idx].data, 0x555,
                &g_playerEntity.scaMatrixData.localMatrix, &g_playerPosScratch, 0);
            g_playerDisplacement = fx;
            if (idx == 8) {
                g_effectPool[fx].animHeader[0] = 8;
            } else {
                g_effectPool[fx].animHeader[3] = 0;
            }
        }
        if (frame % g_weaponFireIntervals[2] == 0) {
            g_playerPosScratch.x = g_weaponFlash2[idx].x;
            g_playerPosScratch.y = g_weaponFlash2[idx].y;
            g_playerPosScratch.z = g_weaponFlash2[idx].z;
            int fx = (int)(char)Effect_CreateBillboard(
                g_weaponFlash2[idx].type, g_weaponFlash2[idx].data, 0,
                &g_playerEntity.jointsStructs[0xe].world, &g_playerPosScratch, 0);
            g_collPushDepthZHi = fx;
            g_effectPool[fx].animHeader[0] = 0;
        }
        if (g_weaponFireEndFrame[idx] < frame && (g_PlayerDpadHeld & 0x100) == 0) {
            g_playerEntity.action_behavior = 0x13;
            g_playerEntity.action_state = 0;
        }
    }

    // Direction-change quick-fires / reversed recover. weaponAimFlags bit 0
    // marks the reverse playback for the state-9 Joint_move.
    char dir = (char)(g_playerEntity.weaponAimFlags >> 7);
    if ((old & 0x40) != 0 && (g_playerEntity.weaponAimFlags & 0xa0) != 0) {
        // neutral -> aimed: quick-fire forward (0x00458a8f: ADD DL,AL)
        g_playerEntity.action_state = 8;
        g_playerEntity.attackAnim = (unsigned char)
            ((((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir) * 3 + 5);
        if (g_playerEntity.equippedWeaponId > 0x6e) {
            g_playerEntity.attackAnim = (unsigned char)
                (((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir + 5);
            weapon_special_frame_update(1, 1);
        }
        g_playerEntity.weaponAimFlags &= 0xfe;
        return;
    }
    if ((old & 0xa0) != 0 && (g_playerEntity.weaponAimFlags & 0x40) != 0) {
        // aimed -> neutral: reversed recover
        unsigned char oldDir = (old >> 5 & 1) * 2 + (old >> 7 & 1);
        g_playerEntity.action_state = 8;
        g_playerEntity.weaponAimFlags |= 1;
        g_playerEntity.attackAnim = (unsigned char)(oldDir * 3 + 5);
        if (g_playerEntity.equippedWeaponId > 0x6e) {
            g_playerEntity.attackAnim = (unsigned char)(((old >> 4) & 2) + (old >> 7 & 1) + 5);
            weapon_special_frame_update(1, 1);
        }
        return;
    }
    if (((g_playerEntity.weaponAimFlags ^ old) & 0xa0) == 0) {
        // fire held, no direction change: keep the fire motion going
        Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
        if (g_playerEntity.equippedWeaponId > 0x6e) {
            weapon_special_frame_update(1, 1);
        }
        if ((g_PlayerDpadHeld & 0x40) == 0) {
            g_playerEntity.action_behavior = 0x13;
            g_playerEntity.action_state = 0;
            return;
        }
        if ((g_PlayerDpadHeld & 2) != 0) {
            g_playerEntity.directionAngle += 0x20;
            return;
        }
        if ((g_PlayerDpadHeld & 8) != 0) {
            g_playerEntity.directionAngle -= 0x20;
            return;
        }
    } else {
        // direction changed: quick-fire in the new direction
        // (0x00458b6d: ADD AL,CL - direction term adds)
        g_playerEntity.action_state = 8;
        g_playerEntity.weaponAimFlags &= 0xfe;
        g_playerEntity.attackAnim = (unsigned char)
            ((((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir) * 3 + 5);
        if (g_playerEntity.equippedWeaponId > 0x6e) {
            g_playerEntity.attackAnim = (unsigned char)
                (((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir + 5);
            return;
        }
    }
}

// 0x00458080 - auto-aim fire dispatcher (state table at 0x004c0d28).
// Full state machine: 0 raise -> 1 fire -> 2 back-to-hold (empty click);
// 3/4 re-raise; 5/6 hold-fire loop (continuous fire while held, weapon 6 and
// specials enter at 3 directly from the hold input); 7 empty; 8/9 reversed
// recover after a direction-change quick-fire; 10 loops back to 5.
static void player_behavior_14_autoaim(void)
{
    switch (g_playerEntity.action_state) {
    case 0:
        player_behavior_14_autoaim_raise();
        return;
    case 1:
        player_behavior_14_autoaim_fire();
        return;
    case 2: {
        // fire motion done: back to the hold; click if the slot is empty
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state = 0;
        unsigned char* slot = (unsigned char*)g_ItemSlotsPointer + g_EquippedItemId * 2;
        // The DC's copy (PS1 0x8003fc6c) drops the empty click while the
        // magnum is the ADVANCED-unlocked infinite one.
        if (slot[-1] == 0 && !dc_is_infinite_colt_python(slot[-2])) {
            Play3DSnd(1, 9, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        }
        return;
    }
    case 3: {
        // re-raise after the fire motion: arm the raise pose, then play it
        // (0x0045862f: ADD AL,CL - same direction-term fix as the raise)
        char dir = (char)(g_playerEntity.weaponAimFlags >> 7);
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.action_state = 4;
        g_playerEntity.attackAnim = (unsigned char)
            ((((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir + 2) * 3);
        if (g_playerEntity.equippedWeaponId > 0x6e) {
            g_playerEntity.attackAnim = (unsigned char)
                (((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir + 5);
            weapon_special_frame_update(1, 1);
        }
        g_playerEntity.unk_8c = 3;
        g_playerEntity.move_speed_current = 1;
        player_behavior_14_autoaim_raise2();
        return;
    }
    case 4:
        player_behavior_14_autoaim_raise2();
        return;
    case 5: {
        // arm the fire-again motion (normal: motion 0xe; special: dir+5)
        // (0x004586ea: ADD AL,CL - direction term adds, as everywhere)
        char dir = (char)(g_playerEntity.weaponAimFlags >> 7);
        g_playerEntity.action_state = 6;
        g_playerEntity.unk_bf = 0;
        if (g_playerEntity.equippedWeaponId < 0x6f) {
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.attackAnim = (unsigned char)
                (((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir + 0xe);
        } else {
            g_playerEntity.attackAnim = (unsigned char)
                (((g_playerEntity.weaponAimFlags & 0x20) >> 4) + dir + 5);
        }
        g_playerEntity.unk_8c = 3;
        player_behavior_14_autoaim_holdfire();
        return;
    }
    case 6:
        player_behavior_14_autoaim_holdfire();
        return;
    case 7:
        return;                       // empty in the original
    case 8:
        // arm the reversed recover: frame 0, then play the fire motion backward
        g_playerEntity.action_state = 9;
        if (g_playerEntity.equippedWeaponId < 0x6f) {
            g_playerEntity.animation_frame_id = 0;
        }
        g_playerEntity.unk_bf = 0;
        g_playerEntity.unk_8c = 3;
        Play3DSnd(1, 6, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        player_behavior_14_autoaim_reverse();
        return;
    case 9:
        player_behavior_14_autoaim_reverse();
        return;
    case 10:
        g_playerEntity.action_state = 5;
        return;
    default:
        // Original auto-aim state table 0x004c0d28 entries (Ghidra names):
        //   player_autoaim_fire_start   0x004585c0   player_autoaim_fire_arm    0x00458600
        //   player_autoaim_fire_hold    0x004586c0   player_autoaim_reverse_start 0x00458c50
        //   player_quickfire_start      0x00458cf0   (empty slot) 0x00458c40
        // States 0..10 are handled above; the original has no case beyond 10
        // either (the table is 11 entries), so this branch is unreachable.
        player_state_report_missing("auto-aim fire state (0x004c0d28)");
        return;
    }
}

// ============================================================================
// player_behavior_15_gun_fire @ 0x00458d00 - action_behavior 0x15/0x16
// The manual quick-fire animation (0x15 forward, 0x16 reverse, selected by
// the g_animFrameIdSave scratch the frame dispatcher writes). Anim-only in
// the original: no damage, no ammo. Returns to the hold on loop or on a
// direction press.
// ============================================================================
static void player_behavior_15_gun_fire(void)
{
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.move_speed_current = 1;
        g_playerEntity.action_state = 1;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.isBeingAttackedFlag = 0;
        g_playerEntity.unk_8c = 3;
        if (g_playerEntity.equippedWeaponId > 0x6e && g_animFrameIdSave != 0) {
            g_playerEntity.attackAnim = 5;
            g_playerEntity.animation_frame_id = 0xf;
            g_weaponSpecialFireCountdown = 0xf;
        }
        if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
            PlayEntitySnd(0);
        }
        break;
    case 1:
        break;
    case 2:
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state = 0;
        return;
    default:
        return;
    }

    if (g_playerEntity.equippedWeaponId < 0x6f || g_animFrameIdSave == 0) {
        unsigned char ret = (unsigned char)Joint_move((char)g_animFrameIdSave,
                                                      g_playerEntity.jointMoveData0,
                                                      g_playerEntity.jointMoveData1, 0x400);
        g_playerEntity.action_state = (unsigned char)(g_playerEntity.action_state + ret);
    } else {
        // special weapons run a fixed 0xf-frame countdown instead
        g_playerEntity.animation_frame_id = 0xf;
        Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
        if (g_weaponSpecialFireCountdown-- < 0) {
            g_playerEntity.action_state++;
        }
    }

    if ((g_PlayerDpadHeld & 5) != 0) {
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state = 0;
    }
}

// ============================================================================
// player_behavior_17_holster @ 0x00458e10 - action_behavior 0x17
// Plays the weapon-lower motion in reverse and returns to locomotion on loop.
// ============================================================================
static void player_behavior_17_holster(void)
{
    if ((g_PlayerDpadHeld & 2) != 0) {
        g_playerEntity.directionAngle += 0x50;
    }
    if ((g_PlayerDpadHeld & 8) != 0) {
        g_playerEntity.directionAngle -= 0x50;
    }

    if (g_playerEntity.action_state == 0) {
        g_playerEntity.attackAnim = 5;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.move_speed_current = 1;
        g_playerEntity.action_state = 1;
        if (g_playerEntity.equippedWeaponId > 0x6e) {
            g_playerEntity.animation_frame_id = 0x1c;
        }
        if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
            PlayEntitySnd(0);
        }
    }

    unsigned char ret = (unsigned char)Joint_move(1, g_playerEntity.jointMoveData0,
                                                  g_playerEntity.jointMoveData1, 0x400);
    if (ret != 0) {
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 0;
        g_playerEntity.animFrameId = 0;
        g_playerEntity.move_speed_current = 0;
    }
}

// ============================================================================
// Hold-input fire family (behaviors 0x18 / 0x19 / 0x1A)
// ============================================================================

// FUN_0045a550 - once-per-trigger ammo gate. Sets the 0x80 bit on the equipped
// slot's quantity byte; returns 1 only on the fresh consumption, so callers
// testing the low byte see TRUE exactly once per trigger pull.
static int fire_ammo_volley_gate(void)
{
    unsigned char* slotQty = ((unsigned char*)g_ItemSlotsPointer) + g_EquippedItemId * 2 - 1;
    if ((*slotQty & 0x80) != 0) {
        return 0;
    }
    *slotQty = (unsigned char)(*slotQty | 0x80);
    return 1;
}

// FUN_0042a000 - arm the joint-15 recoil blend (first frame only)
static void fire_reset_joint15_recoil(void)
{
    JointStruct* j = &g_playerEntity.jointsStructs[0xf];
    if (j->velZ == 0) {
        j->velZ = 1;
        j->rotDeltaX = 0;
    }
}

// FUN_0045a580 - consume one round from the largest ammo stack that fits.
// The ammo item for a weapon is itemId weaponId+9 (weapon_ammo_item_id - the
// DC's item 4 is the Beretta M92FS custom, which takes the clip); the cap comes
// from g_ItemMaxQty[ammoItem*4].
static void fire_consume_ammo_stack(void)
{
    unsigned char weaponId = g_playerEntity.equippedWeaponId;
    unsigned char maxQty = g_ItemMaxQty[(unsigned int)weapon_ammo_item_id(weaponId) * 4];
    unsigned char bestSlot = 0;
    unsigned char bestQty = 0;
    unsigned char slots = (unsigned char)((4 - ((g_playerEntity.id & 3) != 1)) * 2);
    for (unsigned char i = 0; i < slots; i++) {
        unsigned char id = ((unsigned char*)g_ItemSlotsPointer)[i * 2];
        unsigned char qty = ((unsigned char*)g_ItemSlotsPointer)[i * 2 + 1];
        if (id == weapon_ammo_item_id(weaponId) && qty > bestQty) {
            bestSlot = i;
            bestQty = qty;
        }
    }
    if (maxQty < bestQty) {
        ((unsigned char*)g_ItemSlotsPointer)[g_EquippedItemId * 2 - 1] = maxQty;
        ((unsigned char*)g_ItemSlotsPointer)[bestSlot * 2 + 1] = bestQty - maxQty;
        return;
    }
    ((unsigned char*)g_ItemSlotsPointer)[g_EquippedItemId * 2 - 1] = bestQty;
    ((unsigned char*)g_ItemSlotsPointer)[bestSlot * 2] = 0;
    rearrange_item_slots();
}

// Per-weapon reload-FX routine. The original is a function-pointer dispatch -
// 0x00458f2f: `call dword ptr [equippedWeaponId*4 + 0x004c0f98]` - and the table
// is indexed by the RAW item id, so its first live slot is index 2:
//
//   [2] 0x00458ff0  beretta        [4] 0x00459090  colt python (dum-dum)
//   [3] 0x00459040  shotgun        [5] 0x00459090  colt python (magnum)
//   [6..9] 0x00459120              (unreachable: 0x00457fba gates b18 on id < 6)
//
// The port took 0x004c0fa0 - two entries in - as the base and switched on
// equippedWeaponId as though it were a 0-based weapon index, so every weapon ran
// the routine belonging to the weapon two slots up: the beretta AND the shotgun
// both got the python's (whose frame-0xc branch spawns the ejected-case
// billboard the shotgun's reload has no animation for) and the python got the
// bazooka/flamethrower stub. Each routine fires its effects on specific frames
// of the reload motion (EMW motion 0xe) while unk_bf == 1.
// The Beretta's routine (0x00458ff0), shared by the two ids that name that
// weapon: the plain item 2, and the DC's item 4 - the Beretta M92FS custom,
// which is the ADVANCED starting handgun and is the same gun in everything but
// its view art. It fires the volley gate at frame 0xa and consumes the round
// from the ammo stack at frame 0x11; that stack is the 9mm clip for both ids
// (weapon_ammo_item_id), because the DC renamed (4 + 9)'s 0x0D to LOCKPICK.
static void fire_beretta_fx(int frame, int* muzzlePos)
{
    (void)muzzlePos;
    if (frame == 0xa && g_playerEntity.unk_bf == 1) {
        if (fire_ammo_volley_gate() != 0) {
            fire_reset_joint15_recoil();
        }
    }
    if (frame == 0x11 && g_playerEntity.unk_bf == 1) {
        fire_consume_ammo_stack();
        Play3DSnd(1, 5, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
    }
}

static void fire_weapon_fx(void)
{
    const int frame = (int)g_playerEntity.animation_frame_id;
    int* muzzlePos = g_playerEntity.jointsStructs[0xe].world.t;   // [0xbe637c]+0x6c8+0x58

    switch (g_playerEntity.equippedWeaponId) {
    case ITEM_BERETTA: {   // 0x00458ff0
        fire_beretta_fx(frame, muzzlePos);
        break;
    }
    case ITEM_SHOTGUN: {   // 0x00459040
        if ((frame == 0xf || frame == 0x19 || frame == 0x23)
            && g_playerEntity.unk_bf == 1) {
            Play3DSnd(1, 9, 5, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
            if (frame == 0xf) {
                fire_consume_ammo_stack();
            }
        }
        break;
    }
    case ITEM_COLT_PYTHON_DUM:
        // In a DC session this id is the Beretta M92FS custom, so it takes the
        // Beretta routine rather than the Python's (whose frame 0xc spawns an
        // ejected-case billboard the Beretta's reload motion has no animation
        // for). The consumption goes through the same helper, which is what
        // makes the custom Beretta eat clips and not lockpicks.
        if (g_bDcMode) {
            fire_beretta_fx(frame, muzzlePos);
            break;
        }
        // fall through - the USA build's DumDum-rounds Colt Python
    case ITEM_COLT_PYTHON_MAG: {   // 0x00459090
        if (frame == 0xc && g_playerEntity.unk_bf == 1) {
            if (fire_ammo_volley_gate() != 0) {
                Effect_CreateBillboard(5, 4, 0, NULL, muzzlePos, 5);
            }
        }
        if (frame == 0x1c && g_playerEntity.unk_bf == 1) {
            fire_consume_ammo_stack();
            Play3DSnd(1, 9, 0xa, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        }
        break;
    }
    default: {  // 0x00459120 (item ids 6..9)
        if (frame == 0x12 && g_playerEntity.unk_bf == 1) {
            fire_consume_ammo_stack();
            Play3DSnd(1, 5, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        }
        break;
    }
    }
}

// ============================================================================
// player_behavior_18_hold_fire @ 0x00458ec0 - action_behavior 0x18
// The hold-input gun fire entered from the raise-hold (b13 hold-input, weapon
// < 6). State 1 plays the fire motion and runs the per-weapon FX routine; on
// loop it returns to the hold (0x13), or for weapons > 6 chains into the
// state-2 recoil blend before handing back to 0x13.
// ============================================================================
static void player_behavior_18_hold_fire(void)
{
    if (g_playerEntity.action_state == 0) {
        g_playerEntity.action_state = 1;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.healthStatusFlags |= 0x80;
        g_message_flags &= 0xffbf;
        g_playerEntity.move_speed_current = 1;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.attackAnim = 0xe;
        g_playerEntity.unk_8c = 3;
        if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
            PlayEntitySnd(0);
        }
    } else if (g_playerEntity.action_state == 1) {
        fire_weapon_fx();

        unsigned char ret = (unsigned char)Joint_move(0, g_playerEntity.jointMoveData0,
                                                      g_playerEntity.jointMoveData1, 0x400);
        if (ret != 0) {
            g_playerEntity.healthStatusFlags &= 0x7f;
            g_playerEntity.action_behavior = 0x13;
            g_playerEntity.action_state = 0;
            g_message_flags |= 0x40;
            if (g_playerEntity.equippedWeaponId > 6) {
                // special weapons blend the recoil before re-raising
                g_playerEntity.action_behavior = 0x18;
                g_playerEntity.action_state = 2;
                g_playerEntity.animation_frame_id = 0;
                g_playerEntity.unk_bf = 0;
                g_playerEntity.healthStatusFlags |= 0x80;
                g_message_flags &= 0xffbf;
                g_playerEntity.unk_8c = 7;
                g_playerEntity.attackAnim = 7;
            }
        }
    } else if (g_playerEntity.action_state == 2) {
        Joint_move(0, g_playerEntity.jointMoveData0,
                   g_playerEntity.jointMoveData1, 0x200);
        if (g_playerEntity.unk_8c != 0) {
            return;
        }
        g_playerEntity.healthStatusFlags &= 0x7f;
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state = 0;
        g_message_flags |= 0x40;
    }
}

// ============================================================================
// player_behavior_19_fire_click @ 0x00459310 - action_behavior 0x19
// The empty-slot click: plays the dry-fire sfx once, counts attackDirection
// down from 0xf, then hands back to the raise-hold (0x13).
// ============================================================================
static void player_behavior_19_fire_click(void)
{
    if (g_playerEntity.action_state == 0) {
        g_playerEntity.action_state = 1;
        g_playerEntity.attackDirection = 0xf;
        Play3DSnd(1, 9, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
        if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
            PlayEntitySnd(0);
        }
    }

    short prev = g_playerEntity.attackDirection;
    g_playerEntity.attackDirection--;
    if (prev == 0) {
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state = 0;
    }
}

// ============================================================================
// player_behavior_1a_lockon_fire @ 0x00459150 - action_behavior 0x1A
// The target-lock-on fire (raw pad bit 2 held with a lock): alternates the
// weapon-joint fire motion (state 1, jmd buffers) with the body recover
// blend (state 2, animHeader/animBase), steering toward the locked target.
// When aligned, control returns to the raise-hold (0x13). Holding the fire
// button re-acquires the target every frame outside state 0.
// ============================================================================
static void player_behavior_1a_lockon_fire(void)
{
    if (g_playerEntity.action_state != 0 && (g_PlayerPadHeld & 4) != 0) {
        player_find_aim_target();
    }

    char dir = (char)(g_playerEntity.weaponAimFlags >> 7);          // 0/1 (ADD convention)
    unsigned char up = (unsigned char)((g_playerEntity.weaponAimFlags & 0x20) >> 4);

    if (g_playerEntity.action_state == 0) {
        g_playerEntity.unk_8c = 0;
        g_playerEntity.move_speed_current = 1;
        g_playerEntity.action_state = 1;
        goto state1_body;
    }
    if (g_playerEntity.action_state == 1) {
        goto state1_body;
    }
    if (g_playerEntity.action_state == 2) {
        if (g_playerEntity.unk_8c == 0) {
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.attackAnim =
                (unsigned char)((up + dir) * 3 + 7);
            g_playerEntity.unk_bf = 0;
            g_playerEntity.action_state = 1;
            g_playerEntity.unk_8c = 7;
            return;
        }
        Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x200);
        goto turn_tail;
    }
    goto turn_tail;

state1_body:
    if (g_playerEntity.unk_8c == 0) {
        g_playerEntity.action_state = 2;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_8c = 7;
        g_playerEntity.attackAnim =
            (unsigned char)(up + dir + g_playerEntity.equippedWeaponId * 3 + 2);
        if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
            PlayEntitySnd(0);
        }
        return;
    }
    Joint_move(0, g_playerEntity.jointMoveData0,
               g_playerEntity.jointMoveData1, 0x200);

turn_tail:
    g_animFrameIdSave = (unsigned int)(g_playerEntity.id & 1) * 0x20 + 0xf0;
    if ((short)turn_toward_target((VECTOR*)(g_playerEntity.unk_b8 + 0x34), 0x200) == 0) {
        g_animFrameIdSave = ((g_playerEntity.id & 1) + 6) * 0x20;
    }
    entity_rotate_toward_target((VECTOR*)(g_playerEntity.unk_b8 + 0x34),
                                (unsigned short)g_animFrameIdSave);
    if ((short)turn_toward_target((VECTOR*)(g_playerEntity.unk_b8 + 0x34), 0x20) == 0) {
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state = 0;
    }
}

// ============================================================================
// player_behavior_12_knife_aim @ 0x00459370 - knife action_behavior 0x12
// Knife aim pose: motion 5 with the reticle follow, turns of 0x20, and the
// weapon joint update inline. The hold state takes over when the loop ends.
// ============================================================================
static void player_behavior_12_knife_aim(void)
{
    if (g_playerEntity.action_state == 0) {
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.move_speed_current = 1;
        g_playerEntity.action_state = 1;
        g_playerEntity.attackDirection = 0;
        g_playerEntity.isBeingAttackedFlag = 0;
        g_playerEntity.attackAnim = 5;
        g_playerEntity.unk_8c = 3;
        if (g_aimReticleEnabled != 0) {
            g_playerEntity.weaponAimState = (unsigned short)player_reticle_enemy();
        }
    }

    if (g_aimReticleEnabled != 0) {
        g_playerEntity.weaponAimState |= 2;
        if ((g_button_pressed_id & 4) != 0) {
            g_playerEntity.weaponAimState = 0;
        }
        if (g_playerEntity.weaponAimState == 3) {
            g_animFrameIdSave = (unsigned int)(g_playerEntity.id & 1) * 0x20 + 0xf0;
            if ((short)turn_toward_target((VECTOR*)(g_playerEntity.unk_b8 + 0x34), 0x200) == 0) {
                g_animFrameIdSave = ((g_playerEntity.id & 1) + 6) * 0x20;
            }
            entity_rotate_toward_target((VECTOR*)(g_playerEntity.unk_b8 + 0x34),
                                        (unsigned short)g_animFrameIdSave);
        }
    }

    unsigned char ret = (unsigned char)Joint_move(0, g_playerEntity.jointMoveData0,
                                                  g_playerEntity.jointMoveData1, 0x400);
    if (ret != 0) {
        g_playerEntity.unk_8c = 0;
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state = 0;
    }
    EntityUpdateWeaponJoint(0);

    if ((g_PlayerDpadHeld & 2) != 0) {
        g_playerEntity.directionAngle += 0x20;
        return;
    }
    if ((g_PlayerDpadHeld & 8) != 0) {
        g_playerEntity.directionAngle -= 0x20;
    }
}

// ============================================================================
// player_behavior_13_knife_hold @ 0x004594c0 - knife action_behavior 0x13
// Knife aim-hold: writes the aim direction straight into the flags byte
// (the knife's hits read it there), plays the hold motions 0x0c/0x0b/0x0e
// (down/neutral/up, +9 variant on the second pose), decrements the swing
// cooldown (attackDirection), and swings (0x14) on the fire button when the
// cooldown is spent. Holsters (0x15) on aim release.
// ============================================================================
static void player_behavior_13_knife_hold(void)
{
    unsigned char old = g_playerEntity.flags;
    g_animFrameIdSave = (unsigned int)old;
    g_playerEntity.flags = (g_playerEntity.flags & 0x1f) | 0x40;
    if ((g_PlayerDpadHeld & 0x10) != 0) {
        g_playerEntity.flags = (g_playerEntity.flags & 0x1f) | 0x80;
    }
    if ((g_PlayerDpadHeld & 0x20) != 0) {
        g_playerEntity.flags = (g_playerEntity.flags & 0x1f) | 0x20;
    }
    g_playerEntity.weaponAimFlags &= 0xfd;   // clear the "already hit" flag

    if (g_playerEntity.action_state == 0) {
        if (g_playerEntity.unk_8c == 0) {
            g_playerEntity.action_state = 1;
            g_playerEntity.move_speed_current = 1;
            g_playerEntity.attackAnim = (unsigned char)
                (((g_playerEntity.flags & 0xbf) >> 6) + ((g_playerEntity.flags & 0x20) >> 3) + 10);
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.unk_bf = 0;
            g_playerEntity.unk_8c = 0xf;
            if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
                PlayEntitySnd(0);
            }
        }
        Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x100);
    }
    if (g_playerEntity.unk_8c == 0) {
        // 0x004595a6: the word at joint[1].scale_flag (entity+0x134) is
        // masked to its low byte - the blend-progress handshake.
        *(unsigned short*)&g_playerEntity.jointsStructs[1].scale_flag &= 0xff;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.unk_8c = 0xf;
        g_playerEntity.move_speed_current = 1;
        g_playerEntity.attackAnim = (unsigned char)
            (((g_playerEntity.flags & 0xbf) >> 6) + ((g_playerEntity.flags & 0x20) >> 3) + 9);
    }
    Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x100);
    EntityUpdateWeaponJoint(1);

    if (g_playerEntity.attackDirection != 0) {
        g_playerEntity.attackDirection--;
    }

    if ((g_PlayerDpadHeld & 0x40) != 0 && g_playerEntity.attackDirection == 0) {
        g_playerEntity.action_behavior = 0x14;
        g_playerEntity.action_state = 0;
        g_playerEntity.attackDirection = 10;
        return;
    }
    if ((g_PlayerDpadHeld & 0x100) == 0) {
        g_playerEntity.action_behavior = 0x15;   // knife holster
        g_playerEntity.action_state = 0;
        return;
    }
    if ((g_PlayerPadHeld & 4) != 0 && player_find_aim_target() != 0) {
        g_playerEntity.action_behavior = 0x16;   // knife auto-aim turn
        g_playerEntity.action_state = 0;
        return;
    }
    if ((g_PlayerDpadHeld & 2) != 0) {
        g_playerEntity.directionAngle += (short)((g_playerEntity.id & 1) * -0x10 + 0x48);
        return;
    }
    if ((g_PlayerDpadHeld & 8) != 0) {
        g_playerEntity.directionAngle += (short)((g_playerEntity.id & 1) * 0x10 - 0x48);
    }
}

// ============================================================================
// player_behavior_14_knife_swing @ 0x004596c0 - knife action_behavior 0x14
// The knife swing. The swing motion is dir*3+6 (6 neutral, 7 down, 8 up);
// each (character, motion) pair has a fire-frame window in the 12-byte table
// at ESP+4 ({6,7,4,2,4,4,3,8,4,2,4,4}); inside the window apply_weapon_damage
// runs once per swing (weaponAimFlags bit 1). Returns to the hold on loop.
// ============================================================================
static void player_behavior_14_knife_swing(void)
{
    static const unsigned char kFireWindow[12] = { 6, 7, 4, 2, 4, 4, 3, 8, 4, 2, 4, 4 };

    if (g_playerEntity.action_state == 0) {
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.action_state = 1;
        g_playerEntity.unk_bf = 0;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.move_speed_current = 1;
        g_playerEntity.attackAnim = (unsigned char)
            ((((g_playerEntity.flags & 0x20) >> 4) - ((char)g_playerEntity.flags >> 7)) + 6);
    }

    // Swing whistle at frame 7
    if (g_playerEntity.animation_frame_id == 7 && (g_playerEntity.unk_bf & 1) != 0) {
        Play3DSnd(1, 0, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
    }

    // Flash + sound effects, only above the covers-table height
    if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0
        && 1000 < g_playerEntity.scaMatrixData.localMatrix.t[1]
                  - *(int*)((char*)g_omodel_table[0] + 0x38)) {
        if (g_playerEntity.attackAnim == 7) {
            if (g_playerEntity.animation_frame_id < 3) {
                g_playerPosScratch.y = *(int*)((char*)g_deadMoveValue + 0x18);
                g_playerPosScratch.z = *(int*)((char*)g_deadMoveValue + 0x1c);
                g_playerPosScratch.pad = *(int*)((char*)g_deadMoveValue + 0x20);
                g_playerPosScratch.x = 0x96;
                Effect_CreateBillboard(0x17, 8, 0, &g_playerEntity.jointsStructs[0xe].world,
                                       &g_playerPosScratch, 0);
            }
            if (g_playerEntity.animation_frame_id == 0 && g_playerEntity.unk_bf == 0) {
                PlayEntitySnd(1);
            }
        } else {
            if ((g_playerEntity.animation_frame_id & 1) == 0) {
                g_playerPosScratch.y = *(int*)((char*)g_deadMoveValue + 0x18);
                g_playerPosScratch.z = *(int*)((char*)g_deadMoveValue + 0x1c);
                g_playerPosScratch.pad = *(int*)((char*)g_deadMoveValue + 0x20);
                g_playerPosScratch.x = 0x96;
                Effect_CreateBillboard(0x17, 8, 0, &g_playerEntity.jointsStructs[0xe].world,
                                       &g_playerPosScratch, 0);
            }
            if (g_playerEntity.animation_frame_id == 0 && g_playerEntity.unk_bf == 0) {
                Play3DSnd(1, 2, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
            }
        }
    }

    g_playerEntity.isBeingAttackedFlag = 0;

    // Fire window: table entry ((id&1)*3 + attackAnim)*2 - 12
    int e = ((g_playerEntity.id & 1) * 3 + (int)g_playerEntity.attackAnim) * 2 - 12;
    if ((unsigned char)(g_playerEntity.animation_frame_id - kFireWindow[e]) < kFireWindow[e + 1]
        && (g_playerEntity.weaponAimFlags & 2) == 0) {
        if (apply_weapon_damage(1) != 0
            && (g_playerEntity.attackAnim != 6 || g_EnemiesList[0].id == 8
                || g_EnemiesList[0].id == 0xd || g_EnemiesList[1].id == 0x13)) {
            g_playerEntity.weaponAimFlags |= 2;
        }
    }

    unsigned char ret = (unsigned char)Joint_move(0, g_playerEntity.jointMoveData0,
                                                  g_playerEntity.jointMoveData1, 0x400);
    if (ret != 0) {
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state = 0;
    }
    EntityUpdateWeaponJoint(0);
}

// ============================================================================
// player_behavior_15_knife_holster @ 0x00459960 - knife action_behavior 0x15
// Knife holster: reverse motion 5 back to locomotion, weapon joint inline.
// ============================================================================
static void player_behavior_15_knife_holster(void)
{
    if ((g_PlayerDpadHeld & 2) != 0) {
        g_playerEntity.directionAngle += 0x50;
    }
    if ((g_PlayerDpadHeld & 8) != 0) {
        g_playerEntity.directionAngle -= 0x50;
    }

    if (g_playerEntity.action_state == 0) {
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.move_speed_current = 1;
        g_playerEntity.action_state = 1;
        g_playerEntity.attackAnim = 5;
        g_playerEntity.unk_8c = 3;
        g_playerEntity.unk_bf = 0;
    }

    unsigned char ret = (unsigned char)Joint_move(1, g_playerEntity.jointMoveData0,
                                                  g_playerEntity.jointMoveData1, 0x400);
    if (ret != 0) {
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state = 0;
        g_playerEntity.animFrameId = 0;
    }
    EntityUpdateWeaponJoint(0);
}

// ============================================================================
// player_behavior_16_knife_turn @ 0x004599f0 - knife action_behavior 0x16
// Knife auto-aim turn, the knife counterpart of gun behavior 0x14. While the
// aim button is held the locked target is re-acquired every frame; the hold
// motions +9/+10 blend toward it (state 1/2 ping-pong on unk_8c), then the
// shared tail steers the body with the fast/slow turn pair until aligned and
// hands control back to the knife hold (0x13).
// ============================================================================
static void player_behavior_16_knife_turn(void)
{
    if (g_playerEntity.action_state != 0 && (g_PlayerPadHeld & 4) != 0) {
        player_find_aim_target();
    }

    if (g_playerEntity.action_state == 0) {
        // 0x00459a11: enter the turn with hold motion +9
        g_playerEntity.action_state       = 1;
        g_playerEntity.animation_frame_id = 0;      // unk_be
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.unk_8c             = 0xf;
        g_playerEntity.move_speed_current = 1;      // unk_c2
        g_playerEntity.attackAnim         = (unsigned char)
            (((g_playerEntity.flags & 0xbf) >> 6) + ((g_playerEntity.flags & 0x20) >> 3) + 9);
    }

    if (g_playerEntity.action_state <= 1) {
        // LAB_00459a69 - shared by entry and state 1
        if (g_playerEntity.unk_8c == 0) {
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.unk_bf             = 0;
            g_playerEntity.action_state       = 2;
            g_playerEntity.unk_8c             = 0xf;
            g_playerEntity.attackAnim         = (unsigned char)
                (((g_playerEntity.flags & 0xbf) >> 6) + ((g_playerEntity.flags & 0x20) >> 3) + 10);
            if ((g_main_state_flags2 & MSF2_EFFECT_ZONE) != 0) {
                PlayEntitySnd(0);
            }
        }
    } else if (g_playerEntity.action_state == 2) {
        // 0x00459ac0 - loop back to motion +9 when the blend timer expires
        if (g_playerEntity.unk_8c == 0) {
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.unk_bf             = 0;
            g_playerEntity.action_state       = 1;
            g_playerEntity.unk_8c             = 0xf;
            g_playerEntity.attackAnim         = (unsigned char)
                (((g_playerEntity.flags & 0xbf) >> 6) + ((g_playerEntity.flags & 0x20) >> 3) + 9);
        }
    }
    Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x100);

    // LAB_00459afa: steer toward the target until aligned
    g_animFrameIdSave = (unsigned int)(g_playerEntity.id & 1) * 0x20 + 0xf0;
    if ((short)turn_toward_target((VECTOR*)(g_playerEntity.unk_b8 + 0x34), 0x200) == 0) {
        g_animFrameIdSave = ((g_playerEntity.id & 1) + 6) * 0x20;
    }
    entity_rotate_toward_target((VECTOR*)(g_playerEntity.unk_b8 + 0x34),
                                (unsigned short)g_animFrameIdSave);
    if ((short)turn_toward_target((VECTOR*)(g_playerEntity.unk_b8 + 0x34), 0x20) == 0) {
        g_playerEntity.action_behavior = 0x13;
        g_playerEntity.action_state    = 0;
    }
}

// ============================================================================
// player_ctrl_frame2 @ 0x00495330 - animFrameId 2
// The bare action_behavior dispatch (no input read - the frame-0 switch
// without player_input_to_behavior). Reachable when a behavior carries over
// while the machine is not on frame 0.
// ============================================================================
static void player_ctrl_frame2(void)
{
    switch (g_playerEntity.action_behavior) {
    case 0:
        player_behavior_00_idle();
        return;
    case 1:
        player_ctrl_behavior_walk();
        return;
    case 2:
        g_playerEntity.directionAngle = (g_playerEntity.directionAngle + 0x28) & 0xfff;
        player_ctrl_behavior_walk();
        return;
    case 3:
        g_playerEntity.directionAngle = (g_playerEntity.directionAngle - 0x28) & 0xfff;
        player_ctrl_behavior_walk();
        return;
    case 4:
        g_playerEntity.directionAngle = (g_playerEntity.directionAngle + 0x60) & 0xfff;
        player_ctrl_behavior_back();
        return;
    case 5:
        g_playerEntity.directionAngle = (g_playerEntity.directionAngle - 0x60) & 0xfff;
        player_ctrl_behavior_back();
        return;
    case 6:
        g_playerEntity.directionAngle = (g_playerEntity.directionAngle + 0x28) & 0xfff;
        player_ctrl_behavior_run();
        return;
    case 7:
        g_playerEntity.directionAngle = (g_playerEntity.directionAngle - 0x28) & 0xfff;
        player_ctrl_behavior_run();
        return;
    case 8:
        player_ctrl_behavior_run();
        return;
    case 9:
        return;                       // 0x00495df0 - empty in the original
    case 10:          // door transition
    case 0x11:
        player_door_open_sequence();
        return;
    case 0x0b:                        // ladder climb
        player_behavior_0b_ladder();
        return;
    case 0x0c:                        // object interaction
        player_behavior_0c_interact();
        return;
    case 0x0d:
        player_behavior_0d_run();
        return;
    case 0x0e:
    case 0x0f:
        // 0x00495405: turn by 0x30 and pre-rotate the run velocity
        g_playerEntity.directionAngle =
            (g_playerEntity.directionAngle + (g_playerEntity.action_behavior == 0x0e ? 0x30 : -0x30)) & 0xfff;
        g_svecScratch.x = g_playerEntity.move_speed_current;
        g_svecScratch.y = 0;
        g_svecScratch.z = 0;
        RotMatrix((SVECTOR*)&g_playerEntity.position.pad, &g_matrixScratch);
        ApplyMatrixSV(&g_matrixScratch, &g_svecScratch, &g_playerEntity.speed);
        player_behavior_0d_run();
        return;
    case 0x10:                        // push / climb-over object
        player_behavior_10_push();
        return;
    default:
        // 0x00495330: the behavior table behind this switch only spans 0x00-0x11;
        // every other index falls through to a plain return in the original.
        //
        // This default is reached once per aim press and is NOT an unimplemented
        // animation: player_ctrl_frame0 (0x00495320) is CALL 0x004956a0 (input)
        // followed by JMP 0x00495330. When the input step raises a weapon it sets
        // animFrameId=3/4 AND action_behavior=0x12 before this table runs, so the
        // first pass lands here with a weapon behavior that has no frame-2 entry.
        // The next tick re-dispatches on animFrameId and reaches the real gun/knife
        // handler under frames 3/4, so returning silently is exactly what the
        // original does - do not "fix" this by reporting.
        return;
    }
}

// ============================================================================
// player_ctrl_frame3 @ 0x00495530 - animFrameId 3 (gun aim/fire family)
// action_behavior 0x12-0x1A dispatch (table at 0x004955b8). The shared tail
// updates the weapon joint every frame and runs the auto-aim pitch for
// weapon 6. 0x18 (magnum FX), 0x19 (GL) and 0x1A (special lock-on) are not
// yet transcribed and report rather than sit NULL.
// ============================================================================
static void player_ctrl_frame3(void)
{
    switch (g_playerEntity.action_behavior) {
    case 0x12:
        player_behavior_12_gun_aim();
        break;
    case 0x13:
        player_behavior_13_gun_raise();
        player_behavior_13_gun_hold_input();
        break;
    case 0x14:
        player_behavior_14_autoaim();
        break;
    case 0x15:
        g_animFrameIdSave = 0;
        player_behavior_15_gun_fire();
        break;
    case 0x16:
        g_animFrameIdSave = 1;
        player_behavior_15_gun_fire();
        break;
    case 0x17:
        player_behavior_17_holster();
        break;
    case 0x18:
        player_behavior_18_hold_fire();
        break;
    case 0x19:
        player_behavior_19_fire_click();
        break;
    case 0x1a:
        player_behavior_1a_lockon_fire();
        break;
    default:
        player_state_report_missing("action_behavior under animFrameId 3");
        break;
    }

    // 0x0049559c: shared tail
    EntityUpdateWeaponJoint(0);
    if (g_playerEntity.equippedWeaponId == 6) {
        auto_aim_pitch_update();
    }
}

// ============================================================================
// player_ctrl_frame4 @ 0x004955e0 - animFrameId 4 (knife aim/fire family)
// Pure jump through the table at 0x004d4570; the knife behaviours occupy
// 0x12-0x16, and entries 0x00-0x11 alias locomotion handlers that are only
// reachable from states the machine cannot be in here.
// ============================================================================
static void player_ctrl_frame4(void)
{
    switch (g_playerEntity.action_behavior) {
    case 0x12:
        player_behavior_12_knife_aim();
        return;
    case 0x13:
        player_behavior_13_knife_hold();
        return;
    case 0x14:
        player_behavior_14_knife_swing();
        return;
    case 0x15:
        player_behavior_15_knife_holster();
        return;
    case 0x16:
        player_behavior_16_knife_turn();
        return;
    default:
        player_state_report_missing("action_behavior under animFrameId 4");
        return;
    }
}

// ============================================================================
// Player state 8 (0x0044cf30) — the SCD-driven animation state.
//
// This is how a cutscene animates the player. Event opcodes 0x83/0x84/0x85 write
// state 8 plus an action_behavior straight into the entity (see
// scd_event_state1_anim), and this state runs the matching handler from the table
// at 0x004beca0 until the animation completes and the script moves on. With state
// 8 unimplemented the player froze in whatever pose the script had just set and
// the event VM waited on an animation that never advanced.
//
// All ten handlers are transcribed. Behaviour 4 was the last one left logging
// its address, and its absence stalled the room 1151 ceiling-trap cutscene: the
// script parks waiting on the flag the backward walk raises on arrival.
// ============================================================================

extern void Flg_on(int baseAddr, unsigned int bitIndex);                          // 0x00473ef0
extern void entity_rotate_toward_target(VECTOR* pos, unsigned short angleStep);    // 0x004899b0
extern int  turn_toward_target(VECTOR* pos, short angleStep);                       // 0x00489b?0
extern void entity_apply_walk_speed(short speed);                                   // 0x0047a4f0

// Shorthand for the recurring Joint_move first argument. The original computes it
// with a CONCAT31/AND 0xffffff01 dance that Ghidra cannot fold; all it amounts to
// is bit 0 of unk_e0 (the SCD "mirror this animation" flag).
static inline int player_joint_mirror(void)
{
    return (int)(g_playerEntity.unk_e0 & 1);
}

// 0x0044cf80 — behavior 0: plain animation playback. action_state 2-5 pick which
// of the four joint-data pairs to finish on, reload the equipped weapon's
// animation and drop into the terminal state 6.
static void player_scd_behavior_00(void)
{
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.action_state       = 1;
        g_playerEntity.unk_8c             = 3;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.attackAnim         = 0;
        // fall through
    case 1:
        Joint_move(player_joint_mirror(), g_playerEntity.animHeader,
                   g_playerEntity.animBase, 0x400);
        return;
    case 2:
        LoadEquippedWeaponAnimation(g_playerEntity.equippedWeaponId, 0xe,
                                    (unsigned int)(unsigned int*)g_animationBuffer,
                                    (unsigned int)(unsigned int*)g_animObjectBuffer);
        g_playerEntity.action_state = 6;
        Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x400);
        return;
    case 3:
        LoadEquippedWeaponAnimation(g_playerEntity.equippedWeaponId, 0xe,
                                    (unsigned int)(unsigned int*)g_animationBuffer,
                                    (unsigned int)(unsigned int*)g_animObjectBuffer);
        g_playerEntity.action_state = 6;
        Joint_move(0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
        return;
    case 4:
        LoadEquippedWeaponAnimation(g_playerEntity.equippedWeaponId, 0xe,
                                    (unsigned int)(unsigned int*)g_animationBuffer,
                                    (unsigned int)(unsigned int*)g_animObjectBuffer);
        g_playerEntity.action_state = 6;
        Joint_move(0, g_playerEntity.jointMoveData2, g_playerEntity.jointMoveData3, 0x400);
        return;
    case 5:
        LoadEquippedWeaponAnimation(g_playerEntity.equippedWeaponId, 0xe,
                                    (unsigned int)(unsigned int*)g_animationBuffer,
                                    (unsigned int)(unsigned int*)g_animObjectBuffer);
        g_playerEntity.action_state = 6;
        Joint_move(0, g_playerEntity.emdScratchPtr1, g_playerEntity.emdScratchPtr2, 0x400);
        return;
    default:
        return;
    }
}

// 0x0044d100 — behavior 1: remapped animation playback. attackAnim carries the
// script's animation index; below 0x10 it goes through g_ScdAnimRemap, 0x3e and up
// selects the fourth joint-data pair. States 1-4 each advance a different pair and
// converge on 5, which raises the script's completion flag.
static void player_scd_behavior_01(void)
{
    short entryDir = (short)g_playerEntity.attackDirection;
    char done;

    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.unk_8c             = 7;
        g_playerEntity.move_speed_current = 0;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        if ((g_playerEntity.unk_e0 & 0x20) != 0) {
            g_playerEntity.unk_8c = 0;
        }
        if (g_playerEntity.attackAnim < 0x3e) {
            if (g_playerEntity.attackAnim < 0x10) {
                unsigned int idx = g_playerEntity.attackAnim;
                g_playerEntity.attackAnim   = g_ScdAnimRemap[idx * 2 + 1];
                g_playerEntity.action_state = g_ScdAnimRemap[idx * 2] + 1;
            } else {
                g_playerEntity.action_state = 3;
            }
        } else {
            g_playerEntity.action_state = 4;
            g_playerEntity.attackAnim   = g_playerEntity.attackAnim - 0x3e;
        }
        g_playerEntity.attackDirection = 1;
        return;

    case 1:
        // unk_e0 bit 7 makes the animation hold for one extra frame per step
        if (((g_playerEntity.unk_e0 & 0x80) != 0) &&
            (g_playerEntity.attackDirection--, entryDir == 0)) {
            g_playerEntity.attackDirection = 1;
            return;
        }
        done = Joint_move(player_joint_mirror(), g_playerEntity.animHeader,
                          g_playerEntity.animBase, 0x200);
        if (done != 0) {
            g_playerEntity.action_state = 5;
            g_playerEntity.directionAngle += (short)g_playerEntity.unk_de;
        }
        return;

    case 2:
        if (((g_playerEntity.unk_e0 & 0x80) != 0) &&
            (g_playerEntity.attackDirection--, entryDir == 0)) {
            g_playerEntity.attackDirection = 1;
            return;
        }
        done = Joint_move(player_joint_mirror(), g_playerEntity.jointMoveData0,
                          g_playerEntity.jointMoveData1, 0x200);
        if (done != 0) {
            g_playerEntity.action_state = 5;
            g_playerEntity.attackAnim   = g_playerEntity.attackAnim + 5;
            g_playerEntity.directionAngle += (short)g_playerEntity.unk_de;
        }
        return;

    case 3:
        if (((g_playerEntity.unk_e0 & 0x80) != 0) &&
            (g_playerEntity.attackDirection--, entryDir == 0)) {
            g_playerEntity.attackDirection = 1;
            return;
        }
        done = Joint_move(player_joint_mirror(), g_playerEntity.jointMoveData2,
                          g_playerEntity.jointMoveData3, 0x200);
        if (done != 0) {
            g_playerEntity.action_state = 5;
            g_playerEntity.directionAngle += (short)g_playerEntity.unk_de;
        }
        return;

    case 4:
        if (((g_playerEntity.unk_e0 & 0x80) != 0) &&
            (g_playerEntity.attackDirection--, entryDir == 0)) {
            g_playerEntity.attackDirection = 1;
            return;
        }
        done = Joint_move(player_joint_mirror(), g_playerEntity.emdScratchPtr1,
                          g_playerEntity.emdScratchPtr2, 0x200);
        if (done != 0) {
            g_playerEntity.action_state = 5;
            g_playerEntity.attackAnim   = g_playerEntity.attackAnim + 0x3e;
            g_playerEntity.directionAngle += (short)g_playerEntity.unk_de;
        }
        return;

    case 5:
        // Raise the completion flag the script is waiting on, then loop back to
        // state 0 when unk_e0 bit 4 asks for a repeat.
        Flg_on((int)g_SysFlags, g_playerEntity.scd_anim_param);
        g_playerEntity.unk_de = 0;
        if ((g_playerEntity.unk_e0 & 0x10) != 0) {
            g_playerEntity.action_state = 0;
        }
        return;

    default:
        return;
    }
}

// 0x0044dac0 — behavior 5: walk to the scripted destination in unk_c6/unk_c8.
// Faces 180 degrees away while calling entity_rotate_toward_target (the helper
// steers toward a point, so the angle is flipped either side of the call to make
// it walk backwards-facing), then stops once within 100 units.
static void player_scd_behavior_05(void)
{
    if (g_playerEntity.action_state == 0) {
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.action_state       = 1;
        g_playerEntity.unk_8c             = 3;
        g_playerEntity.attackAnim         = 2;
        g_playerEntity.move_speed_current = 0x1d;
    }
    else if (g_playerEntity.action_state != 1) {
        return;
    }

    // Footstep sound on the two contact frames
    if (((g_playerEntity.animation_frame_id == 7) ||
         (g_playerEntity.animation_frame_id == 0x1b)) &&
        (g_playerEntity.unk_bf == 2)) {
        // PlayEntitySnd takes ONE parameter (0x0047fbf0). The call sites push a
        // second dword (0 or -4) that the callee never reads - a dead push, the
        // same pattern as rotate_entity's fourth argument.
        PlayEntitySnd(0);
    }

    g_playerPosScratch.x = (int)g_playerEntity.unk_c6;
    g_playerPosScratch.z = (int)g_playerEntity.unk_c8;
    g_playerPosScratch.y = 0;

    g_playerEntity.directionAngle = (short)((g_playerEntity.directionAngle + 0x800) & 0xfff);
    entity_rotate_toward_target(&g_playerPosScratch, g_playerEntity.unk_de);
    g_playerEntity.directionAngle = (short)(g_playerEntity.directionAngle - 0x800);

    Joint_move(player_joint_mirror(), g_playerEntity.animHeader,
               g_playerEntity.animBase, 0x400);
    Add_speedXZ(0x800);

    int dz = g_playerEntity.scaMatrixData.localMatrix.t[2] - (int)g_playerEntity.unk_c8;
    int dx = g_playerEntity.scaMatrixData.localMatrix.t[0] - (int)g_playerEntity.unk_c6;
    if (SquareRoot0(dz * dz + dx * dx) < 100) {
        Flg_on((int)g_SysFlags, g_playerEntity.scd_anim_param);
        if ((g_playerEntity.healthStatusFlags & 0x80) == 0) {
            // 0x0044dc35: MOV dword ptr [EAX+0x84],1 - back to state 1 with
            // animFrameId, action_behavior and action_state all cleared
            g_playerEntity.animationId     = 1;
            g_playerEntity.animFrameId     = 0;
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state    = 0;
        }
    }
}

// 0x0044dd60 — behavior 7: play the second joint-data pair once, applying the
// per-frame turn from unk_de every frame including after completion.
static void player_scd_behavior_07(void)
{
    unsigned char st = g_playerEntity.action_state;
    if (st == 0) {
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.attackAnim         = g_playerEntity.attackAnim - 5;
        g_playerEntity.action_state       = 1;
        g_playerEntity.unk_8c             = 3;
        st = 1;
    }
    if (st == 1) {
        g_playerEntity.action_state += (unsigned char)Joint_move(
            player_joint_mirror(), g_playerEntity.jointMoveData0,
            g_playerEntity.jointMoveData1, 0x400);
    }
    else if (st == 2) {
        Flg_on((int)g_SysFlags, g_playerEntity.scd_anim_param);
    }
    g_playerEntity.directionAngle += (short)g_playerEntity.unk_de;
}

// 0x0044de10 — behavior 8: same as 7 but never mirrored and with no turn.
static void player_scd_behavior_08(void)
{
    unsigned char st = g_playerEntity.action_state;
    if (st == 0) {
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.attackAnim         = g_playerEntity.attackAnim - 5;
        g_playerEntity.action_state       = 1;
        g_playerEntity.unk_8c             = 3;
    }
    else if (st != 1) {
        if (st == 2) {
            Flg_on((int)g_SysFlags, g_playerEntity.scd_anim_param);
        }
        return;
    }
    g_playerEntity.action_state += (unsigned char)Joint_move(
        0, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
}

// 0x0044deb0 — behavior 9: as 8 but always mirrored, and on completion it hands
// the player back to state 1.
static void player_scd_behavior_09(void)
{
    unsigned char st = g_playerEntity.action_state;
    if (st == 0) {
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.attackAnim         = g_playerEntity.attackAnim - 5;
        g_playerEntity.action_state       = 1;
        g_playerEntity.unk_8c             = 3;
    }
    else if (st != 1) {
        if (st != 2) {
            return;
        }
        g_playerEntity.animationId     = 1;
        g_playerEntity.animFrameId     = 0;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state    = 0;
        Flg_on((int)g_SysFlags, g_playerEntity.scd_anim_param);
        return;
    }
    g_playerEntity.action_state += (unsigned char)Joint_move(
        1, g_playerEntity.jointMoveData0, g_playerEntity.jointMoveData1, 0x400);
}

static void player_scd_report(const char* addr)
{
    static const char* lastReported = NULL;
    if (addr != lastReported) {
        lastReported = addr;
        dbg_printf("[player] unimplemented SCD behavior %s (attackAnim=%u frame=%u action=%u)\n",
               addr, (unsigned int)g_playerEntity.attackAnim,
               (unsigned int)g_playerEntity.animation_frame_id,
               (unsigned int)g_playerEntity.action_state);
    }
}

// 0x0044d380 - behaviour 2: turn toward the scripted target, then walk to it.
// The player twin of npc_scd_02: state 1 turns on the spot until aligned within
// 0x16a, state 3 walks (anim 2, footsteps on frames 8 and 0x16) and finishes
// within 150 units.
//
// Note the completion ordering: the original evaluates
// `dist < 0x96 && (Flg_on(...), (healthStatusFlags & 0x80) == 0)`, so Flg_on fires
// on every frame the player is within range, while the return to state 1 happens
// only when the health flag is clear. Reproduced with the same sequencing.
static void player_scd_behavior_02(void)
{
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.attackAnim         = 2;
        g_playerEntity.action_state       = 1;
        g_playerEntity.unk_8c             = 3;
        // fall through
    case 1:
        g_playerPosScratch.x = (int)g_playerEntity.unk_c6;
        g_playerPosScratch.z = (int)g_playerEntity.unk_c8;
        g_playerPosScratch.y = 0;
        g_playerEntity.directionAngle += (short)turn_toward_target(
            &g_playerPosScratch, (short)g_playerEntity.unk_de);
        Joint_move(player_joint_mirror(), g_playerEntity.jointMoveData0,
                   g_playerEntity.jointMoveData1, 0x400);
        if ((short)turn_toward_target(&g_playerPosScratch, 0x16a) == 0) {
            g_playerEntity.action_state = 2;
        }
        break;

    case 2:
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.attackAnim         = 2;
        g_playerEntity.action_state       = 3;
        g_playerEntity.unk_8c             = 3;
        // fall through
    case 3:
        // PlayEntitySnd takes ONE parameter (0x0047fbf0). The call sites push a
        // second dword (0 or -4) that the callee never reads - a dead push, the
        // same pattern as rotate_entity's fourth argument.
        if (g_playerEntity.animation_frame_id == 8)    PlayEntitySnd(0);
        if (g_playerEntity.animation_frame_id == 0x16) PlayEntitySnd(0);

        entity_apply_walk_speed(0x5d);

        g_playerPosScratch.x = (int)g_playerEntity.unk_c6;
        g_playerPosScratch.z = (int)g_playerEntity.unk_c8;
        g_playerPosScratch.y = 0;
        entity_rotate_toward_target(&g_playerPosScratch, g_playerEntity.unk_de);
        Joint_move(player_joint_mirror(), g_playerEntity.jointMoveData0,
                   g_playerEntity.jointMoveData1, 0x400);
        Add_speedXZ(0);
        {
            int dz = g_playerEntity.scaMatrixData.localMatrix.t[2] - (int)g_playerEntity.unk_c8;
            int dx = g_playerEntity.scaMatrixData.localMatrix.t[0] - (int)g_playerEntity.unk_c6;
            if (SquareRoot0(dz * dz + dx * dx) < 0x96) {
                Flg_on((int)g_SysFlags, g_playerEntity.scd_anim_param);
                if ((g_playerEntity.healthStatusFlags & 0x80) == 0) {
                    g_playerEntity.animationId     = 1;
                    g_playerEntity.animFrameId     = 0;
                    g_playerEntity.action_behavior = 0;
                    g_playerEntity.action_state    = 0;
                }
            }
        }
        break;
    }
}
// 0x0044d5e0 - behaviour 3: turn toward the scripted destination, run to it, then
// decelerate to a stop. This is the "Chris runs forward" beat of the intro.
//   state 1  turn on the spot until aligned within 0x16a
//   state 3  run (anim 3, speed 0xd2), footstep on frames 0 and 10, until within
//            250 units of the target
//   state 5  four frames of anim 0 shedding 0x1e speed each, then
//   state 6  hand the player back to state 1 and raise the script's flag
// healthStatusFlags bit 7 short-circuits the deceleration and finishes at once.
static void player_scd_behavior_03(void)
{
    switch (g_playerEntity.action_state) {
    case 0:
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.attackAnim         = 2;
        g_playerEntity.action_state       = 1;
        g_playerEntity.unk_8c             = 3;
        // fall through
    case 1:
        g_playerPosScratch.x = (int)g_playerEntity.unk_c6;
        g_playerPosScratch.z = (int)g_playerEntity.unk_c8;
        g_playerPosScratch.y = 0;
        g_playerEntity.directionAngle += (short)turn_toward_target(
            &g_playerPosScratch, (short)g_playerEntity.unk_de);
        Joint_move(player_joint_mirror(), g_playerEntity.jointMoveData0,
                   g_playerEntity.jointMoveData1, 0x400);
        if ((short)turn_toward_target(&g_playerPosScratch, 0x16a) == 0) {
            g_playerEntity.action_state = 2;
        }
        break;

    case 2:
        g_playerEntity.move_speed_current = 0xd2;
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.attackAnim         = 3;
        g_playerEntity.action_state       = 3;
        g_playerEntity.unk_8c             = 3;
        // fall through
    case 3:
        // PlayEntitySnd takes ONE parameter (0x0047fbf0). The call sites push a
        // second dword (0 or -4) that the callee never reads - a dead push, the
        // same pattern as rotate_entity's fourth argument.
        if (g_playerEntity.animation_frame_id == 0)  PlayEntitySnd(1);
        if (g_playerEntity.animation_frame_id == 10) PlayEntitySnd(1);

        g_playerPosScratch.x = (int)g_playerEntity.unk_c6;
        g_playerPosScratch.z = (int)g_playerEntity.unk_c8;
        g_playerPosScratch.y = 0;
        entity_rotate_toward_target(&g_playerPosScratch, g_playerEntity.unk_de);
        Joint_move(player_joint_mirror(), g_playerEntity.jointMoveData0,
                   g_playerEntity.jointMoveData1, 0x400);
        Add_speedXZ(0);
        {
            int dz = g_playerEntity.scaMatrixData.localMatrix.t[2] - (int)g_playerEntity.unk_c8;
            int dx = g_playerEntity.scaMatrixData.localMatrix.t[0] - (int)g_playerEntity.unk_c6;
            int dist = SquareRoot0(dz * dz + dx * dx);

            if (dist < 0xfa) {
                if ((g_playerEntity.healthStatusFlags & 0x80) == 0) {
                    g_playerEntity.action_state = 4;
                } else {
                    Flg_on((int)g_SysFlags, g_playerEntity.scd_anim_param);
                }
                return;
            }
        }
        break;

    case 4:
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.attackAnim         = 0;
        g_playerEntity.action_state       = 5;
        g_playerEntity.unk_8c             = 3;
        g_playerEntity.attackDirection    = 0;   // +0xc4, the frame counter
        // fall through
    case 5:
        Joint_move(player_joint_mirror(), g_playerEntity.animHeader,
                   g_playerEntity.animBase, 0x400);
        g_playerEntity.attackDirection = (unsigned short)(g_playerEntity.attackDirection + 1);
        if ((short)g_playerEntity.attackDirection > 3) {
            g_playerEntity.action_state = 6;
        }
        *(short*)&g_playerEntity.move_speed_current -= 0x1e;
        Add_speedXZ(0);
        break;

    case 6:
        // 0x0044d8??: MOV dword ptr [.. + 0x84],1 - back to state 1 with
        // animFrameId, action_behavior and action_state cleared
        g_playerEntity.animationId     = 1;
        g_playerEntity.animFrameId     = 0;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state    = 0;
        Flg_on((int)g_SysFlags, g_playerEntity.scd_anim_param);
        return;
    }
}
// 0x0044d930 - behaviour 4: walk BACKWARDS to the scripted target. The player
// twin of npc_walk_backward_step: the facing is flipped 180 degrees, the normal
// "rotate toward target" step runs against the flipped heading, then the flip is
// undone - so the turn aligns the player's BACK with the target while
// Add_speedXZ(0x800) drives motion along that same flipped heading. This is the
// "step back" beat of a cutscene (Jill backing away before Barry kicks the door
// in room 1151).
//
// Only states 0 and 1 exist; anything else returns immediately. Unlike
// behaviours 2/3 this one animates from animHeader/animBase (+0x90/+0x94), NOT
// from jointMoveData0/1 (+0x15C/+0x160) - the two pairs are pushed from
// different globals at 0x0044da19 and 0x0044d537 respectively.
//
// Arrival is 100 units. Flg_on fires on every frame inside that radius, and the
// return to state 1 (one 32-bit store at +0x84) happens only when
// healthStatusFlags bit 7 is clear.
static void player_scd_behavior_04(void)
{
    if (g_playerEntity.action_state == 0) {
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.unk_8c             = 3;
        g_playerEntity.action_state       = 1;
        g_playerEntity.attackAnim         = 3;
    }
    else if (g_playerEntity.action_state != 1) {
        return;
    }

    // PlayEntitySnd takes ONE parameter; the call site pushes a dead second
    // dword. Unlike behaviour 2 the two frame tests share a single call here.
    if (g_playerEntity.animation_frame_id == 8 ||
        g_playerEntity.animation_frame_id == 0x16) {
        PlayEntitySnd(0);
    }

    // 0x0044d997-0x0044d9ae, transcribed as emitted. Both arms of the compare
    // fall into the 0x40 store - the `JNC` at 0x0044d9a8 targets it, and the
    // `JA` at 0x0044d9ac is only reachable with AL < 5 so it is never taken.
    // The 0x3c written first is therefore dead in every frame; it is kept so
    // the store order matches the original.
    g_playerEntity.move_speed_current = 0x3c;
    if (g_playerEntity.animation_frame_id >= 5 ||
        g_playerEntity.animation_frame_id <= 7) {
        g_playerEntity.move_speed_current = 0x40;
    }

    g_playerPosScratch.x = (int)g_playerEntity.unk_c6;
    g_playerPosScratch.z = (int)g_playerEntity.unk_c8;
    g_playerPosScratch.y = 0;

    // The +0x800 is masked into the 0..0xFFF angle space, the -0x800 is not.
    // That asymmetry is the original's (0x0044d9e7 / 0x0044da0e).
    *(unsigned short*)&g_playerEntity.directionAngle =
        (unsigned short)((*(unsigned short*)&g_playerEntity.directionAngle + 0x800) & 0xfff);
    entity_rotate_toward_target(&g_playerPosScratch, g_playerEntity.unk_de);
    g_playerEntity.directionAngle = (short)(g_playerEntity.directionAngle - 0x800);

    Joint_move(player_joint_mirror(), g_playerEntity.animHeader,
               g_playerEntity.animBase, 0x400);
    Add_speedXZ(0x800);

    int dz = g_playerEntity.scaMatrixData.localMatrix.t[2] - (int)g_playerEntity.unk_c8;
    int dx = g_playerEntity.scaMatrixData.localMatrix.t[0] - (int)g_playerEntity.unk_c6;
    if (SquareRoot0(dz * dz + dx * dx) < 100) {
        Flg_on((int)g_SysFlags, g_playerEntity.scd_anim_param);
        if ((g_playerEntity.healthStatusFlags & 0x80) == 0) {
            g_playerEntity.animationId     = 1;
            g_playerEntity.animFrameId     = 0;
            g_playerEntity.action_behavior = 0;
            g_playerEntity.action_state    = 0;
        }
    }
}
// 0x0044dc50 - behaviour 6: turn in place toward the scripted target. Rotates at
// a fixed 0x38 per frame and finishes when turn_toward_target reports the
// remaining angle closed at the script's own step (unk_de), then hands the player
// back to state 1.
static void player_scd_behavior_06(void)
{
    unsigned char st = g_playerEntity.action_state;
    if (st == 0) {
        g_playerEntity.animation_frame_id = 0;
        g_playerEntity.unk_bf             = 0;
        g_playerEntity.attackAnim         = 2;
        g_playerEntity.action_state       = 1;
        g_playerEntity.unk_8c             = 3;
    }
    else if (st != 1) {
        if (st != 2) {
            return;
        }
        g_playerEntity.animationId     = 1;
        g_playerEntity.animFrameId     = 0;
        g_playerEntity.action_behavior = 0;
        g_playerEntity.action_state    = 0;
        Flg_on((int)g_SysFlags, g_playerEntity.scd_anim_param);
        return;
    }

    g_playerPosScratch.x = (int)g_playerEntity.unk_c6;
    g_playerPosScratch.z = (int)g_playerEntity.unk_c8;
    g_playerPosScratch.y = 0;
    entity_rotate_toward_target(&g_playerPosScratch, 0x38);
    Joint_move(player_joint_mirror(), g_playerEntity.jointMoveData0,
               g_playerEntity.jointMoveData1, 0x400);
    if ((short)turn_toward_target(&g_playerPosScratch,
                                  (short)g_playerEntity.unk_de) == 0) {
        g_playerEntity.action_state = 2;
    }
}

// 0x004beca0 — 10 entries, indexed by action_behavior. The run of pointers ends
// at 0x004becc8 where string data begins.
static void* const g_playerScdBehaviors[10] = {
    (void*)player_scd_behavior_00,  // 0x0044cf80
    (void*)player_scd_behavior_01,  // 0x0044d100
    (void*)player_scd_behavior_02,  // 0x0044d380
    (void*)player_scd_behavior_03,  // 0x0044d5e0
    (void*)player_scd_behavior_04,  // 0x0044d930
    (void*)player_scd_behavior_05,  // 0x0044dac0
    (void*)player_scd_behavior_06,  // 0x0044dc50
    (void*)player_scd_behavior_07,  // 0x0044dd60
    (void*)player_scd_behavior_08,  // 0x0044de10
    (void*)player_scd_behavior_09,  // 0x0044deb0
};

// 0x0044cf30 — player state 8. Mirrors npc_state8_action_update exactly: run the
// handler, run it a second time when unk_e0 bit 1 asks for a double step, then
// refresh the held weapon's joint when bit 2 is set (bit 3 picks the hand).
// Note the handler is re-read from action_behavior on the second call - a handler
// that changes action_behavior redirects its own repeat.
static void player_state_08(void)
{
    unsigned char behavior = g_playerEntity.action_behavior;
    if (behavior >= 10) {
        player_scd_report("action_behavior out of range");
        return;
    }

    ((void(*)(void))g_playerScdBehaviors[behavior])();
    if ((g_playerEntity.unk_e0 & 2) != 0) {
        ((void(*)(void))g_playerScdBehaviors[g_playerEntity.action_behavior % 10])();
    }
    if ((g_playerEntity.unk_e0 & 4) != 0) {
        EntityUpdateWeaponJoint((g_playerEntity.unk_e0 & 8) >> 3);
    }
}

// 0x004d4550 — indexed by g_playerEntity.animationId.
//
// TEN entries, not sixteen. The table ends at 0x004d4577, because the animFrameId
// jump table used by player_state_01_control begins at 0x004d4578 (see
// g_playerCtrlFrameFunctions). The previous 16-entry declaration ran past the end
// and listed five animFrameId handlers as `player_state_10..14`; with the old
// `animationId & 0x0f` mask an animationId of 10-15 would have called an
// animFrameId handler as though it were a player state.
//
// The original applies neither a mask nor a bound:
//   00494da5: MOV AL,[0x00be6368]                  ; animationId
//   00494daa: CALL dword ptr [EAX*0x4 + 0x4d4550]
// animationId only ever reaches 8 in practice, so the bound added at the call site
// is port-only safety rather than a behaviour change.
static void* const g_playerStateFunctions[10] = {
    /* 0x0 */ (void*)player_state_init,           // 0x00494eb0
    /* 0x1 */ (void*)player_state_01_control,     // 0x00495180
    /* 0x2 */ (void*)player_state_02,             // 0x00495250
    /* 0x3 */ (void*)player_state_03,             // 0x00495270 -> FUN_00459be0
    /* 0x4 */ (void*)player_state_block_input,    // 0x00495280
    /* 0x5 */ (void*)player_state_anim_window0,   // 0x00495290
    /* 0x6 */ (void*)player_state_anim_window1,   // 0x004952d0
    /* 0x7 */ (void*)player_state_anim_window2,   // 0x00495310
    /* 0x8 */ (void*)player_state_08,             // 0x0044cf30
    /* 0x9 */ (void*)player_state_null,           // NULL in the original
};

// ============================================================================
// update_player_anim (0x00494d90)
// Per-frame player update: run the current state, resolve collisions, refresh
// the camera-zone membership flag, then the lighting and effect passes.
// ============================================================================
void update_player_anim(void)
{
    // 0x00494d95: aim the global current-entity pointer at the player
    ENTITY = (Entity*)&g_playerEntity;

    // 0x00494da1: only run the state machine when input/messages allow it
    if ((g_message_flags & 1) != 0) {
        // The original is an unmasked, unbounded CALL through the table. Bound it
        // to the real 10 entries instead of masking with 0x0f, which used to fold
        // 10-15 onto 0-5 and would now read past the array.
        if (g_playerEntity.animationId < 10) {
            ((void(*)(void))g_playerStateFunctions[g_playerEntity.animationId])();
        } else {
            player_state_report_missing("animationId >= 10, past 0x004d4550");
        }
        EntityUpdateLookAtAngles();
    }

    SetEntityScaHitData((Entity*)&g_playerEntity);

    // 0x00494dc4: skip collision in state 5 or behavior 0x11
    if ((g_playerEntity.animationId != 5) && (g_playerEntity.action_behavior != 0x11)) {
        HandleEnemyPlayerCollisions();
        check_room_collision((VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t,
                             *(short*)(g_playerEntity.Sca_info + 10));
    }

    g_playerEntity.scaMatrixData.field_00 = 0;

    // 0x00494e11: bit 0 of zoneFlags = player is inside the current camera zone
    g_playerEntity.zoneFlags &= 0xfe;
    g_playerEntity.zoneFlags |= (unsigned char)is_entity_in_switch_zone(
        (VECTOR*)g_playerEntity.scaMatrixData.localMatrix.t, g_CurrentRdtDataTypePtr);

    if ((((g_playerEntity.zoneFlags & 0x7f) != 0) &&
         (((unsigned char)g_playerEntity.unk_e0 & 0x40) == 0)) ||
        ((g_stageId == STAGE_GUARDHOUSE) && (g_roomId == ROOM_WATER_TANK_ENTRY))) {
        player_update_shadow_sprite((int)g_playerEntity.scaMatrixData.localMatrix.t,
                                    (int)&g_playerEntity.pushVelocity,
                                    (int)g_playerEntity.posY,
                                    (int)g_playerEntity.directionAngle);
    }

    // 0x00494e73: the mirror pass - see entity_draw_mirror_reflection in EntityCommon.cpp. Only
    // a room script (SCD opcode 0x0F) raises bit 0, so this is inert everywhere
    // except the rooms that actually have a mirror.
    if ((g_main_state_flags & MSF_MIRROR_ENABLE) != 0) {
        unsigned char lit = mirror_point_visible(
            (void*)((int)g_RdtPointer[1].lights + (unsigned int)g_roomCameraId * 0x2c - 4),
            (unsigned char)((g_main_state_flags & MSF_MIRROR_PLANE_X) != 0),
            (int)g_playerEntity.scaMatrixData.localMatrix.t);
        if (lit != 0) {
            entity_draw_mirror_reflection();
        }
    }

    player_update_detached_joint();
}

// ============================================================================
// FUN_0041b3c0 (0x0041b3c0)
// Is `pos` inside the axis-aligned box at `zone`? The zone is four uint16s:
// x, z, width, depth. The original compares UNSIGNED, so a coordinate left of or
// above the box wraps to a huge value and fails the <= test — that is what makes
// one comparison per axis sufficient. Reproduced deliberately.
// ============================================================================
static int is_point_in_action_zone(VECTOR* pos, unsigned short* zone) // 0x0041b3c0
{
    if (((unsigned int)(pos->x - (unsigned int)zone[0]) <= (unsigned int)zone[2]) &&
        ((unsigned int)(pos->z - (unsigned int)zone[1]) <= (unsigned int)zone[3])) {
        return 1;
    }
    return 0;
}

// ============================================================================
// no_room_action (0x0041c050) — room_check_actions[0]
// Literally `return 0`. It exists so slot 0 is a valid, inert handler rather than a
// null entry the original would have jumped through.
// ============================================================================
int no_room_action(unsigned char* entry)
{
    (void)entry;
    return 0;
}

// ============================================================================
// door_try_enter (0x0041b400) — room_check_actions[1]
//
// Ghidra called this use_mansion_key, after one of the messages it emits (0xc3).
// It is really the whole door interaction: reject, transition, or unlock.
//
// Called every frame while the player's reach probe is inside the door's action
// zone - there is NO action-button check. Confirmed by the two call sites of
// update_player_position: game_loop passes mask 1 and update_room_objects passes mask 4,
// so the entry flag byte is a mask *selector*, and a door with bit 0 set is tested
// every frame. Walking into the zone is the trigger.
//
// The door record is at entry+8. Its byte +0xC is the lock descriptor: bit 0x80 =
// locked, bit 0x40 = restricted to one character, low 6 bits = the lock's flag
// index in g_LocksFlags. Byte +0x16 is the item id required to unlock it.
// ============================================================================
extern int          get_item_slot(unsigned char itemId);
extern unsigned int Flg_ck(int baseAddr, unsigned int bitIndex);   // 0x00473f40
extern void         Flg_on(int baseAddr, unsigned int bitIndex);   // 0x00473ef0

// 0x0041b575: "it's locked" family. The index is biased by 200 into the message
// table, and a lock click plays first.
static void door_locked_message(unsigned int msgIndex)
{
    play_sfx(2, 0x14, 0);
    set_message_display(msgIndex + 200, 0xff);
}

// 0x0041b59c: the transition. An instant full-screen black rect, one frame of
// sleep, then StMask - not a fade. This is why the in-game door cut is instant.
static void door_begin_transition(unsigned char* record)
{
    g_pendingDoorRecord = (int)record;          // the room the transition will load
    g_main_state_flags |= MSF_GAMEPLAY_ACTIVE;
    g_message_flags = 0;

    g_rect.textureId = 0;
    g_rect.x = -160;
    g_rect.y = -120;
    g_rect.w = 320;
    g_rect.h = 240;
    g_rect.r = 0;
    g_rect.g = 0;
    g_rect.b = 0;

    // NOTE: the original writes a byte at 0x00be961b, which Ghidra labels
    // g_openMenuFlag. The port declares g_openMenuFlag at 0x00d22760 instead, so one
    // of the two addresses is wrong. Writing the port's symbol because that is the
    // menu/transition state machine the port actually implements - but if the screen
    // goes black and nothing advances, this is the first thing to check.
    g_openMenuFlag = 1;

    draw_rect(&g_rect, 0, 0);
    Task_sleep(1);
    StMask(0, 0);
}

int door_try_enter(unsigned char* entry)
{
    // 0x0041b400: already mid-transition (climb/door), do nothing.
    if ((g_main_state_flags & MSF_DOOR_TRANSITION) != 0) {
        return 0;
    }

    unsigned char* record = *(unsigned char**)(entry + 8);
    unsigned char  lock   = record[0xc];

    // 0x0041b41a: some doors are barred for one of the two characters.
    if ((lock & 0x40) != 0 && (g_playerEntity.id & 3) == 3) {
        set_message_display(0xd6, 0xff);
        return 0;
    }

    // 0x0041b441: unlocked outright, or its lock flag is already raised.
    if ((lock & 0x80) == 0 || Flg_ck((int)g_LocksFlags, lock & 0x3f) != 0) {
        door_begin_transition(record);
        return 0;
    }

    // Locked: work out whether the player can open it.
    unsigned int need = record[0x16];

    if (need == ITEM_SWORD_KEY && (g_playerEntity.id & 3) == 1) {
        // 0x0041b474: Jill substitutes the lockpick for this key, but only once
        // she has it (g_ScenarioFlags bit SCENARIO_FLAG_HAS_LOCKPICK).
        if (Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_HAS_LOCKPICK) == 0) {
            door_locked_message(0xd);
            return 0;
        }
        g_selectedItemId = lockpick_item_id();
    } else if (need == 0xfe) {
        // 0x0041b4a5: opens only from the other side.
        set_message_display(0xd4, 0xff);
        play_sfx(2, 0x21, 0);
        Flg_on((int)g_LocksFlags, record[0xc] & 0x3f);
        return 0;
    } else if (need == 0xff) {
        door_locked_message(0xb);
        return 0;
    } else {
        // 0x0041b4d7: needs a specific item.
        if (get_item_slot(need) < 0) {
            unsigned int m = need - ITEM_SWORD_KEY; // diff keys to get index
            if (m > 9) {
                m = 10;
            }
            door_locked_message(m);
            return 0;
        }
        g_eventItemUsedFlag = 1;
        g_selectedItemId = (unsigned char)need;
    }

    // 0x0041b4f5: the key turns. Note this only UNLOCKS - it does not transition.
    // The next frame's zone hit finds the lock flag raised and walks through.
    set_message_display(0xc3, 0xff);

    int sfxId;
    if (g_stageId == STAGE_LABORATORY && g_roomId == ROOM_LAB_B3_O_PASSAGE) {
        play_sfx(2, 0x17, 0);
        play_sfx(2, 0x18, 0);
        sfxId = 0x19;
    } else {
        sfxId = 0x22;
    }
    play_sfx(2, sfxId, 0);
    Flg_on((int)g_LocksFlags, record[0xc] & 0x3f);
    return 0;
}

// ============================================================================
// check_door (0x0041b6d0) — room_check_actions[5]
//
// Called from update_player_position when the player enters a door's action zone.
// It does not open anything itself: it records which side the player approached
// from, then raises the two flags the rest of the transition watches -
// has_enter_switch_zone bit 0x20 (which player_input_to_behavior turns into
// action_behavior 0x11) and g_main_state_flags2 bit 0x400000 (which the door
// animation consumes to pick its turn direction).
//
// The entry's +8 field is the 24-byte door record cmd_door_set stored. Its first
// u16 is the zone origin and its third is the zone width; comparing the player's X
// against origin + width/2 is what picks the approach side. The angle test bails out
// entirely when the player is facing the wrong way, leaving only the 0x20 flag set.
//
// Returns 0. cmd_room_action discards this, but update_player_position propagates
// it as its own return value.
// ============================================================================
int check_door(unsigned char* entry)
{
    // The original writes entity offsets 0x85 and 0xC4 through the generic ENTITY
    // pointer. On PlayerEntity those are animFrameId and attackDirection, but the
    // Entity struct names them differently, so they are addressed by offset rather
    // than retargeted onto a same-offset field with an unrelated name.
    unsigned char* ent   = (unsigned char*)ENTITY;
    short*         appr  = (short*)(ent + 0xC4);   // PlayerEntity::attackDirection
    unsigned char* afid  = ent + 0x85;             // PlayerEntity::animFrameId

    unsigned short* record = *(unsigned short**)(entry + 8);
    int playerX = ENTITY->scaMatrixData.localMatrix.t[0];
    short angle = (short)ENTITY->angle;

    bool sideResolved = false;

    if ((playerX - (int)record[0]) < (int)(unsigned int)(record[2] >> 1)) {
        if ((((int)angle + 0x400) & 0x800) == 0) {
            *appr = 1;
            sideResolved = true;
        }
    } else {
        if ((((int)angle - 0x400) & 0x800) == 0) {
            *appr = -1;
            sideResolved = true;
        }
    }

    if (sideResolved) {
        *afid = 0;                   // route into the frame-0/1 handlers
        g_message_flags &= 0xffbf;
        ENTITY->has_enter_switch_zone &= 0xef;
        // Bit 0x10 records that the door swings the opposite way, which the door
        // animation reads to pick attackAnim 0x35 instead of 0x33.
        if (((*appr >> 1) ^ *(unsigned short*)(entry + 2)) & 1) {
            ENTITY->has_enter_switch_zone |= 0x10;
        }
    }

    ENTITY->has_enter_switch_zone |= 0x20;
    g_main_state_flags2 |= MSF2_DOOR_TURN_PENDING;
    return 0;
}

// ============================================================================
// Remaining room_check_actions handlers (0x0041b630 - 0x0041bf90)
//
// Each takes the 12-byte g_RoomActionTable entry. Entries without flag 0x80
// are probed every frame by update_player_position; entries WITH flag 0x80 only
// by check_action_object on the action-key press. Handlers return 0 when they
// acted; set_key_flag and set_room_event_flag return 1 when their +2 field is
// nonzero, which is what selects the action_behavior 0x0c interaction animation.
// ============================================================================

extern void ScdEventEntry_Create(unsigned int slot, int scriptIndex);  // RoomEvents.cpp 0x0041d650

// ============================================================================
// display_msg_room_action (0x0041b630) — room_check_actions[2]
// Shows the message whose id and pause flag live in the entry at +2/+4.
// ============================================================================
int display_msg_room_action(unsigned char* entry)
{
    set_message_display(*(unsigned short*)(entry + 2), *(unsigned short*)(entry + 4));
    return 0;
}

// ============================================================================
// include_key (0x0041b650) — room_check_actions[3]
// "You got the key" prompt. Skips the prompt when the equipped item already is
// the key the record needs (record+8 = item id); otherwise arms the event and
// shows message 0xc1 so the follow-up (message action 10) picks it up.
// ============================================================================
int include_key(unsigned char* entry)
{
    if (g_EquippedItemId != 0 &&
        ((unsigned char*)g_ItemSlotsPointer)[-2 + (unsigned int)g_EquippedItemId * 2] ==
            *(unsigned char*)(*(unsigned char**)(entry + 8) + 8)) {
        return 0;
    }
    g_pRoomActionEntry = entry;
    set_message_display(0xc1, 0xff);
    return 0;
}

// ============================================================================
// set_key_flag (0x0041b6a0) — room_check_actions[4]
// Raises msf bit 11 (0x800) when the entry's +2 field is 0; otherwise returns 1,
// which routes the player into the 0x0c reach animation. g_pRoomEventIndex is
// armed either way so the animation completion can branch on the entry id.
//
// msf 0x800 is menu mode 3/4 (the item viewer: 3D model + description). The
// original disassembly is `OR dword ptr [0x00be41c0], 0x800` — a previous
// revision wrote 0x200 (mode 5, the map display), which is why a pickup opened
// the menu on the map tab instead of the item model.
// ============================================================================
int set_key_flag(unsigned char* entry)
{
    g_pRoomActionEntry = entry;
    if (*(unsigned short*)(entry + 2) == 0) {
        g_main_state_flags |= MSF_MENU_MODE_ITEM_VIEW;
        return 0;
    }
    return 1;
}

// ============================================================================
// check_door_side (0x0041b790) — room_check_actions[6]
// The Z-axis sibling of check_door: records the approach side for doors whose
// zone spans the Z axis (check_door splits on X). Same flag protocol -
// zoneFlags bits 0x20/0x10 (0x60 here) and msf2 0x400000 feed the door animation.
// ============================================================================
int check_door_side(unsigned char* entry)
{
    unsigned char* ent = (unsigned char*)ENTITY;
    short*         appr = (short*)(ent + 0xC4);   // PlayerEntity::attackDirection
    unsigned char* afid = ent + 0x85;             // PlayerEntity::animFrameId

    unsigned short* record = *(unsigned short**)(entry + 8);
    int playerZ = ENTITY->scaMatrixData.localMatrix.t[2];
    short angle  = (short)ENTITY->angle;

    bool sideResolved = false;

    if ((playerZ - (int)record[1]) < (int)(unsigned int)(record[3] >> 1)) {
        if ((((int)angle + 0x800) & 0x800) == 0) {
            *appr = 1;
            sideResolved = true;
        }
    } else {
        // Original reads the high byte of the angle word (directionAngle + 1)
        // and tests bit 3 (= angle bit 0x800).
        if ((*(unsigned char*)(ent + 0x75) & 8) == 0) {
            *appr = -1;
            sideResolved = true;
        }
    }

    if (sideResolved) {
        *afid = 0;
        g_message_flags &= 0xffbf;
        ENTITY->has_enter_switch_zone &= 0xef;
        if (((*appr >> 1) ^ *(unsigned short*)(entry + 2)) & 1) {
            ENTITY->has_enter_switch_zone |= 0x10;
        }
    }

    ENTITY->has_enter_switch_zone |= 0x60;
    g_main_state_flags2 |= MSF2_DOOR_TURN_PENDING;
    return 0;
}

// ============================================================================
// flag_bank_set (0x0041b850) — room_check_actions[7]
// Sets or clears one bit of a selected flag bank. Entry +2 selects the bank
// (0=Player, 1=Player3, 2=Locks, 3=RoomEvent, 4=Sys, 5=main_state_flags,
// 6=message_flags, 7=roomItems, 8=Room, 9=g_itemUseFlags), +4 the bit index
// (MSB-first: bit 0 is 0x80000000), +6 nonzero = set, zero = clear.
// ============================================================================
int flag_bank_set(unsigned char* entry)
{
    unsigned int* pFlags;
    switch (*(unsigned short*)(entry + 2)) {
    case 0:  pFlags = (unsigned int*)&g_ScenarioFlags[(*(unsigned short*)(entry + 4) >> 3) & ~3u]; break;
    case 1:  pFlags = (unsigned int*)&g_ScenarioFlags2[(*(unsigned short*)(entry + 4) >> 3) & ~3u]; break;
    case 2:  pFlags = (unsigned int*)&g_LocksFlags[(*(unsigned short*)(entry + 4) >> 3) & ~3u]; break;
    case 3:  pFlags = (unsigned int*)&g_EnemiesFlags[(*(unsigned short*)(entry + 4) >> 3) & ~3u]; break;
    case 4:  pFlags = (unsigned int*)((char*)g_SysFlags + ((*(unsigned short*)(entry + 4) >> 3) & ~3u)); break;
    // Banks 5 and 6 do NOT add the byte offset here - the original's arms are a
    // bare `mov edx, 0xbe41c0` / `mov edx, 0xbebcc0` (0x0041b8e4 / 0x0041b8eb),
    // so this room action can only reach each bank's FIRST dword. cmd_bit_op has
    // no such limit, which is how scripts reach msf2 (selector 0x20+).
    case 5:  pFlags = (unsigned int*)g_MainStateFlagBank; break;   // first dword only
    case 6:  pFlags = (unsigned int*)&g_message_flags; break;
    case 7:  pFlags = (unsigned int*)&g_roomItemsFlags[(*(unsigned short*)(entry + 4) >> 3) & ~3u]; break;
    case 8:  pFlags = (unsigned int*)&g_RoomFlags[(*(unsigned short*)(entry + 4) >> 3) & ~3u]; break;
    default: pFlags = (unsigned int*)((char*)g_itemUseFlags + ((*(unsigned short*)(entry + 4) >> 3) & ~3u)); break;
    }

    unsigned char bit = (unsigned char)*(unsigned short*)(entry + 4);
    if (*(unsigned short*)(entry + 6) == 0) {
        *pFlags &= ~(0x80000000U >> (bit & 0x1f));
    } else {
        *pFlags |= 0x80000000U >> (bit & 0x1f);
    }
    return 0;
}

// ============================================================================
// open_itembox (0x0041b990) — room_check_actions[8]
// Starts the itembox interaction: gates on the box not already opening, the
// player not being attacked, no menu state (msf byte 1) and the message system
// being ready, then raises g_itembox_state 1 (the lid opens - check_itembox_state
// animates it) and silences the message lines so the box menu can take over.
// ============================================================================
int open_itembox(unsigned char* entry)
{
    if ((g_itembox_state == 0) &&
        (g_playerEntity.isBeingAttackedFlag == 0) &&
        ((g_main_state_flags & MSF_MENU_PENDING) == 0) &&
        ((unsigned short)g_message_flags & 0x40) != 0) {
        g_itembox_state = 1;
        g_message_flags = (unsigned short)g_message_flags & 0xffba;
        play_sfx(2, 0x20, 0);
        g_pRoomActionEntry = entry;
    }
    return 0;
}

// ============================================================================
// create_room_event (0x0041b9e0) — room_check_actions[9]
// Spawns an SCD event script slot from the entry's +2/+4 fields.
// ============================================================================
int create_room_event(unsigned char* entry)
{
    ScdEventEntry_Create(*(unsigned char*)(entry + 2), *(unsigned char*)(entry + 4));
    return 0;
}

// ============================================================================
// room_action_noop10 (0x0041ba00) — room_check_actions[0x0A]
// Literally `return 0` - a reserved slot in the original.
// ============================================================================
int room_action_noop10(unsigned char* entry)
{
    (void)entry;
    return 0;
}

// ============================================================================
// room_action_effect (0x0041ba10) — room_check_actions[0x0B]
// While the player is moving (move_speed_current > 0) and the render frame is
// the non-blank one, spawns a dust billboard under the player and cycles its
// position through the six-entry tables at 0x004b9310/0x004b9328. Also raises
// msf2 bit 0 (the "cannot die" guard) for the duration.
// ============================================================================
int room_action_effect(unsigned char* entry)
{
    (void)entry;
    static int g_roomActionEffectIndex = 0;   // 0x004b9308
    static const int g_roomActionEffectX[6] = {0, -346, -346, 0, 346, 346};
    static const int g_roomActionEffectZ[6] = {-400, 346, -346, 346, 200, 346};

    g_main_state_flags2 |= MSF2_EFFECT_ZONE;
    if ((0 < g_playerEntity.move_speed_current) && (g_spriteAnimActive != 0)) {
        VECTOR pos;
        pos.x = g_roomActionEffectX[g_roomActionEffectIndex];
        pos.y = (int)DAT_00d226e8 - g_playerEntity.scaMatrixData.localMatrix.t[1];
        pos.z = g_roomActionEffectZ[g_roomActionEffectIndex];
        Effect_CreateBillboard(0x17, 8, 0, &g_playerEntity.scaMatrixData.localMatrix, &pos, 0);
        g_roomActionEffectIndex = (g_roomActionEffectIndex + 1) % 6;
    }
    return 0;
}

// ============================================================================
// set_stairs_zone (0x0041baa0) — room_check_actions[0x0C]
// Marks the player as inside a stairs/ladder zone: zoneFlags bit 0x20 (in zone,
// plus 0x10 for the ladder variant when +2 != 0), latches the ladder base
// position into unk_c6/unk_c8, toggles the entry's +2 flag byte (so the next
// zone hit flips it back) and raises msf bit 4 (ladder mode). The +2 toggle is
// what distinguishes the two ends of a two-way ladder.
// ============================================================================
int set_stairs_zone(unsigned char* entry)
{
    unsigned char prev = g_playerEntity.zoneFlags;
    g_playerEntity.zoneFlags |= 0x20;
    if (*(unsigned short*)(entry + 2) != 0) {
        g_playerEntity.zoneFlags = prev | 0x30;
    }
    g_playerEntity.unk_c6 = *(unsigned short*)(entry + 4);
    g_playerEntity.unk_c8 = *(unsigned short*)(entry + 6);
    *(unsigned char*)(entry + 2) ^= 1;
    g_main_state_flags |= MSF_LADDER_DOWN;
    return 0;
}

// ============================================================================
// set_room_event_flag (0x0041bae0) — room_check_actions[0x0D]
// Same shape as set_key_flag but for msf bit 8 (0x100). Returns 1 when the
// entry's +2 field is nonzero, selecting the 0x0c interaction animation whose
// completion re-raises 0x100 for this entry id.
// ============================================================================
int set_room_event_flag(unsigned char* entry)
{
    g_pRoomActionEntry = entry;
    if (*(unsigned short*)(entry + 2) == 0) {
        g_main_state_flags |= MSF_PICKUP_SCREEN;
        return 0;
    }
    return 1;
}

// ============================================================================
// check_desk (0x0041bb10) — room_check_actions[0x0E]
// The desk interaction. Gates on the desk flow idle and the message system
// ready, then:
//   - the roomItems flag named by entry[eventIdx].field6 must be SET (the desk
//     still has something to give)
//   - Jill (id&3 == 3) gets turned away (message 0xd7)
//   - a locked desk (LocksFlags bit at entry+2 clear) needs the small key
//     (0x3d) or Jill's lockpick (ScenarioFlags bit 0x7c) - otherwise "locked"
//     (0xd8); with the key it arms the desk-open state (g_desk_check_state 1)
//   - an unlocked desk swings open: mark its model opened (byte 0 of
//     g_item_model_table[entry[eventIdx].field4] |= 1), cut to the desk
//     camera (entry+6), and run the camera-zone walk to the new cut.
// ============================================================================
int check_desk(unsigned char* entry)
{
    if ((g_desk_check_state == 0) &&
        (g_playerEntity.isBeingAttackedFlag == 0) &&
        ((g_main_state_flags & MSF_MENU_PENDING) == 0) &&
        ((unsigned short)g_message_flags & 0x40) != 0) {
        unsigned short eventIdx = *(unsigned short*)(entry + 4);
        // The flag index and item-model slot index live in the +6/+4
        // fields of the event-table entry at index eventIdx
        // (ITEMS_FLAGS = g_RoomActionTable+6).
        unsigned short itemFlagIdx = *(unsigned short*)((unsigned char*)g_RoomActionTable + 6 + (unsigned int)eventIdx * 0xc);
        if (Flg_ck((int)g_roomItemsFlags, itemFlagIdx) != 0) {
            if ((g_playerEntity.id & 3) == 3) {
                set_message_display(0xd7, 0xff);
                return 0;
            }
            if (Flg_ck((int)g_LocksFlags, *(unsigned short*)(entry + 2)) == 0) {
                if ((get_item_slot(ITEM_DESK_KEY) < 0) && (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_HAS_LOCKPICK) == 0)) {
                    set_message_display(0xd8, 0xff);
                    return 0;
                }
                g_pRoomActionEntry = entry;
                g_desk_check_state = 1;
                return 0;
            }
            // Desk already unlocked: swing the lid open and cut to its camera.
            g_pRoomActionEntry = entry;
            unsigned short modelSlot = *(unsigned short*)((unsigned char*)g_RoomActionTable + 4 + (unsigned int)eventIdx * 0xc);
            ((unsigned char*)g_item_model_table[modelSlot])[0] |= 1;
            play_sfx(2, 0x24, 0);
            g_cutId = g_roomCameraId;
            g_roomCameraId = *(unsigned char*)(entry + 6);
            g_desk_check_state = 35;
            // Walk the camera-zone list to the new camera (same as cmd_current_cut_set).
            unsigned short camId = *(unsigned short*)((char*)g_RdtPointer->cam_switch_zones + 2);
            unsigned int zonePtr = (unsigned int)g_RdtPointer->cam_switch_zones;
            while (camId != g_roomCameraId) {
                g_CurrentRdtDataTypePtr = (void*)(zonePtr + 0x14);
                camId = *(unsigned short*)(zonePtr + 0x16);
                zonePtr = (unsigned int)g_CurrentRdtDataTypePtr;
            }
            g_message_flags = (unsigned short)g_message_flags & 0xffba;
            g_CurrentRdtDataTypePtr = (void*)zonePtr;
            StMask(0, 0);
        }
    }
    return 0;
}

// 0x004885a0 - mark a map "owned" in the RoomFlags bank. The argument is the
// MAP INDEX (itemId - ITEM_MAP_FIRST); map_area_known reads the bits back.
static void set_room_item_seen_flag(int mapIndex)
{
    Flg_on((int)g_RoomFlags, mapIndex + ROOM_FLAG_MAP_BASE);
}

// ============================================================================
// pickup_key_event (0x0041be70) — room_check_actions[0x0F]
// Direct key pickup: deactivates the entry, clears the item model's
// byte 0, clears the roomItems flag at record+0x14, marks the item
// "seen" in RoomFlags (bit 0x7c + itemId - 0x4e) and records the item id in
// g_pickedItemId for the message system.
// ============================================================================
int pickup_key_event(unsigned char* entry)
{
    *entry = 0;
    ((unsigned char*)g_item_model_table[*(unsigned short*)(entry + 4)])[0] = 0;
    FUN_00473f10((int*)&g_roomItemsFlags, *(unsigned char*)(*(unsigned char**)(entry + 8) + 0x14));
    set_room_item_seen_flag(ITEM_TO_MAP_INDEX(*(unsigned char*)(*(unsigned char**)(entry + 8) + 8)));
    g_pickedItemId = *(unsigned char*)(*(unsigned char**)(entry + 8) + 8);
    return 0;
}

// ============================================================================
// check_typewriter (0x0041bed0) — room_check_actions[0x10]
// The save-point interaction. With the typewriter idle and the message system
// ready: an ink ribbon (item 0x2f) in the inventory starts the save flow
// (g_typewriter_state 1, ribbon slot remembered at entry+2); Chris (ids 1/5)
// may save without a ribbon until ScenarioFlags bit 0x7b is set; otherwise the
// "no ink ribbon" message (0xde) plays.
// ============================================================================
int check_typewriter(unsigned char* entry)
{
    if ((g_typewriter_state == 0) &&
        ((g_main_state_flags & MSF_MENU_PENDING) == 0) &&
        ((unsigned short)g_message_flags & 0x40) != 0) {
        int ribbonSlot = get_item_slot(ITEM_INK_RIBBONS);
        if (ribbonSlot >= 0) {
            g_pRoomActionEntry = entry;
            *(unsigned short*)(entry + 2) = (unsigned short)ribbonSlot;
            g_typewriter_state = 1;
            g_message_flags = (unsigned short)g_message_flags & 0xffba;
            return 0;
        }
        if ((g_playerEntity.id == 1) || (g_playerEntity.id == 5)) {
            if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
                g_pRoomActionEntry = entry;
                *(unsigned short*)(entry + 2) = (unsigned short)ribbonSlot;
                g_typewriter_state = 1;
                g_message_flags = (unsigned short)g_message_flags & 0xffba;
                return 0;
            }
        }
        set_message_display(0xde, 0xff);
        g_typewriter_state = 0;
    }
    return 0;
}

// ============================================================================
// stairs_height_update (0x0041bf90) — room_check_actions[0x11]
// Sets the player's height on stairs. Entry +2 selects which edge of the zone
// (+4 length, +6 step) the height ramps from; the player's Y and posY are
// set to (distance/stepCount + 1) * step so walking the zone climbs smoothly.
// ============================================================================
int stairs_height_update(unsigned char* entry)
{
    unsigned short* zone = *(unsigned short**)(entry + 8);
    int local4;
    switch (*(unsigned short*)(entry + 2)) {
    case 0:  local4 = g_playerEntity.scaMatrixData.localMatrix.t[0] - (unsigned int)zone[0]; break;
    case 1:  local4 = ((unsigned int)zone[2] + (unsigned int)zone[0]) - g_playerEntity.scaMatrixData.localMatrix.t[0]; break;
    case 2:  local4 = g_playerEntity.scaMatrixData.localMatrix.t[2] - (unsigned int)zone[1]; break;
    default: local4 = ((unsigned int)zone[1] + (unsigned int)zone[3]) - g_playerEntity.scaMatrixData.localMatrix.t[2]; break;
    }
    int step = ((short)(local4 / (int)(unsigned int)*(unsigned short*)(entry + 4)) + 1) *
               (int)*(short*)(entry + 6);
    g_playerEntity.scaMatrixData.localMatrix.t[1] = step;
    g_playerEntity.posY = (unsigned short)step;
    return 0;
}

// ============================================================================
// update_player_position (0x0041c060)
// Despite the name this does not move the player: it tests the player against
// every entry of the room item/door event table and fires the matching
// room_check_actions handler. This is the door / item / examine interaction
// layer.
//
// Two probe points are used. Entries without flag 0x40 are tested against a
// point 600 units in front of the player (the reach probe, g_playerPosScratch);
// entries with 0x40 are tested against the player's actual position. Flag 0x80
// disables an entry. `mask` gates which entries participate this frame.
// ============================================================================

void update_player_position(PlayerEntity* ent, int mask)
{
    int actionResult = 0;

    // 0x0041c060: build the reach probe 600 units ahead of the facing direction
    g_svecScratch.x = 600;
    g_svecScratch.z = 0;
    MovePlayerXZ(g_playerEntity.directionAngle, &g_svecScratch, &g_svecScratch);
    g_playerPosScratch.x = g_svecScratch.x + g_playerEntity.scaMatrixData.localMatrix.t[0];
    g_playerPosScratch.z = g_svecScratch.z + g_playerEntity.scaMatrixData.localMatrix.t[2];

    // 0x0041c0a8: clear the action-available bit
    ENTITY->has_enter_switch_zone &= 0xdf;

    // 0x0041c0b1: bail out when the action table is empty. The original compares
    // the tail pointer against g_RoomActionTable - 1.
    unsigned char* tail = (unsigned char*)g_RoomActionTail;
    if (tail == NULL || tail < g_RoomActionTable) {
        return;
    }

    unsigned char* entry = g_RoomActionTable;
    char index = 0;
    do {
        unsigned char flags = entry[1];
        if ((*entry != 0) && ((mask & flags) != 0) && ((~flags & 0x80) != 0)) {
            bool hit = false;
            if ((flags & 0x40) == 0) {
                if (is_point_in_action_zone((VECTOR*)&g_playerPosScratch,
                                            *(unsigned short**)(entry + 8)) != 0) {
                    g_fwdPosActionId = (unsigned char)(index + 1);
                    hit = true;
                }
            } else {
                if (is_point_in_action_zone((VECTOR*)ent->scaMatrixData.localMatrix.t,
                                            *(unsigned short**)(entry + 8)) != 0) {
                    g_entPosActionId = (unsigned char)(index + 1);
                    hit = true;
                }
            }

            // 0x0041c125: dispatch the room action handler
            if (hit) {
                void* handler = room_check_actions[*entry];
                if (handler != NULL) {
                    actionResult = ((int(*)(unsigned char*))handler)(entry);
                }
            }
        }
        entry += 12;
        index++;
    } while (entry <= tail);

    (void)actionResult;
}
