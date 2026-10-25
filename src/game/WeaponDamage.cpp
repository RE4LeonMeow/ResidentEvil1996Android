// WeaponDamage.cpp — Real-time weapon hit detection and damage application
// apply_weapon_damage @ 0x0043c020
// Called each frame when the player fires a weapon. Iterates all active
// enemies, calls per-weapon hit detection callbacks (knife cone, gun cone,
// projectile distance) to find the closest target in range+FOV, then
// subtracts damage from the enemy's health and sets the hit reaction state.
#include "../Globals.h"
#include "../DebugPrint.h"   // dbg_printf
#include <cstdlib>                   // rand() - MSVC got this via <windows.h>
#include "entities/EntityCommon.h"   // ENEMY_* / NPC_* type ids
#include "dc/WeaponDamageTables.h"    // DC per-mode damage columns (generated)

// ---- Global scratch variable (set by apply_weapon_damage before hit detection) ----
extern int g_scaled_down_dist;  // holds weapon_id - 1 during hit detection
extern unsigned int g_entity_bkp;   // 0x00be0df4 - shared scratch (health snapshot)

// ---- Forward declarations (post-hit callbacks + reactions, defined at the end) ----
static void weapon_post_hit_knife(Entity* enemy);      // 0x0043c290
static void weapon_post_hit_reaction(Entity* enemy);   // 0x0043c350
static void weapon_post_hit_shotgun(Entity* enemy);    // 0x0043c370
static void weapon_post_hit_blood(Entity* enemy);      // 0x0043c3b0
static void weapon_post_hit_blood2(Entity* enemy);     // 0x0043c770
static void weapon_post_hit_sparks(Entity* enemy);     // 0x0043ca30
static void weapon_post_hit_blood3(Entity* enemy);     // 0x0043cc90
static void enemy_hit_reaction_zombie(Entity* enemy);  // 0x0043d060
static void enemy_hit_reaction_basic(Entity* enemy);   // 0x0043d2c0
static void enemy_hit_reaction_head(Entity* enemy);    // 0x0043d300
static void enemy_hit_reaction_blood(Entity* enemy);   // 0x0043d3a0
static void enemy_hit_reaction_none(Entity* enemy);    // 0x0043d400

// ---- Forward declarations ----
// room_check_sight_blocked (0x0047db90) comes from Globals.h.

// ============================================================================
// check_weapon_line_of_sight @ 0x0048a530
// Multi-layer room obstruction check for weapon hits. Computes the vector
// from the player to the hit position, then checks 4 room partition layers
// (indices 3 down to 0) via room_check_sight_blocked. Returns 0 only if
// all layers are clear — the shot has an unobstructed path.
// ============================================================================
unsigned char check_weapon_line_of_sight(VECTOR* hitPos)
{
    unsigned char blocked = 0;
    VECTOR dir;
    dir.x = hitPos->x - (int)g_playerEntityPointer.scaMatrixData.localMatrix.t[0];
    dir.y = hitPos->y - (int)g_playerEntityPointer.scaMatrixData.localMatrix.t[1];
    dir.z = hitPos->z - (int)g_playerEntityPointer.scaMatrixData.localMatrix.t[2];

    for (unsigned char layer = 3; layer != 0xFF; layer--) {
        blocked |= (unsigned char)room_check_sight_blocked(&dir, layer);
    }
    return blocked;
}
extern void MovePlayerXZ(int angle, SVECTOR* offset, SVECTOR* out);  // movement helper

// ---- 2D cross product helper ----
static int compute_2d_cross_product(int x1, int z1, int x2, int z2)
{
    return z2 * x1 - x2 * z1;
}

// ---- Weapon damage data tables ----
extern void* PTR_weapons_hit_detection_functions[10];
extern void* PTR_post_hit_callbacks[10];
extern unsigned int weapons_ranges[20];

// 0x004bb698 - first-playthrough hit records, 12 bytes each,
// indexed (weaponAdj + enemyType * 10) * 12. The knockback vector (kx/ky/kz)
// is used for BOTH playthroughs; damage/hit-state come from these on the
// first playthrough, or from the second-playthrough table below when
// Flg_ck(g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) is set
// (bit 0x7B = second playthrough, set by EndingScreen after clearing).
typedef struct {
    short         kx;      // +0x00
    short         ky;      // +0x02
    short         kz;      // +0x04
    short         dmg;     // +0x06: first-playthrough damage
    unsigned char type;    // +0x08: hit type for the post-hit billboard
    unsigned char data;    // +0x09: hit data for the post-hit billboard
    unsigned char hit;     // +0x0A: first-playthrough hit state
    unsigned char pad;     // +0x0B
} WeaponHitRecordFirstRun;

// 0x004bbffe - second-playthrough records, 12 bytes each. apply_weapon_damage
// reads the damage short at +0 and the hit-state byte at +4; nothing reads the
// remaining fields. Note this table starts 6 bytes AFTER the first-playthrough
// table ends (0x004bbff8), so the kx/ky/kz words at +6..+0x0B are physically
// the NEXT first-playthrough record's knockback vector - that phase shift is in
// the original data, not a transcription error.
typedef struct {
    short         dmg;     // +0x00: second-playthrough damage
    short         unk_02;  // +0x02
    unsigned char hit;     // +0x04: second-playthrough hit state
    unsigned char unk_05;  // +0x05
    short         kx;      // +0x06: unread (aliases the next first-run record)
    short         ky;      // +0x08: unread
    short         kz;      // +0x0A: unread
} WeaponHitRecordSecondRun;

extern WeaponHitRecordFirstRun       g_weaponHitRecordsFirstRun[200];
extern WeaponHitRecordSecondRun  g_weaponHitRecordsSecondRun[200];

// ============================================================================
// checkEntityInRangeCone @ 0x0043d590
// Checks if an entity lies within a triangular aim cone in front of the
// player. Uses 2D cross products to test if the entity position falls
// between the left and right cone boundaries, and within the far edge.
// Keeps track of the closest entity via g_playerDisplacement.
// ============================================================================
static unsigned char checkEntityInRangeCone(short* leftBound, short* rightBound, Entity* enemy)
{
    int dist_x = *(int*)&enemy->scaMatrixData.localMatrix.t[0]
               - (int)g_playerEntityPointer.scaMatrixData.localMatrix.t[0];
    int dist_z = *(int*)&enemy->scaMatrixData.localMatrix.t[2]
               - (int)g_playerEntityPointer.scaMatrixData.localMatrix.t[2];

    // Check entity is to the LEFT of the right boundary
    int cross = compute_2d_cross_product(
        (int)g_svecScratch.x, (int)g_svecScratch.z,
        dist_x - *leftBound, dist_z - leftBound[2]);
    if (cross > 0) return 0;

    // Check entity is to the RIGHT of the left boundary
    cross = compute_2d_cross_product(
        (int)g_svecScratch.x, (int)g_svecScratch.z,
        dist_x - *rightBound, dist_z - rightBound[2]);
    if (cross < 0) return 0;

    // Check entity is BEFORE the far edge
    cross = compute_2d_cross_product(
        (int)*leftBound - (int)*rightBound, (int)leftBound[2] - (int)rightBound[2],
        dist_x - (int)g_svecScratch.x, dist_z - (int)g_svecScratch.z);
    if (cross > 0) return 0;

    // Entity is inside the cone — keep closest
    unsigned int distance = SquareRoot0(dist_z * dist_z + dist_x * dist_x);
    if (distance < g_playerDisplacement) {
        g_playerDisplacement = distance;
        return 1;
    }
    return 0;
}

// ============================================================================
// weapon_hit_detect_knife @ 0x0043d690
// Knife hit detection. Computes knife reach from the player's weapon joint,
// adjusts range per enemy type, and checks Euclidean distance. Returns 1
// if the entity is the closest within knife reach.
// ============================================================================
static unsigned char weapon_hit_detect_knife(short range, Entity* enemy)
{
    short offsets[6] = { 0x99, 0, 0, -0x17C, 0, 0 };

    g_matrixScratch = g_identityMatrixData;

    unsigned int pid = (unsigned int)(g_playerEntityPointer.id & 1);
    g_matrixScratch.t[0] = (int)offsets[pid * 3];
    g_matrixScratch.t[1] = (int)offsets[pid * 3 + 1];
    g_matrixScratch.t[2] = (int)offsets[pid * 3 + 2];

    // ApplyLVAndMul0Matrix writes a whole MATRIX, not an SVECTOR: the original
    // reserves 32 bytes (SUB ESP,0x20), fills the first 12 with the offset pair
    // above, and then hands that same buffer to 0x0040a1f0 as the output matrix
    // (0x0043d6e5: LEA EAX,[ESP+0xc] -> PUSH EAX). Declaring the destination as
    // an 8-byte SVECTOR let the call write 24 bytes past it - /GS caught it as
    // "stack around the variable 'knifePos' was corrupted" the first time the
    // knife swung - and the two reads below took the rotation instead of the
    // translation.
    MATRIX knifeMtx;
    ApplyLVAndMul0Matrix(&g_playerEntityPointer.jointsStructs[0xE].world,
                         &g_matrixScratch, &knifeMtx);

    // 0x0043d721 / 0x0043d728 read the output's t[0] and t[2] (+0x14 / +0x1C).
    int dist_x = *(int*)&enemy->scaMatrixData.localMatrix.t[0] - knifeMtx.t[0];
    int dist_z = *(int*)&enemy->scaMatrixData.localMatrix.t[2] - knifeMtx.t[2];

    unsigned int effectiveRange = (unsigned int)*(short*)(*(int*)((char*)enemy + 4) + 10) + range;
    unsigned char enemyId = *(unsigned char*)((char*)enemy + 1);

    // Zombies — skip if player aiming down
    if ((enemyId == ENEMY_ZOMBIE || enemyId == ENEMY_ZOMBIE_NAKED || enemyId == ENEMY_ZOMBIE_VARIANT)
        && (g_playerEntityPointer.flags & 0x80) != 0)
        return 0;

    // Cerberus / Web Spinner — skip if player aiming up and enemy on ground
    if (((enemyId == ENEMY_CERBERUS && *(int*)&enemy->scaMatrixData.localMatrix.t[1] > -400)
         || enemyId == ENEMY_WEB_SPINNER)
        && (g_playerEntityPointer.flags & 0x40) != 0)
        return 0;

    // Crow / Bee / Chimera on ceiling
    if ((enemyId == ENEMY_CROW || enemyId == ENEMY_WASP || enemyId == ENEMY_CHIMERA)
        && *(int*)&enemy->scaMatrixData.localMatrix.t[1] < -4800)
        return 0;

    // Per-enemy range adjustments
    if (enemyId == ENEMY_BLACK_TIGER)                 effectiveRange -= 800;
    if (enemyId == ENEMY_PLANT42)                     effectiveRange += 2000;
    if (enemyId == ENEMY_WASP || enemyId == ENEMY_ADDER) effectiveRange += 100;

    unsigned int distance = SquareRoot0(dist_z * dist_z + dist_x * dist_x);
    if (distance < effectiveRange && distance < g_playerDisplacement) {
        g_playerDisplacement = distance;
        return 1;
    }
    return 0;
}

