// EntityModels.cpp - Director's Cut entity model selection.
//
// The DC's model table (PS1 SLUS_005.51 0x8008d03c, 69 entries per block at
// stride 0x8a) against the port's g_emdPathTable (the OG's, 53 per block).
// Read off the table itself and cross-checked against the DC disc's ENEMY
// directory, whose alphabetical order is what the PS1's file-system indices
// count in:
//
//   index 26 -> 54 / 101    EM1016 / EM1116   Forest zombie
//   index 51 -> 70 / 71     EM1030 / EM1031   costume variant (port has these)
//   index 52 -> 72 / 73     EM1032 / EM1033   player, ADVANCED (port has these)
//   index 53 -> 74 / 74     EM1035            player 3, ADVANCED
//   index 56 -> 75          EM1040            NPC Chris, ADVANCED
//   index 57 -> 76          EM1041            NPC Jill, ADVANCED
//   index 59 -> 77          EM1043            NPC Rebecca, ADVANCED
//   index 68 -> 78          EM104C            NPC id 0x2C, ADVANCED
//
// Only the indices the port's table cannot express are overridden here. 51 and
// 52 are not among them: they land inside the port's block (52 = em1032,
// 52 + 53 = em1033), so the port's own table already names the DC's files.
// 53 and up would read the *next* character block out of the port's table, so
// every one the remap can produce is answered below.
#include "EntityModels.h"
#include "Items.h"       // DC_ITEM_BERETTA_CUSTOM

#include "../../Globals.h"
#include "../../DebugPrint.h"

#include <cstddef>

// Bit 17 of g_main_state_flags2 - the DC's ADVANCED flag, set by
// dc_apply_mode_flags() in GameStart.cpp (PS1 g_status_flags 0x20000).
static int dc_is_advanced(void)
{
    return (g_main_state_flags2 & MSF2_DC_ADVANCED) != 0;
}

unsigned char dc_emd_advanced_index(unsigned char modelIndex)
{
    if (!dc_is_advanced()) {
        return modelIndex;
    }

    // The original computes `modelIndex - 0x24` between the two player tests
    // and forces it to 0x11 on the id-3 arm, so the NPC test below cannot fire
    // on an index the player arms just produced. Kept in that order.
    unsigned char npcProbe;

    if (modelIndex < 2) {
        modelIndex = DC_EMD_INDEX_PLAYER_ADV;     // Chris / Jill -> EM1032 / EM1033
        npcProbe = DC_EMD_INDEX_PLAYER_ADV - 0x24;
    } else {
        npcProbe = (unsigned char)(modelIndex - 0x24);
    }

    if (modelIndex == 3) {
        modelIndex = DC_EMD_INDEX_PLAYER3_ADV;    // player model 3 -> EM1035
        npcProbe = 0x11;
    }

    // 0x24/0x25 = the cutscene Chris/Jill, 0x27 = Rebecca, 0x30 = NPC id 0x2C.
    if (npcProbe < 2 || modelIndex == 0x27 || modelIndex == 0x30) {
        modelIndex = (unsigned char)(modelIndex + 0x14);
    }

    // The costume-variant slot defers to the player's own model when mannequin
    // A's outfit is the one being worn - that is what makes OUTFIT_A the OG
    // outfit and OUTFIT_B (which leaves the index at DC_EMD_INDEX_COSTUME) the
    // OG alternate. See the flag comment in EntityModels.h.
    if (modelIndex == DC_EMD_INDEX_COSTUME
        && Flg_ck((int)g_ScenarioFlags, DC_SCENARIO_FLAG_OUTFIT_A) != 0) {
        modelIndex = (unsigned char)(g_playerEntity.id & 1);
    }

    return modelIndex;
}

const char* dc_emd_path(unsigned char modelIndex, unsigned char jillBlock)
{
    switch (modelIndex) {
    // Index 26 = entity id 0x16. The PC's table has the first em100a filler
    // there because no USA room ever spawns id 0x16.
    case DC_EMD_INDEX_FOREST_ZOMBIE:
        return jillBlock ? "enemy/em1116.emd" : "enemy/em1016.emd";
    // The five ADVANCED models past the port's block. All five are the same
    // file in both of the DC's character blocks.
    case DC_EMD_INDEX_PLAYER3_ADV:    return "enemy/em1035.emd";
    case DC_EMD_INDEX_NPC_CHRIS_ADV:  return "enemy/em1040.emd";
    case DC_EMD_INDEX_NPC_JILL_ADV:   return "enemy/em1041.emd";
    case DC_EMD_INDEX_NPC_REBECCA_ADV: return "enemy/em1043.emd";
    case DC_EMD_INDEX_NPC_2C_ADV:     return "enemy/em104c.emd";
    default:
        break;
    }

    // Anything else at or past the block width would read the other character
    // block's row out of the port's table. The remap cannot produce one, so
    // this is a "the data changed under us" report, not a code path.
    if (modelIndex >= 53) {
        dbg_printf("[dc] model index %u is past the %u-entry block and has no DC"
                   " override; falling back to the port's table\n",
                   (unsigned int)modelIndex, 53u);
    }
    return NULL;
}

void dc_emd_post_load(unsigned char modelIndex)
{
    if (!dc_is_advanced()) {
        return;
    }

    // `if (param_2 == 0x1a) *(byte *)(g_CurrentEntity + 1) = 0;` - entity +1 is
    // the id. So in ADVANCED the Forest zombie stops being id 0x16 the moment
    // his model is in memory: he dispatches through slot 0 and takes damage off
    // the zombie's row, where id 0x16 would be past the 20-row hit table and
    // therefore invulnerable (the DC keeps that `< 0x14` bound - 0x800120e8).
    // The arrange rooms spawn a second, inert id 0x16 alongside him; that one
    // reuses the loaded model, never re-enters this function, and so keeps its
    // id - which is exactly how the DC has it.
    if (modelIndex == DC_EMD_INDEX_FOREST_ZOMBIE) {
        ENTITY->id = 0;
    }
}

// ---------------------------------------------------------------------------
// The custom Beretta's in-hand model. See EntityModels.h for why W0F/W1F are
// its files; the short version is that the DC's two new weapon models are the
// plain Beretta's meshes at the same sizes as W02/W12, and item 4 is the only
// weapon the DC adds.
// ---------------------------------------------------------------------------
const char* dc_weapon_model_path(int charBlock, unsigned char weaponId)
{
    if (weaponId != DC_ITEM_BERETTA_CUSTOM) {
        return 0;
    }
    switch (charBlock) {
    case 0:  return "players/w0f.emw";   // Chris
    case 1:  return "players/w1f.emw";   // Jill
    default: return 0;                   // NPC blocks carry no such weapon
    }
}
