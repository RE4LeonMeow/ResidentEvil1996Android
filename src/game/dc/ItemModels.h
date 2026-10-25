// ItemModels.h - Director's Cut item-view (ITEM_M2 *.IVM) names.
//
// menu_load_item_model (MainMenu.cpp) picks the item's view file with byte 0 of
// its g_ItemImageLookupTable record - the same number as its ITEM_ALL.PIX
// sprite row + 1 - through the 75-record g_ItemModelFileNames table. The DC
// repurposes item ids whose image types were unused or out of range in the USA
// build, so a few of those numbers either name a different view or run past the
// end of the USA table. Hand-written (not generated): the PS1 builds load
// ITEM_M2 by file index and carry no name table at all, so this mapping is
// derived from the DC's own art, not extracted. See docs/DC_PORT.md 3g.
#pragma once

// The DC's replacement for g_ItemModelFileNames[imageType], or NULL when the
// USA name applies. Callers must still bound-check: an image type past the end
// of the USA table has no name, and must not be read from the table.
const unsigned char* dc_item_model_name(int imageType);