// ============================================================================
// weapon_hit_detect_gun @ 0x0043d410
// Gun hit detection for handgun, shotgun, magnum, and grenade launcher.
// Creates a triangular aim cone in front of the player. The cone height
// shifts up (offset 0 vs 50) when aiming up (player flags & 0x20).
// Two cone tiers: near (50 depth) and far (200 to range depth).
// Calls checkEntityInRangeCone to test if the enemy is inside the cone.
// ============================================================================
static unsigned char weapon_hit_detect_gun(short range, Entity* enemy)
{
    short enemyRadius = *(short*)(*(int*)((char*)enemy + 4) + 10);
    unsigned char enemyId = *(unsigned char*)((char*)enemy + 1);

    // Zombies — skip if aiming down and NOT shotgun (weapon index 2)
    if ((enemyId == ENEMY_ZOMBIE || enemyId == ENEMY_ZOMBIE_NAKED || enemyId == ENEMY_ZOMBIE_VARIANT)
        && (g_playerEntityPointer.flags & 0x80) != 0
        && g_scaled_down_dist != 2)
        return 0;

    // Per-enemy range adjustments
    if (enemyId == ENEMY_BLACK_TIGER)  range -= 1000;
    if (enemyId == ENEMY_PLANT42)      range += 2000;

    // Cone height: 50 normal, 0 when aiming up (headshot cone)
    g_svecScratch.x = 50;
    if ((g_playerEntityPointer.flags & 0x20) != 0)
        g_svecScratch.x = 0;

    // Near cone boundaries
    g_svecScratch.z = enemyRadius + 200;
    g_svecScratch.y = 0;
    SVECTOR nearLeft, nearRight;
    MovePlayerXZ(g_playerEntityPointer.directionAngle, &g_svecScratch, &nearLeft);

    g_svecScratch.z = -200 - enemyRadius;
    MovePlayerXZ(g_playerEntityPointer.directionAngle, &g_svecScratch, &nearRight);

    // Far cone boundaries
    g_svecScratch.x = 0x28A;
    g_svecScratch.z = enemyRadius + range;
    SVECTOR farLeft, farRight;
    MovePlayerXZ(g_playerEntityPointer.directionAngle, &g_svecScratch, &farLeft);

    g_svecScratch.z = -(enemyRadius + range);
    MovePlayerXZ(g_playerEntityPointer.directionAngle, &g_svecScratch, &farRight);

    g_svecScratch.z = 0;
    MovePlayerXZ(g_playerEntityPointer.directionAngle, &g_svecScratch, &g_svecScratch);

    // Check near cone tier
    if (checkEntityInRangeCone((short*)&nearLeft, (short*)&nearRight, enemy))
        return 1;

    // Check far cone tier
    if (checkEntityInRangeCone((short*)&farLeft, (short*)&farRight, enemy))
        return 1;

    return 0;
}

// ============================================================================
// weapon_hit_detect_projectile @ 0x0043d810
// Projectile hit detection for grenade launcher and heavy weapons.
// Simple Euclidean distance check with per-enemy range adjustments.
// ============================================================================
static unsigned char weapon_hit_detect_projectile(short range, Entity* enemy)
{
    // 0x0043d819 / 0x0043d824: the reference point is g_playerPosScratch
    // (0x00be11b0 / 0x00be11b8), NOT the player entity. That is the whole point
    // of this detector - the caller writes the PROJECTILE's position there and
    // the hit is measured from it:
    //
    //   effect_behavior_flamethrower  (0x0040ed30) sets it to the flame's x/z
    //   before apply_weapon_damage(6)
    //
    // Measuring from the player instead meant the flame's ~400-unit reach was
    // tested against the distance from Chris, so a zombie standing inside the
    // flame was always out of range and the flamethrower did nothing at all.
    int dist_x = *(int*)&enemy->scaMatrixData.localMatrix.t[0] - g_playerPosScratch.x;
    int dist_z = *(int*)&enemy->scaMatrixData.localMatrix.t[2] - g_playerPosScratch.z;

    // `XOR EDI,EDI; MOV DI, word [EAX+0xa]` - the radius is ZERO-extended.
    unsigned int effectiveRange =
        (unsigned int)*(unsigned short*)(*(int*)((char*)enemy + 4) + 10) + range;

    if (*(unsigned char*)((char*)enemy + 1) == ENEMY_BLACK_TIGER)
        effectiveRange -= 1000;

    unsigned int distance = SquareRoot0(dist_z * dist_z + dist_x * dist_x);
    if (distance < effectiveRange && distance < g_playerDisplacement) {
        g_playerDisplacement = distance;
        return 1;
    }
    return 0;
}

// ============================================================================
// apply_weapon_damage @ 0x0043c020
// Real-time weapon hit detection and damage application. Iterates all
// active enemies, calls per-weapon hit detection to find the closest
// target in range+FOV, then subtracts weapon damage from health and
// transitions the enemy to damaged (state 2) or dead (state 3).
// ============================================================================
unsigned char apply_weapon_damage(unsigned int weapon_id)
{
    unsigned char activeIdx[32];
    unsigned char enemyCount = g_enemy_count;

    if (enemyCount == 0) return 0;

    // First pass: collect indices of all active enemies (reverse order)
    {
        Entity* ent = g_EnemiesList;
        unsigned char slot = 0;
        unsigned char remaining = enemyCount;
        do {
            if (ent->status_flags != 0) {
                activeIdx[--remaining] = slot;
            }
            if (remaining == 0) break;
            ent = (Entity*)((char*)ent + sizeof(Entity));
            slot++;
        } while (slot < 30);
    }

    g_playerDisplacement = 0x7FFFFFFF;

    unsigned char weaponAdj = (unsigned char)(weapon_id - 1);
    g_scaled_down_dist = weaponAdj;  // set global so hit detection callbacks can read it
    unsigned int wpnRange = weapons_ranges[
        (unsigned int)weaponAdj + (unsigned int)(g_playerEntityPointer.id & 1) * 10];

    // DC ADVANCED: the PS1 promotes the Beretta (item 2) to slot 3 - the DC's
    // Beretta M92FS custom (item 4; in the USA build that slot is the unused
    // DumDum Python) - for this shot (~1/8 chance, table 0x8008C854): that
    // slot's hit detector, records and post-hit callback, plus a hit-state base
    // of 4 instead of the weapon id (SLUS_005.51 0x800120e8). The range above
    // was already read with the real weapon, matching the original's order.
    unsigned char hitStateBase = (unsigned char)weapon_id;
    if (g_bDcMode && (g_main_state_flags2 & MSF2_DC_ADVANCED) != 0 &&
        weaponAdj == 1 && ((DC_BERETTA_ROLL_MASK >> (rand() & 0xF)) & 1) != 0) {
        weaponAdj = 3;
        g_scaled_down_dist = weaponAdj;
        hitStateBase = 4;
    }

    Entity* enemy = NULL;

    // Second pass: check each active enemy for weapon hit
    unsigned char idx = enemyCount;
    while (idx != 0) {
        idx--;
        Entity* candidate = &g_EnemiesList[activeIdx[0]];

        if ((candidate->status_flags & g_playerEntityPointer.flags & 0xE0) != 0
            && candidate->hit_state == 0)
        {
            typedef unsigned char (*hitDetectFn)(short range, Entity* ent);
            hitDetectFn detector = (hitDetectFn)PTR_weapons_hit_detection_functions[weaponAdj];
            if (detector((short)wpnRange, candidate) != 0) {
                enemy = candidate;
            }
        }
        activeIdx[0] = activeIdx[idx];
    }

    if (enemy == NULL || (check_weapon_line_of_sight((VECTOR*)enemy->scaMatrixData.localMatrix.t) && weaponAdj < 5)) {
        return 0;
    }

    unsigned char enemyType = enemy->id;
    if (enemyType >= NPC_ENTITIES_IDS) return enemy->id;   // 0x14+ NPC ids: no damage (matches the original, returns the enemy id)

    unsigned int tableIdx = (unsigned int)weaponAdj + (unsigned int)enemyType * 10;

    // 12-byte hit record lookup. The knockback vector (kx/ky/kz) and the
    // post-hit inputs (type/data) come from the first-run records for BOTH
    // playthroughs - only damage and hit-state differ (verified against the
    // original disassembly of 0x0043c020).
    WeaponHitRecordFirstRun* rec = &g_weaponHitRecordsFirstRun[tableIdx];
    g_playerPosScratch.x = (int)rec->kx;                    // 0x00be11b0
    g_playerPosScratch.y = (int)rec->ky;
    g_playerPosScratch.z = (int)rec->kz;
    g_weaponHitEnemyType = enemyType;                       // 0x00be0de4
    g_collPushDepthZHi   = rec->type;                       // 0x00be0dec (shared scratch)
    g_collPushDepthZLo   = rec->data;                       // 0x00be0df0 (shared scratch)
    g_entity_bkp         = (unsigned int)(int)enemy->health;// 0x00be0df4 (shared scratch)

    unsigned char hitState;
    short damage;
    if (g_bDcMode) {
        // The DC reads the damage from one of three mode columns and has no
        // second-playthrough damage table; the knockback/type/data/hit fields
        // still come from the STANDARD records (SLUS_005.51 0x800120e8: the
        // 0x40000 bit -> 0x8008B59A, (bits & 0x30000) == 0x20000 ->
        // 0x8008BEFA, else the base column - so ADVANCED* reads the base
        // column, exactly as the difficulty byte does here).
        if (g_DcDifficulty == DC_DIFFICULTY_TRAINING) {
            damage = g_dcWeaponDamageTraining[tableIdx];
        } else if (g_DcDifficulty == DC_DIFFICULTY_ADVANCED) {
            damage = g_dcWeaponDamageAdvanced[tableIdx];
        } else {
            damage = rec->dmg;              // STANDARD and ADVANCED* share this column
        }
        hitState = rec->hit;
    } else if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_SECOND_PLAYTHROUGH) == 0) {
        hitState = rec->hit;                                // first-playthrough hit-state @ +10
        damage = rec->dmg;                                  // first-playthrough damage @ +6
    } else {
        hitState = g_weaponHitRecordsSecondRun[tableIdx].hit;  // second-playthrough hit-state @ +4
        damage = g_weaponHitRecordsSecondRun[tableIdx].dmg;    // second-playthrough damage @ +0
    }

    enemy->health -= damage;

    if ((g_playerEntityPointer.flags & 0xE0) != 0x20) {
        hitState += (g_playerEntityPointer.flags >> 5);
    }
    hitState |= (unsigned char)(hitStateBase << 3);
    enemy->hit_state = hitState;

    typedef void (*postHitFn)(Entity* ent);
    postHitFn postHit = (postHitFn)PTR_post_hit_callbacks[weaponAdj];
    postHit(enemy);

    enemy->state = 3;
    enemy->ignore_player_flag = 0;
    enemy->action_behavior = 0;
    enemy->action_state = 0;

    if (enemy->health >= 0) {
        enemy->state = 2;
        enemy->ignore_player_flag = 0;
        enemy->action_behavior = 0;
        enemy->action_state = 0;
    }

    return 1;
}

// ============================================================================
// Weapon damage data tables (ROM .data, dumped byte-for-byte from the exe)
// ============================================================================

// Weapon index used by every per-weapon table below (weaponAdj = weapon_id - 1):
//   0 knife          1 handgun         2 shotgun        3 python DumDum  4 python magnum
//   5 flamethrower   6 GL explosive    7 GL acid        8 GL flame       9 rocket launcher
// Slot 3 is item 4 (unobtainable in the USA; the DC's Beretta M92FS custom) and
// slot 4 is item 5 (the live Colt Python) - see the note on the hit-record table.

