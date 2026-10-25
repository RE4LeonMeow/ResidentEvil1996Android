// ItemModels.cpp - Director's Cut item-view (ITEM_M2 *.IVM) names.
//
// The USA/PC build loads an item's view through g_ItemModelFileNames[imageType]
// (0x004bd348, 75 x 8-byte names), where imageType is byte 0 of the item's
// g_ItemImageLookupTable record. The DC's lookup gives a few items image types
// the USA build never uses, so the DC's own view files have to be named here:
//
//   0x2E / 0x2F  the two MOON CREST halves (items 0x31/0x32). The DC made those
//                slots obtainable, so they needed real art: I60V_L/I60V_R are
//                the left and right half of the crest - one shared TIM (the
//                crest's texture) and two TMDs, matching the assembled crest's
//                I60V at image type 0x29 (item 0x2C). The USA table names these
//                two image types i18v/i40v (a grey prop and the OIL can), which
//                the unreachable slots never showed.
//   0x4B         the Beretta M92FS custom (item 4), the ADVANCED starting
//                handgun. I00V_S1 is the DC's "custom edition" of the standard
//                Beretta's view (I00V, image type 2): same mesh, flat-shaded,
//                re-textured silver with the wooden grip. 0x4B is exactly one
//                past the USA table's last index (0x4A), so the USA build read
//                the "ING" string that follows the table in .rdata - which is
//                how item 4 rendered as an Ingram.
//
// The ITEM_M2 folder's other DC-only file, I99V.IVM (a second silver Beretta,
// dated with the February 1997 crest batch rather than with I00V_S1's April
// 1997), is *not* referenced: no DC item's image type resolves to it. It ships
// with the rest of the DC art in case a later pass finds its owner.
#include "ItemModels.h"

namespace {

struct DcItemModel {
    unsigned char imageType;    // g_ItemImageLookupTable[itemId * 4]
    char name[8];               // 8-byte record, as g_ItemModelFileNames
};

const DcItemModel s_dcItemModels[] = {
    { 0x2E, "i60v_l" },     // MOON CREST left half  (item 0x31)
    { 0x2F, "i60v_r" },     // MOON CREST right half (item 0x32)
    { 0x4B, "i00v_s1" },    // Beretta M92FS custom (item 0x04, ADVANCED)
};

const int s_dcItemModelCount = (int)(sizeof(s_dcItemModels) / sizeof(s_dcItemModels[0]));

} // namespace

const unsigned char* dc_item_model_name(int imageType)
{
    for (int i = 0; i < s_dcItemModelCount; i++) {
        if ((int)s_dcItemModels[i].imageType == imageType) {
            return (const unsigned char*)s_dcItemModels[i].name;
        }
    }
    return 0;
}
