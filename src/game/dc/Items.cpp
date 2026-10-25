// Items.cpp - see Items.h. Port-only: nothing here exists in the retail
// build, and every function returns the retail answer while g_bDcMode is off.
#include "../../Globals.h"
#include "../Types.h"
#include "Items.h"

unsigned char lockpick_item_id(void)
{
    return g_bDcMode ? (unsigned char)DC_ITEM_LOCKPICK : (unsigned char)ITEM_LOCK_PICK;
}

int is_lockpick_item(unsigned char itemId)
{
    return itemId == lockpick_item_id();
}

unsigned char weapon_ammo_item_id(unsigned char weaponId)
{
    // The custom Beretta takes the same 9mm clip as the standard one. Every
    // other weapon keeps weaponId + 9 - the DC's ids for those are unchanged,
    // so the derivation is safe there.
    if (g_bDcMode && weaponId == DC_ITEM_BERETTA_CUSTOM) {
        return (unsigned char)ITEM_CLIP;
    }
    return (unsigned char)(weaponId + 9);
}

unsigned char dc_item_pickup_quantity(unsigned char itemId, unsigned char quantity)
{
    // Items.h has the PS1 reference. 0x50000 matches both TRAINING and
    // ADVANCED*, exactly as the DC's own mask does; this is deliberate, not a
    // widened test - the original doubles in TRAINING too.
    if (!g_bDcMode ||
        (g_main_state_flags2 & (MSF2_DC_TRAINING | MSF2_DC_ADVANCED_HOLD)) == 0) {
        return quantity;
    }
    if (((ITEM_ROCKET_LAUNCHER < itemId) && (itemId < ITEM_EMPTY_BOTTLE)) ||
        (itemId == ITEM_INK_RIBBONS)) {
        return (unsigned char)(quantity << 1);
    }
    return quantity;
}

int dc_is_infinite_colt_python(unsigned char itemId)
{
    // The flag is DC-only (the DC's ENDING overlay raises it on an ADVANCED
    // best ending) and the port sets it only in DC mode, so the g_bDcMode gate
    // changes nothing for a DC session and keeps Mode=OG exactly as it was.
    return g_bDcMode
        && itemId == (unsigned char)ITEM_COLT_PYTHON_MAG
        && Flg_ck((int)g_ScenarioFlags, DC_SCENARIO_FLAG_INF_COLT_PYTHON) != 0;
}