// 0x004bb530 - per-weapon hit detection callbacks (weaponAdj = weapon_id - 1)
void* PTR_weapons_hit_detection_functions[10] = {
    (void*)weapon_hit_detect_knife,      // [0] 0x0043d690 - knife
    (void*)weapon_hit_detect_gun,        // [1] 0x0043d410 - handgun
    (void*)weapon_hit_detect_gun,        // [2] shotgun
    (void*)weapon_hit_detect_gun,        // [3] python (DumDum rounds, item 4)
    (void*)weapon_hit_detect_gun,        // [4] python (magnum rounds, item 5)
    (void*)weapon_hit_detect_projectile, // [5] 0x0043d810 - flamethrower
    (void*)weapon_hit_detect_projectile, // [6] GL explosive rounds
    (void*)weapon_hit_detect_projectile, // [7] GL acid rounds
    (void*)weapon_hit_detect_projectile, // [8] GL flame rounds
    (void*)weapon_hit_detect_projectile, // [9] rocket launcher
};

// 0x004bb558 - post-hit callbacks (called with the hit enemy, no null check)
void* PTR_post_hit_callbacks[10] = {
    (void*)weapon_post_hit_knife,    // [0] 0x0043c290 - knife
    (void*)weapon_post_hit_reaction, // [1] 0x0043c350 - handgun
    (void*)weapon_post_hit_shotgun,  // [2] 0x0043c370 - shotgun
    (void*)weapon_post_hit_shotgun,  // [3] python (DumDum rounds, item 4)
    (void*)weapon_post_hit_shotgun,  // [4] python (magnum rounds, item 5)
    (void*)weapon_post_hit_blood,    // [5] 0x0043c3b0 - flamethrower
    (void*)weapon_post_hit_blood2,   // [6] 0x0043c770 - GL explosive rounds
    (void*)weapon_post_hit_sparks,   // [7] 0x0043ca30 - GL acid rounds
    (void*)weapon_post_hit_blood,    // [8] 0x0043c3b0 - GL flame rounds
    (void*)weapon_post_hit_blood3,   // [9] 0x0043cc90 - rocket launcher
};

// 0x004bb580 - per-enemy-type hit reactions (indexed by enemy->id, see
// EntityCommon.h for the ENEMY_* ids)
void* g_enemy_hit_reactions[20] = {
    (void*)enemy_hit_reaction_zombie,   // [0x00] ENEMY_ZOMBIE
    (void*)enemy_hit_reaction_zombie,   // [0x01] ENEMY_ZOMBIE_NAKED
    (void*)enemy_hit_reaction_zombie,   // [0x02] ENEMY_CERBERUS
    (void*)enemy_hit_reaction_basic,    // [0x03] ENEMY_WEB_SPINNER
    (void*)enemy_hit_reaction_basic,    // [0x04] ENEMY_BLACK_TIGER
    (void*)enemy_hit_reaction_basic,    // [0x05] ENEMY_CROW
    (void*)enemy_hit_reaction_head,     // [0x06] ENEMY_HUNTER
    (void*)enemy_hit_reaction_basic,    // [0x07] ENEMY_WASP
    (void*)enemy_hit_reaction_blood,    // [0x08] ENEMY_PLANT42
    (void*)enemy_hit_reaction_none,     // [0x09] ENEMY_CHIMERA
    (void*)enemy_hit_reaction_basic,    // [0x0A] ENEMY_ADDER
    (void*)enemy_hit_reaction_none,     // [0x0B] ENEMY_NEPTUNE
    (void*)enemy_hit_reaction_none,     // [0x0C] ENEMY_TYRANT_1
    (void*)enemy_hit_reaction_none,     // [0x0D] ENEMY_YAWN_1
    (void*)enemy_hit_reaction_none,     // [0x0E] ENEMY_PLANT42_ROOTS
    (void*)enemy_hit_reaction_none,     // [0x0F] ENEMY_MONSTER_PLANT
    (void*)enemy_hit_reaction_none,     // [0x10] ENEMY_TYRANT_2
    (void*)enemy_hit_reaction_zombie,   // [0x11] ENEMY_ZOMBIE_VARIANT
    (void*)enemy_hit_reaction_none,     // [0x12] ENEMY_YAWN_2
    (void*)enemy_hit_reaction_none,     // [0x13] ENEMY_SPIDER_WEB
};

// 0x004bb5d0 - per-enemy-type joint index lists for the blood-spurt effects
// (6 bytes each, read by weapon_post_hit_blood2; indexed by enemy->id)
unsigned char g_enemyHitJointLists[20][6] = {
    { 2, 3, 4, 5, 7, 8 },  // [0x00] ENEMY_ZOMBIE
    { 3, 4, 5, 6, 7, 8 },  // [0x01] ENEMY_ZOMBIE_NAKED
    { 2, 3, 4, 5, 6, 8 },  // [0x02] ENEMY_CERBERUS
    { 0, 0, 0, 0, 0, 0 },  // [0x03] ENEMY_WEB_SPINNER
    { 0, 0, 0, 0, 0, 0 },  // [0x04] ENEMY_BLACK_TIGER
    { 1, 2, 4, 5, 8, 9 },  // [0x05] ENEMY_CROW
    { 4, 5, 6, 7, 8, 9 },  // [0x06] ENEMY_HUNTER
    { 0, 0, 0, 0, 0, 0 },  // [0x07] ENEMY_WASP - none
    { 0, 0, 0, 0, 0, 0 },  // [0x08] ENEMY_PLANT42 - none
    { 8, 4, 5, 6, 9, 10 }, // [0x09] ENEMY_CHIMERA
    { 0, 0, 0, 0, 0, 0 },  // [0x0A] ENEMY_ADDER - none
    { 0, 0, 0, 0, 0, 0 },  // [0x0B] ENEMY_NEPTUNE - none
    { 0, 0, 0, 0, 0, 0 },  // [0x0C] ENEMY_TYRANT_1 - none
    { 0, 0, 0, 0, 0, 0 },  // [0x0D] ENEMY_YAWN_1 - none
    { 0, 0, 0, 0, 0, 0 },  // [0x0E] ENEMY_PLANT42_ROOTS
    { 0, 0, 0, 0, 0, 0 },  // [0x0F] ENEMY_MONSTER_PLANT
    { 0, 0, 0, 0, 0, 0 },  // [0x10] ENEMY_TYRANT_2 - none
    { 2, 4, 5, 6, 7, 8 },  // [0x11] ENEMY_ZOMBIE_VARIANT
    { 0, 0, 0, 0, 0, 0 },  // [0x12] ENEMY_YAWN_2 - none
    { 0, 0, 0, 0, 0, 0 },  // [0x13] ENEMY_SPIDER_WEB - none
};

// 0x004bb648 - per-weapon hit ranges in world units, DWORDs. Indexed
// weaponAdj + (player.id & 1) * 10, where weaponAdj = weapon_id - 1 and
// weapon_id is the equipped weapon's ITEM_ id, so the ten slots line up 1:1
// with ITEM_KNIFE (0x01) .. ITEM_ROCKET_LAUNCHER (0x0A). Confirmed by the two
// hardcoded call sites in EffectSystem.cpp: apply_weapon_damage(6) from
// effect_behavior_flamethrower and apply_weapon_damage(10) from the rocket
// behavior, which land on slots 5 and 9.
//
// First block of 10 is Chris (CHAR_CHRIS == 0), second is Jill. The mask is
// `& 1`, so Rebecca (CHAR_REBECCA == 3) reads Jill's block too.
//
// The value is never used raw - every detector adds the target's collision
// radius (hitbox +0x0A) on top, and several apply per-enemy fudges:
//   slots 0     weapon_hit_detect_knife      radial, measured from the knife
//                                            joint (0x0E), not from the player
//   slots 1-4   weapon_hit_detect_gun        depth of the FAR aim-cone tier in
//                                            front of the player; the near tier
//                                            is a fixed 200 and ignores this
//   slots 5-9   weapon_hit_detect_projectile radial, measured from
//                                            g_playerPosScratch - the caller
//                                            parks the PROJECTILE there first
//
// Only the knife and the handgun differ between the two characters, and Jill
// gets 100 units more reach on each; shotgun and below are shared.
unsigned int weapons_ranges[20] = {
    // ---- Chris (CHAR_CHRIS == 0) ----
     400,  // [0] ITEM_KNIFE             - combat knife
    1200,  // [1] ITEM_BERETTA           - handgun
    2600,  // [2] ITEM_SHOTGUN           - shotgun
     800,  // [3] ITEM_COLT_PYTHON_DUM   - Colt Python, DumDum rounds (item 4; see the note below)
     800,  // [4] ITEM_COLT_PYTHON_MAG   - Colt Python, magnum rounds (the live one, item 5)
     400,  // [5] ITEM_FLAMETHROWER      - flamethrower (flame sprite radius)
     900,  // [6] ITEM_BAZOOKA_EXPLOSIVE - grenade launcher, explosive rounds
     900,  // [7] ITEM_BAZOOKA_ACID      - grenade launcher, acid rounds
     900,  // [8] ITEM_BAZOOKA_FLAME     - grenade launcher, flame rounds
    1100,  // [9] ITEM_ROCKET_LAUNCHER   - rocket launcher
    // ---- Jill (CHAR_JILL == 1, and CHAR_REBECCA via `& 1`) ----
     500,  // [0] ITEM_KNIFE             - combat knife      (+100 vs Chris)
    1300,  // [1] ITEM_BERETTA           - handgun           (+100 vs Chris)
    2600,  // [2] ITEM_SHOTGUN           - shotgun
     800,  // [3] ITEM_COLT_PYTHON_DUM   - Colt Python, DumDum rounds (item 4; see the note below)
     800,  // [4] ITEM_COLT_PYTHON_MAG   - Colt Python, magnum rounds (the live one, item 5)
     400,  // [5] ITEM_FLAMETHROWER      - flamethrower (flame sprite radius)
     900,  // [6] ITEM_BAZOOKA_EXPLOSIVE - grenade launcher, explosive rounds
     900,  // [7] ITEM_BAZOOKA_ACID      - grenade launcher, acid rounds
     900,  // [8] ITEM_BAZOOKA_FLAME     - grenade launcher, flame rounds
    1100,  // [9] ITEM_ROCKET_LAUNCHER   - rocket launcher
};

// 0x004bb698 - first-playthrough hit records (WeaponHitRecordFirstRun = { kx, ky, kz,
// dmg, type, data, hit, pad }, see top of file), 10 records per enemy,
// indexed (weaponAdj + enemyType * 10). One record per weapon slot, same
// order as weapons_ranges: knife, handgun, shotgun, python (DumDum),
// python (magnum), flamethrower, GL explosive, GL acid, GL flame, rocket launcher.
//
// The two Python slots, by item id:
//   slot 3 = item 4 = ITEM_COLT_PYTHON_DUM - Colt Python taking DumDum rounds.
//            Unobtainable in the USA builds. The Director's Cut gives item 4 to
//            its Beretta M92FS custom, so there the slot-3 records (and the
//            generated per-mode damage columns) are that Beretta's - which is
//            also the row the ADVANCED Beretta promotion below switches to.
//   slot 4 = item 5 = ITEM_COLT_PYTHON_MAG - the live Colt Python, magnum
//            rounds. Plain "magnum" in this file means this slot.
// The ten slots line up 1:1 with the ITEM_ ids 0x01..0x0A (weapon_id - 1).
WeaponHitRecordFirstRun g_weaponHitRecordsFirstRun[200] = {
    // ---- 0x00 ENEMY_ZOMBIE ----
//  {    kx,    ky,  kz, dmg, type, data, hit, pad }
    {   100,  -1800,   0,    8,   0,   0,   1, 0 },  // knife
    {   100,  -2620,   0,    9,   0,   1,   1, 0 },  // handgun
    {   100,  -2500,   0,   53,   4,   0,   2, 0 },  // shotgun
    {   100,  -2620,   0,   50,   4,   0,   2, 0 },  // python (DumDum)
    {   100,  -2620,   0,  130,   4,   0,   2, 0 },  // python (magnum)
    {   150,  -1500,   0,   20,  14,   6,   1, 0 },  // flamethrower
    {   150,  -1620,   0,  201,   0,   0,   2, 0 },  // GL explosive
    {   150,  -1520,   0,   95,   9,   0,   1, 0 },  // GL acid
    {   150,  -1500,   0,   95,  14,   6,   1, 0 },  // GL flame
    {   150,  -1620,   0,  900,   0,   6,   2, 0 },  // rocket launcher
    // ---- 0x01 ENEMY_ZOMBIE_NAKED ----
    {   100,  -1800,   0,    8,   0,   0,   1, 0 },  // knife
    {   100,  -2620,   0,    9,   0,   1,   1, 0 },  // handgun
    {   100,  -2500,   0,   53,   4,   0,   2, 0 },  // shotgun
    {   100,  -2620,   0,   50,   4,   0,   2, 0 },  // python (DumDum)
    {   100,  -2620,   0,  130,   4,   0,   2, 0 },  // python (magnum)
    {   150,  -1500,   0,   20,  14,   6,   1, 0 },  // flamethrower
    {   150,  -1620,   0,  201,   0,   0,   2, 0 },  // GL explosive
    {   150,  -1520,   0,   95,   9,   0,   1, 0 },  // GL acid
    {   150,  -1500,   0,   95,  14,   6,   1, 0 },  // GL flame
    {   150,  -1620,   0,  900,   0,   6,   2, 0 },  // rocket launcher
    // ---- 0x02 ENEMY_CERBERUS ----
    {   100,  -1200,   0,   30,   0,   0,   1, 0 },  // knife
    {   100,  -1200,   0,   20,   0,   1,   1, 0 },  // handgun
    {   100,  -1200,   0,   40,   4,   0,   2, 0 },  // shotgun
    {   100,  -1200,   0,   60,   4,   0,   2, 0 },  // python (DumDum)
    {   100,  -1200,   0,  130,   4,   0,   2, 0 },  // python (magnum)
    {   100,      0,   0,   20,  14,   6,   1, 0 },  // flamethrower
    {   100,   -100,   0,  200,   0,   0,   2, 0 },  // GL explosive
    {   100,      0,   0,  100,   9,   0,   1, 0 },  // GL acid
    {   100,      0,   0,  100,  14,   6,   1, 0 },  // GL flame
    {   100,   -100,   0,  900,   0,   6,   2, 0 },  // rocket launcher
    // ---- 0x03 ENEMY_WEB_SPINNER ----
    {     0,  -1200,   0,   10,   0,   8,   1, 0 },  // knife
    {     0,  -1220,   0,   20,   0,   9,   1, 0 },  // handgun
    {     0,  -1100,   0,   40,   0,   8,   2, 0 },  // shotgun
    {     0,  -1220,   0,   40,   0,   8,   2, 0 },  // python (DumDum)
    {     0,  -1220,   0,  130,   0,   8,   2, 0 },  // python (magnum)
    {     0,  -1100,   0,   20,  14,   7,   1, 0 },  // flamethrower
    {     0,  -1120,   0,  100,   0,   0,   2, 0 },  // GL explosive
    {     0,  -1120,   0,  100,   9,   0,   1, 0 },  // GL acid
    {     0,  -1100,   0,  200,  14,   7,   1, 0 },  // GL flame
    {     0,  -1120,   0,  900,   0,   7,   2, 0 },  // rocket launcher
    // ---- 0x04 ENEMY_BLACK_TIGER ----
    {     0,   -900,   0,   10,   0,   8,   1, 0 },  // knife
    {     0,   -920,   0,   14,   0,   9,   1, 0 },  // handgun
    {     0,   -800,   0,   40,   0,   8,   2, 0 },  // shotgun
    {     0,   -920,   0,   50,   0,   8,   2, 0 },  // python (DumDum)
    {     0,   -920,   0,   70,   0,   8,   2, 0 },  // python (magnum)
    {     0,   -800,   0,   20,  14,   7,   1, 0 },  // flamethrower
    {     0,   -820,   0,   60,   0,   0,   2, 0 },  // GL explosive
    {     0,   -820,   0,   60,   9,   0,   1, 0 },  // GL acid
    {     0,   -800,   0,  205,  14,   7,   1, 0 },  // GL flame
    {     0,   -820,   0,  900,   0,   7,   2, 0 },  // rocket launcher
    // ---- 0x05 ENEMY_CROW ----
    {     0,      0,   0,   50,   0,   0,   1, 0 },  // knife
    {     0,      0,   0,   26,   0,   0,   1, 0 },  // handgun
    {     0,      0,   0,   50,   0,   0,   2, 0 },  // shotgun
    {     0,      0,   0,   50,   0,   0,   2, 0 },  // python (DumDum)
    {     0,      0,   0,  130,   0,   0,   2, 0 },  // python (magnum)
    {     0,      0,   0,   20,  14,   5,   1, 0 },  // flamethrower
    {     0,      0,   0,  200,   0,   0,   2, 0 },  // GL explosive
    {     0,      0,   0,   60,   9,   0,   1, 0 },  // GL acid
    {     0,      0,   0,   60,  14,   5,   1, 0 },  // GL flame
    {     0,      0,   0,  900,   0,   5,   2, 0 },  // rocket launcher
    // ---- 0x06 ENEMY_HUNTER ----
    {     0,  -1500,   0,   16,   9,   6,   1, 0 },  // knife
    {     0,  -1500,   0,   14,   9,   6,   1, 0 },  // handgun
    {     0,  -1500,   0,   32,   9,   6,   2, 0 },  // shotgun
    {     0,  -1500,   0,   40,   9,   6,   2, 0 },  // python (DumDum)
    {     0,  -1500,   0,  130,   9,   6,   2, 0 },  // python (magnum)
    {   150,  -1500,   0,   20,  14,   6,   1, 0 },  // flamethrower
    {   150,  -1500,   0,  100,   0,   0,   2, 0 },  // GL explosive
    {   150,  -1500,   0,  200,   9,   0,   1, 0 },  // GL acid
    {   150,  -1500,   0,  100,  14,   6,   1, 0 },  // GL flame
    {   150,  -1500,   0,  900,   0,   6,   2, 0 },  // rocket launcher
    // ---- 0x07 ENEMY_WASP ----
    {     0,      0,   0,   20,   0,  16,   1, 0 },  // knife
    {     0,      0,   0,   30,   0,  17,   1, 0 },  // handgun
    {     0,      0,   0,   60,   0,  16,   2, 0 },  // shotgun
    {     0,      0,   0,   70,   0,  16,   2, 0 },  // python (DumDum)
    {     0,      0,   0,  130,   0,  16,   2, 0 },  // python (magnum)
    {     0,      0,   0,   20,  14,   4,   1, 0 },  // flamethrower
    {     0,      0,   0,  200,   0,   0,   2, 0 },  // GL explosive
    {     0,      0,   0,   80,   9,   0,   1, 0 },  // GL acid
    {     0,      0,   0,   80,  14,   4,   1, 0 },  // GL flame
    {     0,      0,   0,  900,   0,   4,   2, 0 },  // rocket launcher
    // ---- 0x08 ENEMY_PLANT42 ----
    {     0,      0,   0,   15,   1,   0,   1, 0 },  // knife
    {     0,   1000,   0,   15,   0,   0,   1, 0 },  // handgun
    {     0,   1500,   0,   20,   0,   0,   2, 0 },  // shotgun
    {     0,   1000,   0,   38,   0,   0,   2, 0 },  // python (DumDum)
    {     0,   1000,   0,   74,   0,   0,   2, 0 },  // python (magnum)
    {     0,   1500,   0,   20,  14,   6,   1, 0 },  // flamethrower
    {     0,   1500,   0,   50,   2,   0,   2, 0 },  // GL explosive
    {     0,   1500,   0,   40,   9,   0,   1, 0 },  // GL acid
    {     0,   1500,   0,  150,  14,   6,   1, 0 },  // GL flame
    {     0,   1500,   0,  900,   2,   6,   2, 0 },  // rocket launcher
    // ---- 0x09 ENEMY_CHIMERA ----
    {     0,      0,   0,   17,   0,   0,   1, 0 },  // knife
    {     0,      0,   0,   20,   1,   0,   1, 0 },  // handgun
    {     0,      0,   0,   30,   1,   0,   2, 0 },  // shotgun
    {     0,      0,   0,   40,   1,   0,   2, 0 },  // python (DumDum)
    {     0,      0,   0,  130,   1,   0,   2, 0 },  // python (magnum)
    {   150,  -1500,   0,   20,  14,   6,   1, 0 },  // flamethrower
    {   150,  -1500,   0,  200,   0,   0,   2, 0 },  // GL explosive
    {   150,  -1500,   0,   60,   9,   0,   1, 0 },  // GL acid
    {   150,  -1500,   0,   60,  14,   6,   1, 0 },  // GL flame
    {   150,  -1500,   0,  900,   0,   6,   2, 0 },  // rocket launcher
    // ---- 0x0A ENEMY_ADDER ----
    {     0,      0,   0,   20,   0,   0,   1, 0 },  // knife
    {     0,      0,   0,   20,   0,   0,   1, 0 },  // handgun
    {     0,      0,   0,   40,   0,   0,   2, 0 },  // shotgun
    {     0,      0,   0,   50,   0,   0,   2, 0 },  // python (DumDum)
    {     0,      0,   0,  130,   0,   0,   2, 0 },  // python (magnum)
    {     0,      0,   0,   30,  14,   3,   1, 0 },  // flamethrower
    {     0,      0,   0,  200,   0,   0,   2, 0 },  // GL explosive
    {     0,      0,   0,   60,   9,   0,   1, 0 },  // GL acid
    {     0,      0,   0,   60,  14,   3,   1, 0 },  // GL flame
    {     0,      0,   0,  900,   0,   3,   2, 0 },  // rocket launcher
    // ---- 0x0B ENEMY_NEPTUNE ----
    {     0,      0,   0,    0,   1,   0,   1, 0 },  // knife (no damage)
    {     0,      0,   0,    0,   1,   0,   1, 0 },  // handgun (no damage)
    {     0,      0,   0,    0,   1,   0,   1, 0 },  // shotgun (no damage)
    {     0,      0,   0,    0,   1,   0,   1, 0 },  // python (DumDum, no damage)
    {     0,      0,   0,    0,   1,   0,   1, 0 },  // python (magnum, no damage)
    {     0,  -1500,   0,    0,  14,   7,   1, 0 },  // flamethrower (no damage)
    {     0,  -1500,   0,    0,   0,   0,   2, 0 },  // GL explosive (no damage)
    {     0,  -1500,   0,    0,   9,   0,   1, 0 },  // GL acid (no damage)
    {     0,  -1500,   0,    0,  14,   7,   1, 0 },  // GL flame (no damage)
    {     0,  -1500,   0,    0,   0,   7,   2, 0 },  // rocket launcher (no damage)
    // ---- 0x0C ENEMY_TYRANT_1 ----
    {     0,      0,   0,   10,   0,   0,   1, 0 },  // knife
    {     0,      0,   0,   20,   1,   0,   1, 0 },  // handgun
    {     0,      0,   0,   30,   1,   0,   2, 0 },  // shotgun
    {     0,      0,   0,   50,   1,   0,   2, 0 },  // python (DumDum)
    {     0,      0,   0,   80,   1,   0,   2, 0 },  // python (magnum)
    {   150,  -2000,   0,   20,   2,   6,   1, 0 },  // flamethrower
    {   150,  -2000,   0,  100,   2,   0,   2, 0 },  // GL explosive
    {   150,  -2000,   0,  100,   2,   0,   1, 0 },  // GL acid
    {   150,  -2000,   0,  100,   2,   6,   1, 0 },  // GL flame
    {   150,  -2000,   0,  900,   2,   6,   2, 0 },  // rocket launcher
    // ---- 0x0D ENEMY_YAWN_1 ----
    {     0,      0,   0,   20,   0,   8,   1, 0 },  // knife
    {     0,      0,   0,   30,   1,   0,   1, 0 },  // handgun
    {     0,      0,   0,   40,   1,   0,   2, 0 },  // shotgun
    {     0,      0,   0,   40,   1,   0,   2, 0 },  // python (DumDum)
    {     0,      0,   0,   80,   1,   0,   2, 0 },  // python (magnum)
    {     0,      0,   0,   20,   2,   7,   1, 0 },  // flamethrower
    {     0,      0,   0,   80,   2,   0,   2, 0 },  // GL explosive
    {     0,      0,   0,  130,   2,   0,   1, 0 },  // GL acid
    {     0,      0,   0,   50,   2,   7,   1, 0 },  // GL flame
    {     0,      0,   0,  900,   2,   7,   2, 0 },  // rocket launcher
    // ---- 0x0E ENEMY_PLANT42_ROOTS ----
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // knife (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // handgun (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // shotgun (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // python (DumDum, no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // python (magnum, no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // flamethrower (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // GL explosive (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // GL acid (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // GL flame (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // rocket launcher (no damage)
    // ---- 0x0F ENEMY_MONSTER_PLANT ----
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // knife (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // handgun (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // shotgun (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // python (DumDum, no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // python (magnum, no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // flamethrower (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // GL explosive (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // GL acid (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // GL flame (no damage)
    {     0,      0,   0,    0,   1,   0,   0, 0 },  // rocket launcher (no damage)
    // ---- 0x10 ENEMY_TYRANT_2 ----
    {     0,      0,   0,   10,   0,   0,   1, 0 },  // knife
    {     0,      0,   0,   20,   1,   0,   1, 0 },  // handgun
    {     0,      0,   0,   30,   1,   0,   2, 0 },  // shotgun
    {     0,      0,   0,   50,   1,   0,   2, 0 },  // python (DumDum)
    {     0,      0,   0,   80,   1,   0,   2, 0 },  // python (magnum)
    {   150,  -2000,   0,   20,   2,   6,   1, 0 },  // flamethrower
    {   150,  -2000,   0,  100,   2,   0,   2, 0 },  // GL explosive
    {   150,  -2000,   0,  100,   2,   0,   1, 0 },  // GL acid
    {   150,  -2000,   0,  100,   2,   6,   1, 0 },  // GL flame
    {   150,  -2000,   0,  900,   2,   6,   2, 0 },  // rocket launcher
    // ---- 0x11 ENEMY_ZOMBIE_VARIANT ----
    {   100,  -1800,   0,    8,   0,   0,   1, 0 },  // knife
    {   100,  -2620,   0,    9,   0,   1,   1, 0 },  // handgun
    {   100,  -1500,   0,   53,   4,   0,   2, 0 },  // shotgun
    {   100,  -2620,   0,   50,   4,   0,   2, 0 },  // python (DumDum)
    {   100,  -2620,   0,  130,   4,   0,   2, 0 },  // python (magnum)
    {   150,  -2500,   0,   20,  14,   6,   1, 0 },  // flamethrower
    {   150,  -1620,   0,  201,   0,   0,   2, 0 },  // GL explosive
    {   150,  -1520,   0,   95,   9,   0,   1, 0 },  // GL acid
    {   150,  -1500,   0,   95,  14,   6,   1, 0 },  // GL flame
    {   150,  -1620,   0,  900,   0,   6,   2, 0 },  // rocket launcher
    // ---- 0x12 ENEMY_YAWN_2 ----
    {     0,      0,   0,   20,   0,   8,   1, 0 },  // knife
    {     0,      0,   0,   30,   1,   0,   1, 0 },  // handgun
    {     0,      0,   0,   40,   1,   0,   2, 0 },  // shotgun
    {     0,      0,   0,   40,   1,   0,   2, 0 },  // python (DumDum)
    {     0,      0,   0,   80,   1,   0,   2, 0 },  // python (magnum)
    {     0,      0,   0,   20,   2,   7,   1, 0 },  // flamethrower
    {     0,      0,   0,   80,   2,   0,   2, 0 },  // GL explosive
    {     0,      0,   0,  130,   2,   0,   1, 0 },  // GL acid
    {     0,      0,   0,   50,   2,   7,   1, 0 },  // GL flame
    {     0,      0,   0,  900,   2,   7,   2, 0 },  // rocket launcher
    // ---- 0x13 ENEMY_SPIDER_WEB ----
    {     0,      0,   0,   10,   1,   0,   1, 0 },  // knife (burns it)
    {     0,      0,   0,    0,   1,   0,   1, 0 },  // handgun (no damage)
    {     0,      0,   0,    0,   1,   0,   2, 0 },  // shotgun (no damage)
    {     0,      0,   0,    0,   1,   0,   2, 0 },  // python (DumDum, no damage)
    {     0,      0,   0,    0,   1,   0,   2, 0 },  // python (magnum, no damage)
    {     0,      0,   0,    2,   2,   7,   1, 0 },  // flamethrower (burns it)
    {     0,      0,   0,   30,   2,   0,   2, 0 },  // GL explosive (burns it)
    {     0,      0,   0,   30,   2,   0,   1, 0 },  // GL acid (burns it)
    {     0,      0,   0,   30,   2,   7,   1, 0 },  // GL flame (burns it)
    {     0,      0,   0,  900,   2,   7,   2, 0 },  // rocket launcher (burns it)
};

// 0x004bbffe - second-playthrough ("western" difficulty) records (WeaponHitRecordSecondRun = { dmg,
// unk_02, hit, unk_05, kx, ky, kz }, see top of file), 10 records per enemy,
// indexed (weaponAdj + enemyType * 10). One record per weapon slot, same
// order as weapons_ranges: knife, handgun, shotgun, python (DumDum),
// python (magnum), flamethrower, GL explosive, GL acid, GL flame, rocket
// launcher - see the item-id note on the first-playthrough table above.
WeaponHitRecordSecondRun g_weaponHitRecordsSecondRun[200] = {
    // ---- 0x00 ENEMY_ZOMBIE ----
//  { dmg, unk_02, hit, unk_05,    kx,     ky,  kz }
    {    8,    0,   1, 0,   100,  -2620,   0 },  // knife
    {    9,  256,   1, 0,   100,  -2500,   0 },  // handgun
    {   20,    4,   2, 0,   100,  -2620,   0 },  // shotgun
    {   50,    4,   2, 0,   100,  -2620,   0 },  // python (DumDum)
    {   60,    4,   2, 0,   150,  -1500,   0 },  // python (magnum)
    {   20, 1550,   1, 0,   150,  -1620,   0 },  // flamethrower
    {  201,    0,   2, 0,   150,  -1520,   0 },  // GL explosive
    {   70,    9,   1, 0,   150,  -1500,   0 },  // GL acid
    {   70, 1550,   1, 0,   150,  -1620,   0 },  // GL flame
    {  900, 1536,   2, 0,   100,  -1800,   0 },  // rocket launcher
    // ---- 0x01 ENEMY_ZOMBIE_NAKED ----
    {    8,    0,   1, 0,   100,  -2620,   0 },  // knife
    {    9,  256,   1, 0,   100,  -2500,   0 },  // handgun
    {   20,    4,   2, 0,   100,  -2620,   0 },  // shotgun
    {   50,    4,   2, 0,   100,  -2620,   0 },  // python (DumDum)
    {   60,    4,   2, 0,   150,  -1500,   0 },  // python (magnum)
    {   20, 1550,   1, 0,   150,  -1620,   0 },  // flamethrower
    {  201,    0,   2, 0,   150,  -1520,   0 },  // GL explosive
    {   70,    9,   1, 0,   150,  -1500,   0 },  // GL acid
    {   70, 1550,   1, 0,   150,  -1620,   0 },  // GL flame
    {  900, 1536,   2, 0,   100,  -1200,   0 },  // rocket launcher
    // ---- 0x02 ENEMY_CERBERUS ----
    {   30,    0,   1, 0,   100,  -1200,   0 },  // knife
    {   20,  256,   1, 0,   100,  -1200,   0 },  // handgun
    {   35,    4,   2, 0,   100,  -1200,   0 },  // shotgun
    {   60,    4,   2, 0,   100,  -1200,   0 },  // python (DumDum)
    {   60,    4,   2, 0,   100,      0,   0 },  // python (magnum)
    {   20, 1550,   1, 0,   100,   -100,   0 },  // flamethrower
    {  200,    0,   2, 0,   100,      0,   0 },  // GL explosive
    {   90,    9,   1, 0,   100,      0,   0 },  // GL acid
    {   90, 1550,   1, 0,   100,   -100,   0 },  // GL flame
    {  900, 1536,   2, 0,     0,  -1200,   0 },  // rocket launcher
    // ---- 0x03 ENEMY_WEB_SPINNER ----
    {   10, 2048,   1, 0,     0,  -1220,   0 },  // knife
    {   15, 2304,   1, 0,     0,  -1100,   0 },  // handgun
    {   24, 2048,   2, 0,     0,  -1220,   0 },  // shotgun
    {   30, 2048,   2, 0,     0,  -1220,   0 },  // python (DumDum)
    {   40, 2048,   2, 0,     0,  -1100,   0 },  // python (magnum)
    {   20, 1806,   1, 0,     0,  -1120,   0 },  // flamethrower
    {  100,    0,   2, 0,     0,  -1120,   0 },  // GL explosive
    {  100,    9,   1, 0,     0,  -1100,   0 },  // GL acid
    {  200, 1806,   1, 0,     0,  -1120,   0 },  // GL flame
    {  900, 1792,   2, 0,     0,   -900,   0 },  // rocket launcher
    // ---- 0x04 ENEMY_BLACK_TIGER ----
    {   10, 2048,   1, 0,     0,   -920,   0 },  // knife
    {   12, 2304,   1, 0,     0,   -800,   0 },  // handgun
    {   20, 2048,   2, 0,     0,   -920,   0 },  // shotgun
    {   50, 2048,   2, 0,     0,   -920,   0 },  // python (DumDum)
    {   70, 2048,   2, 0,     0,   -800,   0 },  // python (magnum)
    {   20, 1806,   1, 0,     0,   -820,   0 },  // flamethrower
    {   50,    0,   2, 0,     0,   -820,   0 },  // GL explosive
    {   50,    9,   1, 0,     0,   -800,   0 },  // GL acid
    {  103, 1806,   1, 0,     0,   -820,   0 },  // GL flame
    {  900, 1792,   2, 0,     0,      0,   0 },  // rocket launcher
    // ---- 0x05 ENEMY_CROW ----
    {   50,    0,   1, 0,     0,      0,   0 },  // knife
    {   26,    0,   1, 0,     0,      0,   0 },  // handgun
    {   50,    0,   2, 0,     0,      0,   0 },  // shotgun
    {   50,    0,   2, 0,     0,      0,   0 },  // python (DumDum)
    {   50,    0,   2, 0,     0,      0,   0 },  // python (magnum)
    {   20, 1294,   1, 0,     0,      0,   0 },  // flamethrower
    {  200,    0,   2, 0,     0,      0,   0 },  // GL explosive
    {   60,    9,   1, 0,     0,      0,   0 },  // GL acid
    {   60, 1294,   1, 0,     0,      0,   0 },  // GL flame
    {  900, 1280,   2, 0,     0,  -1500,   0 },  // rocket launcher
    // ---- 0x06 ENEMY_HUNTER ----
    {   16, 1545,   1, 0,     0,  -1500,   0 },  // knife
    {   14, 1545,   1, 0,     0,  -1500,   0 },  // handgun
    {   25, 1545,   2, 0,     0,  -1500,   0 },  // shotgun
    {   40, 1545,   2, 0,     0,  -1500,   0 },  // python (DumDum)
    {   80, 1545,   2, 0,   150,  -1500,   0 },  // python (magnum)
    {   20, 1550,   1, 0,   150,  -1500,   0 },  // flamethrower
    {   50,    0,   2, 0,   150,  -1500,   0 },  // GL explosive
    {   80,    9,   1, 0,   150,  -1500,   0 },  // GL acid
    {   50, 1550,   1, 0,   150,  -1500,   0 },  // GL flame
    {  900, 1536,   2, 0,     0,      0,   0 },  // rocket launcher
    // ---- 0x07 ENEMY_WASP ----
    {   20, 4096,   1, 0,     0,      0,   0 },  // knife
    {   30, 4352,   1, 0,     0,      0,   0 },  // handgun
    {   60, 4096,   2, 0,     0,      0,   0 },  // shotgun
    {   70, 4096,   2, 0,     0,      0,   0 },  // python (DumDum)
    {   70, 4096,   2, 0,     0,      0,   0 },  // python (magnum)
    {   20, 1038,   1, 0,     0,      0,   0 },  // flamethrower
    {  200,    0,   2, 0,     0,      0,   0 },  // GL explosive
    {   80,    9,   1, 0,     0,      0,   0 },  // GL acid
    {   80, 1038,   1, 0,     0,      0,   0 },  // GL flame
    {  900, 1024,   2, 0,     0,      0,   0 },  // rocket launcher
    // ---- 0x08 ENEMY_PLANT42 ----
    {   15,    1,   1, 0,     0,   1000,   0 },  // knife
    {   10,    0,   1, 0,     0,   1500,   0 },  // handgun
    {   20,    0,   2, 0,     0,   1000,   0 },  // shotgun
    {   38,    0,   2, 0,     0,   1000,   0 },  // python (DumDum)
    {   20,    0,   2, 0,     0,   1500,   0 },  // python (magnum)
    {   20, 1550,   1, 0,     0,   1500,   0 },  // flamethrower
    {   40,    2,   2, 0,     0,   1500,   0 },  // GL explosive
    {   40,    9,   1, 0,     0,   1500,   0 },  // GL acid
    {  150, 1550,   1, 0,     0,   1500,   0 },  // GL flame
    {  900, 1538,   2, 0,     0,      0,   0 },  // rocket launcher
    // ---- 0x09 ENEMY_CHIMERA ----
    {   10,    0,   1, 0,     0,      0,   0 },  // knife
    {   12,    1,   1, 0,     0,      0,   0 },  // handgun
    {   20,    1,   2, 0,     0,      0,   0 },  // shotgun
    {   40,    1,   2, 0,     0,      0,   0 },  // python (DumDum)
    {   41,    1,   2, 0,   150,  -1500,   0 },  // python (magnum)
    {   20, 1550,   1, 0,   150,  -1500,   0 },  // flamethrower
    {   60,    0,   2, 0,   150,  -1500,   0 },  // GL explosive
    {   60,    9,   1, 0,   150,  -1500,   0 },  // GL acid
    {  100, 1550,   1, 0,   150,  -1500,   0 },  // GL flame
    {  900, 1536,   2, 0,     0,      0,   0 },  // rocket launcher
    // ---- 0x0A ENEMY_ADDER ----
    {   20,    0,   1, 0,     0,      0,   0 },  // knife
    {   20,    0,   1, 0,     0,      0,   0 },  // handgun
    {   40,    0,   2, 0,     0,      0,   0 },  // shotgun
    {   50,    0,   2, 0,     0,      0,   0 },  // python (DumDum)
    {   50,    0,   2, 0,     0,      0,   0 },  // python (magnum)
    {   30,  782,   1, 0,     0,      0,   0 },  // flamethrower
    {  200,    0,   2, 0,     0,      0,   0 },  // GL explosive
    {   60,    9,   1, 0,     0,      0,   0 },  // GL acid
    {   60,  782,   1, 0,     0,      0,   0 },  // GL flame
    {  900,  768,   2, 0,     0,      0,   0 },  // rocket launcher
    // ---- 0x0B ENEMY_NEPTUNE ----
    {    0,    1,   1, 0,     0,      0,   0 },  // knife (no damage)
    {    0,    1,   1, 0,     0,      0,   0 },  // handgun (no damage)
    {    0,    1,   1, 0,     0,      0,   0 },  // shotgun (no damage)
    {    0,    1,   1, 0,     0,      0,   0 },  // python (DumDum, no damage)
    {    0,    1,   1, 0,     0,  -1500,   0 },  // python (magnum, no damage)
    {    0, 1806,   1, 0,     0,  -1500,   0 },  // flamethrower (no damage)
    {    0,    0,   2, 0,     0,  -1500,   0 },  // GL explosive (no damage)
    {    0,    9,   1, 0,     0,  -1500,   0 },  // GL acid (no damage)
    {    0, 1806,   1, 0,     0,  -1500,   0 },  // GL flame (no damage)
    {    0, 1792,   2, 0,     0,      0,   0 },  // rocket launcher (no damage)
    // ---- 0x0C ENEMY_TYRANT_1 ----
    {   10,    0,   1, 0,     0,      0,   0 },  // knife
    {   15,    1,   1, 0,     0,      0,   0 },  // handgun
    {   20,    1,   2, 0,     0,      0,   0 },  // shotgun
    {   50,    1,   2, 0,     0,      0,   0 },  // python (DumDum)
    {   40,    1,   2, 0,   150,  -2000,   0 },  // python (magnum)
    {   20, 1538,   1, 0,   150,  -2000,   0 },  // flamethrower
    {   35,    2,   2, 0,   150,  -2000,   0 },  // GL explosive
    {   35,    2,   1, 0,   150,  -2000,   0 },  // GL acid
    {   35, 1538,   1, 0,   150,  -2000,   0 },  // GL flame
    {  900, 1538,   2, 0,     0,      0,   0 },  // rocket launcher
    // ---- 0x0D ENEMY_YAWN_1 ----
    {   15, 2048,   1, 0,     0,      0,   0 },  // knife
    {   18,    1,   1, 0,     0,      0,   0 },  // handgun
    {    5,    1,   2, 0,     0,      0,   0 },  // shotgun
    {   40,    1,   2, 0,     0,      0,   0 },  // python (DumDum)
    {   60,    1,   2, 0,     0,      0,   0 },  // python (magnum)
    {   20, 1794,   1, 0,     0,      0,   0 },  // flamethrower
    {   60,    2,   2, 0,     0,      0,   0 },  // GL explosive
    {  120,    2,   1, 0,     0,      0,   0 },  // GL acid
    {   60, 1794,   1, 0,     0,      0,   0 },  // GL flame
    {  900, 1794,   2, 0,     0,      0,   0 },  // rocket launcher
    // ---- 0x0E ENEMY_PLANT42_ROOTS ----
    {    0,    1,   0, 0,     0,      0,   0 },  // knife (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // handgun (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // shotgun (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // python (DumDum, no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // python (magnum, no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // flamethrower (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // GL explosive (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // GL acid (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // GL flame (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // rocket launcher (no damage)
    // ---- 0x0F ENEMY_MONSTER_PLANT ----
    {    0,    1,   0, 0,     0,      0,   0 },  // knife (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // handgun (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // shotgun (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // python (DumDum, no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // python (magnum, no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // flamethrower (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // GL explosive (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // GL acid (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // GL flame (no damage)
    {    0,    1,   0, 0,     0,      0,   0 },  // rocket launcher (no damage)
    // ---- 0x10 ENEMY_TYRANT_2 ----
    {   10,    0,   1, 0,     0,      0,   0 },  // knife
    {   15,    1,   1, 0,     0,      0,   0 },  // handgun
    {   20,    1,   2, 0,     0,      0,   0 },  // shotgun
    {   50,    1,   2, 0,     0,      0,   0 },  // python (DumDum)
    {   40,    1,   2, 0,   150,  -2000,   0 },  // python (magnum)
    {   20, 1538,   1, 0,   150,  -2000,   0 },  // flamethrower
    {   35,    2,   2, 0,   150,  -2000,   0 },  // GL explosive
    {   35,    2,   1, 0,   150,  -2000,   0 },  // GL acid
    {   35, 1538,   1, 0,   150,  -2000,   0 },  // GL flame
    {  900, 1538,   2, 0,   100,  -1800,   0 },  // rocket launcher
    // ---- 0x11 ENEMY_ZOMBIE_VARIANT ----
    {    8,    0,   1, 0,   100,  -2620,   0 },  // knife
    {    9,  256,   1, 0,   100,  -1500,   0 },  // handgun
    {   20,    4,   2, 0,   100,  -2620,   0 },  // shotgun
    {   50,    4,   2, 0,   100,  -2620,   0 },  // python (DumDum)
    {   60,    4,   2, 0,   150,  -2500,   0 },  // python (magnum)
    {   20, 1550,   1, 0,   150,  -1620,   0 },  // flamethrower
    {  201,    0,   2, 0,   150,  -1520,   0 },  // GL explosive
    {   70,    9,   1, 0,   150,  -1500,   0 },  // GL acid
    {   70, 1550,   1, 0,   150,  -1620,   0 },  // GL flame
    {  900, 1536,   2, 0,     0,      0,   0 },  // rocket launcher
    // ---- 0x12 ENEMY_YAWN_2 ----
    {   15, 2048,   1, 0,     0,      0,   0 },  // knife
    {   18,    1,   1, 0,     0,      0,   0 },  // handgun
    {   15,    1,   2, 0,     0,      0,   0 },  // shotgun
    {   40,    1,   2, 0,     0,      0,   0 },  // python (DumDum)
    {   60,    1,   2, 0,     0,      0,   0 },  // python (magnum)
    {   20, 1794,   1, 0,     0,      0,   0 },  // flamethrower
    {   60,    2,   2, 0,     0,      0,   0 },  // GL explosive
    {  120,    2,   1, 0,     0,      0,   0 },  // GL acid
    {   60, 1794,   1, 0,     0,      0,   0 },  // GL flame
    {  900, 1794,   2, 0,     0,      0,   0 },  // rocket launcher
    // ---- 0x13 ENEMY_SPIDER_WEB ----
    {    6,    1,   1, 0,     0,      0,   0 },  // knife (burns it)
    {    0,    1,   1, 0,     0,      0,   0 },  // handgun (no damage)
    {    0,    1,   2, 0,     0,      0,   0 },  // shotgun (no damage)
    {    0,    1,   2, 0,     0,      0,   0 },  // python (DumDum, no damage)
    {    0,    1,   2, 0,     0,      0,   0 },  // python (magnum, no damage)
    {    2, 1794,   1, 0,     0,      0,   0 },  // flamethrower (burns it)
    {   10,    2,   2, 0,     0,      0,   0 },  // GL explosive (burns it)
    {   20,    2,   1, 0,     0,      0,   0 },  // GL acid (burns it)
    {   30, 1794,   1, 0,     0,      0,   0 },  // GL flame (burns it)
    {  900, 1794,   2, 0,     0,    500, -800 },  // rocket launcher (burns it)
};

// ============================================================================
// MovePlayerXZ (0x0041b350)
// Rotate `offset` by a yaw angle about Y and write it to `out`.
//
// This was an empty stub with no address recorded, and it is the single reason
// door and item action zones never fired on approach: update_player_position
// builds its 600-unit reach probe by calling this with the SAME buffer as both
// input and output, so with the stub in place the probe came back unrotated as
// (600, 0, 0). Chris then had to be walked far enough that a due-east probe
// happened to land in the zone. The real address was found from the call site at
// 0x0041c087, not from any comment.
//
// The original builds a rotation SVECTOR with only .y set (.x and .z zeroed),
// hands it to RotMatrix, then routes the result through the g_playerPosScratch
// VECTOR before narrowing to shorts:
//
//   0041b358: MOV word ptr [ESP+0x2],CX      ; local.y = angle
//   0041b373: CALL 0x00409df0                ; RotMatrix(&local, &g_matrixScratch)
//   0041b38a: CALL 0x00409cd0                ; ApplyMatrix(&g_matrixScratch, offset, 0x00be11b0)
//   0041b396: MOV EDX,dword ptr [0x00be11b0] ; out->x = (short)result.x  (etc)
//
// Using g_playerPosScratch as the intermediate is faithful and safe: the caller
// overwrites it with the final probe immediately afterwards. The angle is read as
// a 16-bit value in the original (`MOV CX, word ptr [ESP+4]`) even though callers
// push a dword, so it is truncated here.
// ============================================================================
void MovePlayerXZ(int angle, SVECTOR* offset, SVECTOR* out)
{
    SVECTOR rot;
    rot.x = 0;
    rot.y = (short)angle;
    rot.z = 0;

    RotMatrix(&rot, &g_matrixScratch);
    ApplyMatrix(&g_matrixScratch, offset, &g_playerPosScratch);

    out->x = (short)g_playerPosScratch.x;
    out->y = (short)g_playerPosScratch.y;
    out->z = (short)g_playerPosScratch.z;
}

// ============================================================================
// Post-hit callbacks (0x004bb558) and per-enemy hit reactions (0x004bb580)
//
// apply_weapon_damage calls PTR_post_hit_callbacks[weaponAdj] with the hit
// enemy AFTER health/hit-state are set but BEFORE the enemy's state 3/2
// decision - so these can still change health (the shotgun callback adds
// health back; the zombie head-shot reaction kills outright). The reaction
// table is indexed by the enemy type snapshot in g_weaponHitEnemyType
// (0x00be0de4), and the per-weapon type/data bytes live in the shared
// 0x00be0dec / 0x00be0df0 scratch globals. ENTITY is saved/restored because
// joint_setup_attack_effect reads it for the costume size and weapon joint.
// ============================================================================

extern void Flg_on(int baseAddr, unsigned int bitIndex);                        // 0x00473ef0
extern void joint_setup_attack_effect(int joint, unsigned char effectType, unsigned short timer, unsigned short frameMatch);  // 0x0048a070

// Hit-billboard rotation: the angle between the enemy's facing and the
// player's facing, with the 0x800 offset the PSX angle convention uses.
static short hit_billboard_rot(Entity* enemy)
{
    return (short)(g_playerEntityPointer.directionAngle - enemy->angle + 0x800);
}

// Dispatch to the per-enemy-type reaction.
static void enemy_hit_reaction_dispatch(Entity* enemy)
{
    ((void(*)(Entity*))g_enemy_hit_reactions[g_weaponHitEnemyType])(enemy);
}

// 0x0043d400 - no reaction
static void enemy_hit_reaction_none(Entity* enemy) { }

// 0x0043d2c0 - single blood billboard
static void enemy_hit_reaction_basic(Entity* enemy)
{
    Effect_CreateBillboard((unsigned char)g_collPushDepthZHi,
                           (unsigned char)g_collPushDepthZLo, hit_billboard_rot(enemy),
                           &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
}

// 0x0043d300 - head blood: aim-up headshot lifts the blood to head height
static void enemy_hit_reaction_head(Entity* enemy)
{
    if ((g_playerEntityPointer.flags & 0x20) != 0
        && (g_playerPosScratch.y += 1000, -800 < ((JointStruct*)enemy->jointsStructs)[1].world.t[1])) {
        g_playerPosScratch.y = -300;
    }
    Effect_CreateBillboard(3, 8, hit_billboard_rot(enemy),
                           &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
    Effect_CreateBillboard((unsigned char)g_collPushDepthZHi,
                           (unsigned char)g_collPushDepthZLo, hit_billboard_rot(enemy),
                           &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
}

// 0x0043d3a0 - blood spray; long-range hits can restore health randomly
static void enemy_hit_reaction_blood(Entity* enemy)
{
    if (g_scaled_down_dist != 0) {
        if (7000 < g_playerDisplacement && (rand() & 1) != 0) {
            enemy->health = (short)g_entity_bkp;   // restore from the pre-shot snapshot
        }
        Effect_CreateBillboard(0, 0x18, hit_billboard_rot(enemy),
                               &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
    }
}

// 0x0043d060 - zombie reaction: shotgun point-blank / magnum hits blow the
// head off (instant kill + death event + head explosion effects)
static void enemy_hit_reaction_zombie(Entity* enemy)
{
    // The guaranteed head-explode: the shotgun inside 3000 units with the aim
    // off level, and weapon IDs 4 and 5 - the Colt Python, and in a DC
    // session the Beretta M92FS custom that shares slot 4 - on ANY level-aim
    // hit, whatever the enemy's health. This is the retail Python's signature
    // and it is why the DC's custom Beretta one-shots every standing zombie.
    //
    // The DC adds exactly one exception, and only in ADVANCED: stage 6 Mansion 
    // Kitchen is excluded (SLUS_005.51 0x80013938, testing g_status_flags 0x20000
    // and then the two bytes at 0x800c8660/61 against 6 and 0x1C - the same
    // g_stageId/g_roomId pair FUN_80043fb4, the arrange-file lookup, reads).
    // Everything else about the block is unchanged from the USA build.
    const int dcHeadExplodeSuppressed =
        g_bDcMode && (g_main_state_flags2 & MSF2_DC_ADVANCED) != 0
        && g_stageId == STAGE_MANSION_RETURN_2F && g_roomId == ROOM_MANSION_KITCHEN;

    if (g_weaponHitEnemyType != ENEMY_CERBERUS) {
        if ((((g_scaled_down_dist == 2 && g_playerDisplacement < 3000)
              && (g_playerEntityPointer.flags & 0xC0) != 0)
             || ((g_scaled_down_dist == 3 || g_scaled_down_dist == 4)
                 && (g_playerEntityPointer.flags & 0x40) != 0))
            && !dcHeadExplodeSuppressed) {
            enemy->health = -300;    // instant kill
            // 0x0043d0c5-0x0043d10c: the original swaps ENTITY to the hit enemy
            // for these three calls and restores it before the billboards.
            // joint_setup_attack_effect reads ENTITY->id for the effect size and
            // ENTITY->weaponJointsPtr - ENTITY->jointsStructs to reach the
            // weapon-part joint, and Snd_em reads ENTITY for the sound bank and
            // pan position. Left on the player, the sound came out of the
            // player's bank at the player's position and the weapon-joint write
            // landed at (playerWeaponJoints - playerJoints + zombieHead), i.e.
            // outside the zombie entirely - the wrong head-explosion effects.
            Entity* savedEntity = ENTITY;
            ENTITY = enemy;
            Flg_on((int)g_EnemiesFlags, enemy->death_event_id);
            joint_setup_attack_effect((int)((char*)enemy->jointsStructs + 0xf8), 30, 2, 3);
            Snd_em(6);                 // head-explosion sound
            ENTITY = savedEntity;
            g_playerPosScratch.x = 100;
            g_playerPosScratch.y = -600;
            g_playerPosScratch.z = 0;
            Effect_CreateBillboard(0, 3, 0, (void*)((char*)enemy->jointsStructs + 0x44),
                                   &g_playerPosScratch, 0);
            unsigned int idx = g_weaponHitEnemyType * 10 + g_scaled_down_dist;
            g_playerPosScratch.x = g_weaponHitRecordsFirstRun[idx].kx;
            g_playerPosScratch.y = g_weaponHitRecordsFirstRun[idx].ky - 0x78;
            g_playerPosScratch.z = g_weaponHitRecordsFirstRun[idx].kz;
            Effect_CreateBillboard(3, 0, hit_billboard_rot(enemy),
                                   &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
            g_playerPosScratch.y += 0x78;
        }
        if ((g_playerEntityPointer.flags & 0x20) != 0
            && (g_playerPosScratch.y = -600, (enemy->behavior_flags & 2) != 0)) {
            g_playerPosScratch.y = -500;
        }
    }

    if (g_bDcMode && (g_main_state_flags2 & MSF2_ATTRACT_DEMO) != 0) {
        enemy->health = (short)(enemy->health + 1);
    }

    // Jill + handgun: extra chip damage (cerberus takes a fixed -7)
    if ((g_playerEntityPointer.id & 1) != 0 && g_scaled_down_dist == 1) {
        short h = enemy->health;
        enemy->health = (short)(h - 3);
        if (g_weaponHitEnemyType == ENEMY_CERBERUS) {
            enemy->health = (short)(h - 7);
        }
    }

    if (g_collPushDepthZHi == 4) {
        if (g_playerDisplacement < 9000) {
            for (unsigned int i = 4; i != 0; i--) {
                Effect_CreateBillboard(4, (unsigned char)i,
                    (short)((i + 5) * 0x100 - enemy->angle + g_playerEntityPointer.directionAngle),
                    &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
            }
        }
        g_collPushDepthZHi = 0;
    }
    Effect_CreateBillboard((unsigned char)g_collPushDepthZHi,
                           (unsigned char)g_collPushDepthZLo, hit_billboard_rot(enemy),
                           &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
}

// 0x0043c290 - knife post-hit: stab sound, reaction, knife blood billboard
static void weapon_post_hit_knife(Entity* enemy)
{
    short offset[6] = { 153, 0, 0, -380, 0, 0 };   // per-character blood offset

    Play3DSnd(1, 1, 0, (int)&g_playerEntity.scaMatrixData.localMatrix.t);
    enemy_hit_reaction_dispatch(enemy);

    if (g_collPushDepthZHi != 1) {
        unsigned int pid = (unsigned int)(g_playerEntityPointer.id & 1);
        g_playerPosScratch.x = (int)offset[pid * 3];
        g_playerPosScratch.y = (int)offset[pid * 3 + 1];
        g_playerPosScratch.z = (int)offset[pid * 3 + 2];
        Effect_CreateBillboard((unsigned char)g_collPushDepthZHi,
                               (unsigned char)g_collPushDepthZLo, 0,
                               &g_playerEntityPointer.jointsStructs[0xe].world,
                               &g_playerPosScratch, 0);
    }
}

// 0x0043c350 - reaction only
static void weapon_post_hit_reaction(Entity* enemy)
{
    enemy_hit_reaction_dispatch(enemy);
}

// 0x0043c370 - reaction + long-range shotgun health chip
static void weapon_post_hit_shotgun(Entity* enemy)
{
    if (g_scaled_down_dist == 2 && 9000 < g_playerDisplacement) {
        enemy->health += 10;
        enemy->hit_state -= 1;   // +0x8A as a signed byte, per the original
    }
    enemy_hit_reaction_dispatch(enemy);
}

// 0x0043c3b0 - heavy blood FX: spurts + joint red tint
static void weapon_post_hit_blood(Entity* enemy)
{
    JointStruct* joints = enemy->jointsStructs;
    short rot = hit_billboard_rot(enemy);

    if (g_collPushDepthZHi != 1) {
        // Aim-up headshot: lift the blood to head height
        if (g_weaponHitRecordsFirstRun[g_weaponHitEnemyType * 10 + g_scaled_down_dist].kx == 0x96
            && (g_playerEntityPointer.flags & 0x20) != 0) {
            g_playerPosScratch.y += 1000;
            if (-1000 < joints[1].world.t[1]) {
                g_playerPosScratch.y = -300;
            }
        }
        Effect_CreateBillboard(0x0e, (unsigned char)g_collPushDepthZLo, rot,
                               &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0x09, 0x0d, rot,
                               &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        if (g_weaponHitEnemyType != ENEMY_WASP && g_weaponHitEnemyType != ENEMY_ADDER && g_weaponHitEnemyType != ENEMY_PLANT42) {
            g_playerPosScratch.y -= 500;
            Effect_CreateBillboard(0x0e, 0x03, rot,
                                   &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
            Effect_CreateBillboard(0x09, 0x0d, rot,
                                   &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        }
        if (enemy->health < 0 && g_collPushDepthZHi == 0x0e) {
            if (g_weaponHitEnemyType != ENEMY_CROW && g_weaponHitEnemyType != ENEMY_WASP && g_weaponHitEnemyType != ENEMY_ADDER) {
                g_playerPosScratch.y = 0;
                g_playerPosScratch.x = -100;
                g_playerPosScratch.z = -300;
                Effect_CreateBillboard(0x0e, 0x06, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                Effect_CreateBillboard(0x09, 0x0d, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                g_playerPosScratch.x = 100;
                g_playerPosScratch.z = 300;
                Effect_CreateBillboard(0x0e, 0x06, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                Effect_CreateBillboard(0x09, 0x0d, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                if (g_weaponHitRecordsFirstRun[g_weaponHitEnemyType * 10 + g_scaled_down_dist].kx == 0x96) {
                    g_playerPosScratch.y = -1000;
                    g_playerPosScratch.x = -100;
                    g_playerPosScratch.z = -300;
                    Effect_CreateBillboard(0x0e, 0x06, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                    Effect_CreateBillboard(0x09, 0x0d, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                    g_playerPosScratch.x = 100;
                    g_playerPosScratch.z = 300;
                    Effect_CreateBillboard(0x0e, 0x06, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                    Effect_CreateBillboard(0x09, 0x0d, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                }
            }
            // Red tint over every joint
            ENTITY = enemy;
            for (int i = enemy->jointCount; i != 0; i--) {
                JointApplyColorTint(joints + (i - 1), 0x202020, 0x101010, (void*)0x303030);
                if (g_weaponHitEnemyType == ENEMY_ZOMBIE) {
                    JointApplyColorTint(joints + (i - 1), 0x202020, 0x101010, (void*)0x0a0a0a);
                }
            }
            ENTITY = enemy;
            g_animFrameIdSave = 0xffffffff;   // the original ends the tint loop by writing -1 to the 0x00be0dfc scratch
        }
    }
}

// 0x0043c770 - blood FX with per-enemy-type joint spurts
static void weapon_post_hit_blood2(Entity* enemy)
{
    JointStruct* joints = enemy->jointsStructs;
    Entity* savedEntity = ENTITY;
    short rot = hit_billboard_rot(enemy);

    if (g_collPushDepthZHi != 1) {
        if (g_weaponHitRecordsFirstRun[g_weaponHitEnemyType * 10 + g_scaled_down_dist].kx == 0x96
            && (g_playerEntityPointer.flags & 0x20) != 0) {
            g_playerPosScratch.y += 1000;
            if (-1000 < joints[1].world.t[1]) {
                g_playerPosScratch.y = -300;
            }
        }
        Effect_CreateBillboard(0x0e, 7, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0x09, 0x0d, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0x09, 0x0d, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        if (g_weaponHitEnemyType != ENEMY_PLANT42) {
            g_playerPosScratch.y -= 200;
            for (int i = 2; i != 0; i--) {
                Effect_CreateBillboard(0x09, 0x0d, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
            }
            if (enemy->health < 0 && g_collPushDepthZHi == 0
                && g_enemyHitJointLists[g_weaponHitEnemyType][0] != 0) {
                g_playerPosScratch.y = -500;
                for (int i = 2; i != 0; i--) {
                    Effect_CreateBillboard(0x09, 0x0d, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                }
                if ((g_playerEntityPointer.flags & 0x20) == 0
                    || (g_weaponHitRecordsFirstRun[g_weaponHitEnemyType * 10 + g_scaled_down_dist].kx & 1)
                       * (enemy->behavior_flags & 2)) {
                    // spurts from the per-type joint list (6 joints)
                    ENTITY = enemy;
                    for (int i = 5; i >= 0; i--) {
                        joint_setup_attack_effect(
                            (int)((char*)joints + g_enemyHitJointLists[g_weaponHitEnemyType][i] * 0x7c),
                            0x1e, 2, 3);
                    }
                } else {
                    // spurts on the top 5 joints
                    ENTITY = enemy;
                    for (int i = enemy->jointCount - 1; (int)(enemy->jointCount - 5) <= i; i--) {
                        joint_setup_attack_effect((int)((char*)joints + i * 0x7c), 0x1e, 2, 3);
                    }
                }
            }
        }
    }
    ENTITY = savedEntity;
}

// 0x0043ca30 - spark FX + joint tint
static void weapon_post_hit_sparks(Entity* enemy)
{
    JointStruct* joints = enemy->jointsStructs;
    short rot = hit_billboard_rot(enemy);

    if (g_collPushDepthZHi != 1) {
        if (g_weaponHitRecordsFirstRun[g_weaponHitEnemyType * 10 + g_scaled_down_dist].kx == 0x96
            && (g_playerEntityPointer.flags & 0x20) != 0) {
            g_playerPosScratch.y += 1000;
            if (-1000 < joints[1].world.t[1]) {
                g_playerPosScratch.y = -300;
            }
        }
        for (int i = 2; i != 0; i--) {
            Effect_CreateBillboard(0x09, 0, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        }
        if (enemy->health < 0 && g_collPushDepthZHi == 9) {
            if (g_weaponHitEnemyType != ENEMY_CROW && g_weaponHitEnemyType != ENEMY_WASP
                && g_weaponHitEnemyType != ENEMY_ADDER && g_weaponHitEnemyType != ENEMY_PLANT42) {
                g_playerPosScratch.y = 0;
                g_playerPosScratch.x = -100;
                g_playerPosScratch.z = -300;
                Effect_CreateBillboard(0x09, 0, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                Effect_CreateBillboard(0x09, 1, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                g_playerPosScratch.x = 100;
                g_playerPosScratch.z = 300;
                Effect_CreateBillboard(0x09, 0, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                Effect_CreateBillboard(0x09, 1, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
            }
            ENTITY = enemy;
            for (int i = enemy->jointCount; i != 0; i--) {
                JointApplyColorTint(joints + (i - 1), 0x4040, 0x1010, (void*)0x3030);
                if (g_weaponHitEnemyType == ENEMY_ZOMBIE) {
                    JointApplyColorTint(joints + (i - 1), 0x2020, 0x1010, (void*)0x0a0a);
                }
            }
            ENTITY = enemy;
            g_animFrameIdSave = 0xffffffff;
        }
    }
}

// 0x0043cc90 - blood FX (health<0 && type != 2 variant) then the joint spurts
static void weapon_post_hit_blood3(Entity* enemy)
{
    JointStruct* joints = enemy->jointsStructs;
    short rot = hit_billboard_rot(enemy);

    if (g_collPushDepthZHi != 1) {
        if (g_weaponHitRecordsFirstRun[g_weaponHitEnemyType * 10 + g_scaled_down_dist].kx == 0x96
            && (g_playerEntityPointer.flags & 0x20) != 0) {
            g_playerPosScratch.y += 1000;
            if (-1000 < joints[1].world.t[1]) {
                g_playerPosScratch.y = -300;
            }
        }
        Effect_CreateBillboard(0x0e, (unsigned char)g_collPushDepthZLo, rot,
                               &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        Effect_CreateBillboard(0x09, 0x0d, rot,
                               &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        if (g_weaponHitEnemyType != ENEMY_WASP && g_weaponHitEnemyType != ENEMY_ADDER && g_weaponHitEnemyType != ENEMY_PLANT42) {
            g_playerPosScratch.y -= 500;
            Effect_CreateBillboard(0x0e, 0x03, rot,
                                   &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
            Effect_CreateBillboard(0x09, 0x0d, rot,
                                   &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
        }
        if (enemy->health < 0 && g_collPushDepthZHi != 2) {
            if (g_weaponHitEnemyType != ENEMY_CROW && g_weaponHitEnemyType != ENEMY_WASP && g_weaponHitEnemyType != ENEMY_ADDER) {
                g_playerPosScratch.y = 0;
                g_playerPosScratch.x = -100;
                g_playerPosScratch.z = -300;
                Effect_CreateBillboard(0x0e, 0x06, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                Effect_CreateBillboard(0x09, 0x0d, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                g_playerPosScratch.x = 100;
                g_playerPosScratch.z = 300;
                Effect_CreateBillboard(0x0e, 0x06, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                Effect_CreateBillboard(0x09, 0x0d, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                if (g_weaponHitRecordsFirstRun[g_weaponHitEnemyType * 10 + g_scaled_down_dist].kx == 0x96) {
                    g_playerPosScratch.y = -1000;
                    g_playerPosScratch.x = -100;
                    g_playerPosScratch.z = -300;
                    Effect_CreateBillboard(0x0e, 0x06, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                    Effect_CreateBillboard(0x09, 0x0d, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                    g_playerPosScratch.x = 100;
                    g_playerPosScratch.z = 300;
                    Effect_CreateBillboard(0x0e, 0x06, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                    Effect_CreateBillboard(0x09, 0x0d, rot, &enemy->scaMatrixData.localMatrix, &g_playerPosScratch, 0);
                }
            }
            ENTITY = enemy;
            for (int i = enemy->jointCount; i != 0; i--) {
                JointApplyColorTint(joints + (i - 1), 0x202020, 0x101010, (void*)0x303030);
                if (g_weaponHitEnemyType == ENEMY_ZOMBIE) {
                    JointApplyColorTint(joints + (i - 1), 0x202020, 0x101010, (void*)0x0a0a0a);
                }
            }
            ENTITY = enemy;
            g_animFrameIdSave = 0xffffffff;
        }
        weapon_post_hit_blood2(enemy);
    }
}
