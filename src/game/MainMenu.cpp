// MainMenu.cpp - Main in-game menu (0x00463710)
// Decompiled from Ghidra with original addresses.
//
// The main menu is the in-game pause menu system. It handles:
//   - Status screen (character model, health bar, EKG)
//   - Inventory management (equip weapons, use items)
//   - Map screen (floor plan with room markers)
//   - Item box interaction (store/retrieve items)
//   - Options submenu access
//
// State machine: DAT_00ae9f11 controls the current submenu (0-8)
// Menu mode: DAT_00ae9f10 determines initial submenu on open
// ============================================================================
#include "../Globals.h"
#include "../platform/platform.h"
#include "../system/AssetPath.h"
#include "../DebugPrint.h"
#include "FileLoader.h"
#include "../marni/MarniSystem.h"
#include "../marni/PSXTexture.h"
#include "../marni/MarniBits.h"
#include "../marni/Marni3DObject.h"
#include "TmdRenderer.h"     // TMD slot regions - see the slot-region note there
#include "SpriteRenderer.h"  // g_SubpixelOffsetX/Y (item viewer OT quad records)
#include "MenuPresentation.h"
#include "dc/ItemModels.h"    // DC item-view file names (item_model_file_name)
#include "dc/Items.h"       // lockpick/ammo item ids the DC moved
#include <math.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================================
// Main menu state block (0x00ae9f08 - 0x00ae9f4b)
// These globals are used exclusively by the menu system.
// ============================================================================

// Inventory draw depth layer (Z-order for render sorting)
static unsigned short g_invDepthLayer;    // 0x00ae9f08

// Menu mode (0=status, 1=use item, 2=itembox, 3=desk locked, 4=desk take,
//            5=map, 6=item model viewer)
unsigned char  DAT_00ae9f10;       // 0x00ae9f10

// Menu state (0=init, 1=status/nav, 2=message wait, 3=itembox, 4=model view,
//             5=map anim, 6=model view2, 7=exit anim, 8=exit wait)
static char           g_MainMenuState;       // 0x00ae9f11

static unsigned char  DAT_00ae9f12;       // 0x00ae9f12
unsigned char  DAT_00ae9f13;       // 0x00ae9f13

// 0x00ae9f14
// Frame part data pointer (used by load_main_menu_frame_part_tex_area)
unsigned short* g_CurrentMenuFramesDataPtr;      

static char           DAT_00ae9f18;       // 0x00ae9f18

// Character type flags (bit 0 = chris/jill, bit 1 = rebecca/barry)
static unsigned char  DAT_00ae9f19;       // 0x00ae9f19

// Max inventory slots (6 for chris, 8 for jill)
unsigned char  g_totalInventorySlots;       // 0x00ae9f1a

// Selected item ID for 3D model display
unsigned char  g_bItemMenuSelectedItemId;       // 0x00ae9f1b

// Item model load direction (-1 or 1)
static char           DAT_00ae9f1c;       // 0x00ae9f1c

// Model loaded flag (0x80 = loaded, lower bits = image type)
static unsigned char  DAT_00ae9f1e;       // 0x00ae9f1e

// Map available flag (1 = player has map)
static unsigned char  DAT_00ae9f1f;       // 0x00ae9f1f

// Item box submenu state (0=init, 1=cursor, 2=item selected, 3=scrolling)
static unsigned char  DAT_00ae9f20;       // 0x00ae9f20

// Item action submenu state (0=init, 1=action confirm, 2=use/view confirm,
// 3=return, 4=item-mix message wait)
static unsigned char  DAT_00ae9f22;       // 0x00ae9f22

static unsigned char  DAT_00ae9f21;       // 0x00ae9f21

// Cursor grid position (0-7 for items, 0-4 for menu tabs)
unsigned char  DAT_00ae9f23;       // 0x00ae9f23

// Item box scroll position
static unsigned char  DAT_00ae9f24;       // 0x00ae9f24

// Item move: original cursor position
static unsigned char  DAT_00ae9f25;       // 0x00ae9f25

static unsigned char  DAT_00ae9f26;       // 0x00ae9f26
static unsigned char  DAT_00ae9f27;       // 0x00ae9f27
static unsigned char  DAT_00ae9f28;       // 0x00ae9f28

// Item move: slide animation counters
static unsigned char  DAT_00ae9f29;       // 0x00ae9f29
static unsigned char  DAT_00ae9f2a;       // 0x00ae9f2a

// Health bar animation state (0=init, 1=scrolling in, 2=init out, 3=scrolling out)
static unsigned char  DAT_00ae9f2e;       // 0x00ae9f2e

// Health status (0=danger, 1=caution, 2-3=good, 4=poisoned)
static unsigned char  DAT_00ae9f2f;       // 0x00ae9f2f

// EKG line colors (RGB)
static unsigned char  DAT_00ae9f30;       // 0x00ae9f30
static unsigned char  DAT_00ae9f31;       // 0x00ae9f31
static unsigned char  DAT_00ae9f32;       // 0x00ae9f32

// EKG line colors (secondary)
static unsigned char  DAT_00ae9f33;       // 0x00ae9f33
static unsigned char  DAT_00ae9f34;       // 0x00ae9f34
static unsigned char  DAT_00ae9f35;       // 0x00ae9f35

// EKG scroll positions
static short          DAT_00ae9f36;       // 0x00ae9f36
static short          DAT_00ae9f38;       // 0x00ae9f38
static short          DAT_00ae9f3a;       // 0x00ae9f3a
static short          DAT_00ae9f3c;       // 0x00ae9f3c

// EKG wave data pointers
static char*          DAT_00ae9f40;       // 0x00ae9f40
static char*          DAT_00ae9f44;       // 0x00ae9f44

// Health bar face animation index (bits 0-1 = face, bits 2-3 = health status)
static unsigned char  DAT_00ae9f48;       // 0x00ae9f48
unsigned char  DAT_00ae9f49;       // 0x00ae9f49
unsigned char  g_bItemViewerActionIndex;       // 0x00ae9f4a

// Item box submenu scroll position (0-15)
static unsigned char  SUBMENU_STATE_ID;   // 0x00ae9f1d

// Item model path buffer (for loading TIM files)
static char           DAT_008e1cb0[128];  // 0x008e1cb0

// External function stubs (pending decompilation)
extern void FUN_004844b0(void);
extern void FUN_0047d0e0(void);
extern void FUN_0040ac80(int idx, void* lightData);
extern void FUN_00484420(void* src, void* dst);
extern int draw_texture(TextureDesc* tex, unsigned short depth);
extern int   FUN_0044e1b0(void);
extern int   FUN_00470c60(void* prim, int depth);
extern int   FUN_00470e60(void* prim, int depth);
extern void  FUN_00438800(int a, int b, int c);
extern void FUN_00473f10(int* baseAddr, unsigned int bitIndex); // 0x00473f10
extern void Flg_on(int baseAddr, unsigned int bitIndex);         // 0x00473ef0
static void FUN_00481ab0(unsigned char* state);                  // 0x00481ab0 - file book selector
static void pickup_list_advance(unsigned char* state);           // 0x00482250 - file reading mode advance
// Japanese build (Biohazard.exe) equivalents, selected when [Assets] Version=JPN.
static void jpn_file_book_advance(unsigned char* state);                  // 0x0042b790 - JPN file book selector
static void jpn_file_list_advance(unsigned char* state);                  // 0x0042bf30 - JPN file reading mode advance
static void file_reader_advance(unsigned char* state);           // dispatch USA/JPN reader advance
static void file_load_book_cover(unsigned char book);            // dispatch USA/JPN cover load
extern unsigned char* message_item_name_lookup(unsigned char itemId); // 0x00455140
void FUN_00454fd0(int itemId, int mode, short x, short y);            // 0x00454fd0

// Left edge of the item-name line. The Japanese build's callers of
// draw_item_name (0x004912c0) push 0x22 where the USA ones push 0x30 - its
// 14px glyphs need the extra room, the same shift the message box gets. The
// item box (0x2a) and the file-title x table are identical in both builds.
static short item_name_left(void)
{
    return (GetAssetVersion() != 0) ? 0x22 : 0x30;
}

// g_ItemSlotsPointer is void* in the port; all slot access is byte-based.
#define ITEM_SLOTS  ((unsigned char*)g_ItemSlotsPointer)


extern const char DAT_004c29a0[];
extern const char DAT_004c29a8[];
extern const char DAT_004bd348[];
extern const char DAT_004bd5a0[];
extern const char DAT_004bd5a8[];
extern const unsigned char DAT_004b92c8[];
extern const unsigned char* g_ItemNamePointers[128];
extern const unsigned char* g_UnknownItemNamePointers[16];
extern const char* PTR_DAT_004b9278[];
extern const unsigned char DAT_004b92d8[];
extern const unsigned char DAT_004b92e0[];
extern const unsigned char DAT_004b92f0[];
extern const unsigned char DAT_004b92f8[];
extern void* DAT_004ba1e0[];
extern const unsigned short DAT_004d446c[];
extern const unsigned short DAT_004d4494[];
extern const unsigned short DAT_004d44c0[];
extern const char DAT_004d44e0[];
extern const char DAT_004d4500[];
extern const unsigned char DAT_004d2a08[];

// ============================================================================
// Input helpers
// The original read the D-pad from byte 1 of g_PlayerPadHeld (0x00bf0a11)
// and the action/cancel buttons from byte 1 of the remapped g_PlayerDpadPressed
// (0x00be9843). The port's pad words keep the same bit layout, so the byte-1
// accesses translate directly to (word >> 8) & 0xFF.
// ============================================================================
static unsigned char pad_held_byte1(void)
{
    return (unsigned char)(g_PlayerPadHeld >> 8);
}

static unsigned char dpad_pressed_byte1(void)
{
    return (unsigned char)(g_PlayerDpadPressed >> 8);
}

// Forward declarations for menu sub-functions
static void menu_exit_cleanup(void);              // 0x00463da0
static void menu_restore_game_state(void);        // 0x00463e10
void menu_update_equipped_weapon(void);    // 0x00463ec0
static void menu_draw_inventory(void);            // 0x00463f20
static void menu_load_item_model(void);           // 0x00464770
static void menu_update_fading_rect(void);        // 0x00429cb0
static void menu_handle_input(void);              // 0x00420880
static void menu_draw_health_bar(void);           // 0x00437fd0
static void menu_init_map_screen(void);           // 0x00488160
static void menu_init_status_screen(void);        // 0x004828f0
static int  menu_update_status_screen(void);      // 0x00482910
static void menu_reset_status_state(void);        // 0x00482b80
static int  menu_update_map_animation(void);      // 0x004886b0
static void menu_init_map_display(void);          // 0x00488880
static int  menu_itembox_interaction(void);       // 0x004941f0
static void load_main_menu_frame_part_tex_area(void);
static void draw_itembox_menu(void);              // 0x004947c0
static void loadMenuAssets(void);                 // 0x00494730
static void display_item_qty(unsigned char itemId, unsigned char qty, int depth); // 0x004645b0
static int  menu_item_use_item(void);             // 0x00401070
static int  menu_item_view_model(void);           // 0x004013a0
static int  menu_item_move_item(void);            // 0x00401470
static void menu_item_toggle_equip(void);         // 0x00401050
static int  menu_item_check_combine(void);        // 0x00401960
static void menu_item_apply_combine(void);        // 0x00401b00
static void menu_item_combine_refresh(unsigned char slot1, unsigned char slot2,
                                      unsigned char new1, unsigned char new2); // 0x00401df0
static int  menu_item_use_heal(unsigned char slot); // 0x00401260
static int  menu_item_use_item_none(unsigned char slot);   // 0x00401180/0x00401190
static int  menu_item_use_if_flag(unsigned char slot);     // 0x004011a0/0x004011e0
static int  menu_item_use_red_book(unsigned char slot);    // 0x00401220
static int  menu_item_use_always(unsigned char slot);      // 0x00401380
static int  menu_item_use_none_return0(unsigned char slot);// 0x00401390
static void menu_use_ammo_combine_a(unsigned char slotA, unsigned char slotB); // 0x00401bf0
static void menu_use_ammo_combine_b(unsigned char slotA, unsigned char slotB); // 0x00401c60
static void menu_use_qty_merge(unsigned char slotA, unsigned char slotB);      // 0x00401cd0
static void menu_use_set_flag(unsigned char slotA, unsigned char slotB);       // 0x00401d20
static void menu_use_set_mode(unsigned char slotA, unsigned char slotB);       // 0x00401d40
static void menu_use_ammo_transfer_a(unsigned char slotA, unsigned char slotB);// 0x00401d50
static void menu_use_ammo_transfer_b(unsigned char slotA, unsigned char slotB);// 0x00401da0
static void menu_item_combine_noop(unsigned char a, unsigned char b); // 0x00401390
static void menu_tab_map(void);                   // 0x00420fd0
static void menu_tab_file(void);                  // 0x00420ff0
static void menu_tab_radio(void);                 // 0x00421010
static void menu_tab_exit(void);                  // 0x00421060
static int  menu_map_state_machine(void);         // 0x00487100
static int  menu_file_state_machine(void);        // 0x00481980
static void map_tab_update(unsigned char* state);       // 0x00487200
static void map_display_update(unsigned char* state);   // 0x00487640

// Menu tab action table (0x004ba1e0), indexed by (cursor position >> 1)
static void (*const g_MenuTabActions[4])(void) = {
    menu_tab_map,    // tab 0: map
    menu_tab_file,   // tab 2: file (file book / document reader)
    menu_tab_radio,  // tab 4: radio
    menu_tab_exit    // tab 6: exit
};

// Item action submenu table (0x004ba1f0), indexed by submenu option
static int (*const g_ItemActionFunctions[3])(void) = {
    menu_item_use_item,     // option 0: use / equip
    menu_item_view_model,   // option 1: view item model
    menu_item_move_item     // option 2: move / combine
};

// Item use dispatch (0x004b1090), indexed by item use category
static int (*const g_ItemUseFunctions[12])(unsigned char) = {
    menu_item_use_item_none,     // 0: weapons (cannot use)
    menu_item_use_item_none,     // 1: ammo (cannot use)
    menu_item_use_item_none,     // 2: empty bottle (cannot use)
    menu_item_use_if_flag,       // 3: chemicals (use if flag set)
    menu_item_use_if_flag,       // 4: special items (use if flag set)
    menu_item_use_if_flag,       // 5: keys (use if flag set)
    menu_item_use_red_book,      // 6: books 0x3E-0x40 (handler only accepts the red book)
    menu_item_use_heal,          // 7: healables (spray, herbs)
    menu_item_use_always,        // 8: always usable (consumed)
    menu_item_use_none_return0   // 9: unusable
};

// Combine effect dispatch (0x004b10b4), indexed by record effect byte
static void (*const g_CombineEffectFunctions[8])(unsigned char, unsigned char) = {
    menu_item_combine_noop,          // 0: no effect
    menu_use_ammo_combine_a,         // 1
    menu_use_ammo_combine_b,         // 2
    menu_use_qty_merge,              // 3
    menu_use_set_flag,               // 4
    menu_use_set_mode,               // 5
    menu_use_ammo_transfer_a,        // 6
    menu_use_ammo_transfer_b         // 7
};

// ============================================================================
// main_menu (0x00463710)
// Main in-game menu: status screen, inventory, map, item box.
// Called as Task_execute(1, main_menu) from check_menus_state.
// ============================================================================
void main_menu(void)
{
    OutputDebugStringA("[MENU] main_menu: entered\n");

    // 0x00463710-0x0046373e: Suspend gameplay task and set up menu environment
    g_bGameActive = 0;
    Task_suspend(0);
    // empty_00470a20(): empty in the original - call dropped
    TexturePage_ClearAll();
    setMenuScreenOffset(320, 240, 4, 0, 0);
    SetSubpixelOffset(112, 76);
    menu_update_fading_rect();

    // 0x0046373e-0x00463753: Initialize sprite animation color
    g_spriteAnimR = 0;
    g_spriteAnimG = 0;
    g_spriteAnimB = 0;

    // 0x00463753-0x00463768: Fade in
    g_fade_type_id = 2;
    g_fading_counter = 0xE800;
    fade_update();

    // Declared before the goto below so the jump cannot cross an
    // initialisation (GCC rejects that; MSVC allowed it). Assigned at their
    // use sites further down.
    unsigned char charBits;
    int hasFlag;

    // 0x00463768-0x0046377d: Check if opening for message display (bit 8)
    if ((g_main_state_flags & MSF_PICKUP_SCREEN) != 0) {
        g_MainMenuState = 8;
        goto LAB_00463966;
    }

    // 0x0046377d-0x004637ae: Determine initial menu mode
    DAT_00ae9f10 = 5;
    do {
        if ((g_main_state_flags & (MSF_MENU_MODE_SHIFT_BASE >> (DAT_00ae9f10 & 0x1f))) != 0) break;
        DAT_00ae9f10 = DAT_00ae9f10 - 1;
    } while (DAT_00ae9f10 != 0);

    // 0x004637ae-0x0046381c: Initialize menu state variables
    DAT_00ae9f23 = 8;
    g_MainMenuState = 0;
    DAT_00ae9f49 = 0;
    g_bItemViewerActionIndex = 0;
    SUBMENU_STATE_ID = 0;
    DAT_00ae9f1e = 0;

    // 0x004637d4-0x0046381c: Load mode-specific assets
    switch (DAT_00ae9f10) {
    case 1:
        if (ITEM_OIL < g_selectedItemId) {
            DAT_00ae9f10 = 6;
            g_bItemMenuSelectedItemId = g_selectedItemId;
            goto LAB_0046381c;
        }
        break;
    case 2:
        // Item box mode: load item box textures
        loadMenuAssets();
        break;
    case 3:
    case 4:
        // Desk mode: get item from room event
        g_bItemMenuSelectedItemId = *(unsigned char*)(*(int*)((int)g_pRoomActionEntry + 8) + 8);
LAB_0046381c:
        menu_load_item_model();
    }

    // 0x0046381c-0x00463880: Set character display flags
    charBits = ((g_playerEntity.id + 1) & 2);
    g_totalInventorySlots = charBits + 6;
    DAT_00ae9f19 = charBits | (g_playerEntity.id & 1);

    // 0x00463880-0x004638e3: Check if map is available
    if ((g_playerEntity.id & 3) == 3) {
        DAT_00ae9f1f = 0;
        hasFlag = Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_HAS_RADIO);
        if ((hasFlag == 0) &&
            (hasFlag = Flg_ck((int)g_ScenarioFlags2, SCENARIO2_FLAG_PLANT42_OBJ), hasFlag != 0) &&
            (hasFlag = Flg_ck((int)g_ScenarioFlags2, SCENARIO2_FLAG_PROGRESS_22), hasFlag == 0))
        {
            DAT_00ae9f1f = 1;
        }
    } else {
        DAT_00ae9f1f = Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_HAS_RADIO);
    }

    // 0x004638e3-0x00463966: Set up camera and lighting for menu
    DAT_00ae9f12 = 0;
    DAT_00ae9f13 = 0;
    set_scene_render_param(0xC0);

    MATRIX_00d22680.m[0][0] = 15000;
    MATRIX_00d22680.m[0][1] = 0;
    MATRIX_00d22680.m[0][2] = 0;
    MATRIX_00d22680.m[1][0] = 0;
    MATRIX_00d22680.m[1][1] = 0;
    MATRIX_00d22680.m[1][2] = 0;
    MATRIX_00d22680.m[2][0] = 0;
    MATRIX_00d22680.m[2][1] = 0;
    MATRIX_00d22680.m[2][2] = 0;
    MATRIX_00d22680.t[0] = 0;
    MATRIX_00d22680.t[1] = 0;
    MATRIX_00d22680.t[2] = 0;
    MatrixToCamera(&MATRIX_00d22680);

    setBackColor(0x199, 0x199, 0x199);
    empty_40ae40(0);

LAB_00463966:
    StMask(0, 1);
    OutputDebugStringA("[MENU] Entering main loop\n");

    // ====================================================================
    // Main menu frame loop
    // ====================================================================
    do {
        Game_timer = Game_timer + 1;

        // 0x00463973-0x0046398a: Handle game reset
        if (g_resetGameFlag != 0) {
            menu_restore_game_state();
            Task_exit();
            return;
        }

        // 0x0046398a-0x004639b5: Set texture descriptor defaults
        g_TextureDesc.colorMulR = 0x80;
        g_TextureDesc.pivotX = 0;
        unk_00be1180 = 0;
        g_TextureDesc.pivotY = 0;
        g_TextureDesc.colorMulG = 0x80;
        g_TextureDesc.colorMulB = 0x80;

        // 0x004639b5: Menu state machine
        switch (g_MainMenuState) {
        case 0:
            // Wait for fade-in to complete, then enter appropriate submenu
            if (-1 < (short)g_fading_state) break;
            switch (DAT_00ae9f10) {
            case 0:
                g_MainMenuState = 1;
                DAT_00ae9f20 = 0;
                menu_init_map_screen();
                menu_init_status_screen();
                break;
            case 1:
                g_MainMenuState = 2;
                set_message_display(0xC4, 0);
                break;
            case 2:
                g_MainMenuState = 3;
                DAT_00ae9f20 = 0;
                break;
            case 3:
            case 4:
                g_MainMenuState = 4;
                goto LAB_00463a53;
            case 5:
                g_MainMenuState = 5;
                menu_init_map_display();
                break;
            case 6:
                g_MainMenuState = 6;
                menu_init_map_screen();
                menu_init_status_screen();
LAB_00463a53:
                DAT_00ae9f49 = 0;
            }
            DAT_00ae9f48 = 0xFF;
            DAT_00ae9f2e = 0;
            break;

        case 1:
            // Status screen + inventory navigation
            {
                // cond1 (0x00463a6b): the REMAPPED cancel button, and only
                // while the cursor owns the screen (mode 1). g_PlayerDpadPressed
                // byte 1 bit 0x80 is cancel, 0x40 is confirm; do NOT confuse it
                // with the RAW word, whose byte 1 is the D-pad itself.
                //
                // cond2 (0x00463a7d): the RAW word's 0x0800 = START, i.e. the
                // inventory toggle ('Z' in g_keyBindingData). This deliberately
                // has NO mode check - START closes the whole menu from any
                // submenu, which is why a "cancel closes everything" report is
                // usually the player pressing Z rather than a cancel key
                // (V / Left Ctrl / Esc).
                bool cond1 = (((g_PlayerDpadPressed >> 8) & 0x80) == 0) || (DAT_00ae9f20 != 1);
                bool cond2 = (((g_PlayerPadHeld >> 8) & 8) == 0) || (g_bItemViewerActionIndex != 0) || (DAT_00ae9f12 != 0);
                if (cond1 && cond2) {
                    menu_handle_input();
                    if ((DAT_00ae9f13 & 0x41) == 0) {
                        if (g_MainMenuState != 7) {
                            menu_draw_health_bar();
                        }
                        if ((DAT_00ae9f13 & 0x41) == 0) break;
                    }
                }
            }
            menu_exit_cleanup();
            play_sfx(3, 5);
            g_menu_choice_id = 0;
            break;

        case 2:
            // Message wait (use item prompt)
            if ((g_menu_choice_id & 0x80) == 0) {
                menu_exit_cleanup();
            } else {
                menu_draw_health_bar();
            }
            break;

        case 3:
            // Item box submenu
            if (menu_itembox_interaction() == 0) {
                menu_draw_health_bar();
            } else {
                menu_exit_cleanup();
            }
            break;

        case 4:
            // 3D model viewer (desk item)
            if (FUN_0044e1b0() == 0) {
                menu_draw_health_bar();
            } else {
                menu_exit_cleanup();
            }
            break;

        case 5:
            // Map screen
            if (menu_update_map_animation() == 0) {
                menu_draw_health_bar();
            } else {
                menu_exit_cleanup();
            }
            break;

        case 6:
            // Item model viewer (map/key items)
            if (FUN_0044e1b0() != 0) {
                DAT_00ae9f10 = 0;
                DAT_00ae9f20 = 0;
                g_MainMenuState = 1;
            }
            menu_draw_health_bar();
            break;

        case 7:
            // Exit animation: restore player state
            if ((DAT_00ae9f1e & 0x80) != 0) {
                ENTITY = (Entity*)&g_playerEntity;
                SetupJointStructures(&g_entityModelBuffer2);
            }

            g_playerEntity.unk_8c = 0;
            g_playerEntity.animation_frame_id = 0;
            g_playerEntity.unk_bf = 0;
            g_playerEntity.isBeingAttackedFlag = 0;

            if ((((DAT_00ae9f13 & 0x80) == 0) || (g_usedItemId < 0x1D)) || (0x1E < g_usedItemId)) {
                g_playerEntity.attackAnim = 0;
                g_playerEntity.animationId = 1;
                g_playerEntity.animFrameId = 0;
                g_playerEntity.action_behavior = 0;
                g_playerEntity.action_state = 0;
                // 0x00463c10/c16 set slot 0 (+0xBD) / ctrl id 1 (+0x84), then
                // 0x00463c21 runs ONE Joint_move over animBase before control
                // returns to gameplay. This pre-applies the first idle pose and
                // its frame timing while the menu-close fade still covers the
                // screen; leaving it out left the last BENT reach pose frozen
                // into the first visible post-close frames (the pickup "no
                // stand-up" look).
                Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x400);
            } else {
                g_playerEntity.animationId = 8;
                g_playerEntity.animFrameId = 0;
                g_playerEntity.action_behavior = 1;
                g_playerEntity.action_state = 0;
                g_playerEntity.attackAnim = 0x37;
                // 0x00463bc8: the crank-use exit also raises the completion flag the
                // room SCD event script spins on: MOV EAX,0x20 /
                // MOV [0x00be63bf],AL (unk_db = 0x20). player_scd_behavior_01's
                // completion then does Flg_on(g_SysFlags, unk_db) = SysFlags bit
                // 0x20, which the courtyard drain scripts (room3010 script 1/2's
                // FC/FD wait) test before the FMV. Without it the wait never
                // resolves and the player is stuck in the crank pose forever.
                g_playerEntity.scd_anim_param = 0x20;
                // 0x00463bd8/bde/decompile: the crank branch feeds the SAME shared
                // 0x00463c21 Joint_move from jointMoveData2/jointMoveData3.
                Joint_move(0, g_playerEntity.jointMoveData2, g_playerEntity.jointMoveData3, 0x400);
            }

            // 0x00463c35: Handle weapon equip/unequip.
            // Original disassembly: mode 0 with the used-item flag set only
            // special-cases g_usedItemId 0x1d/0x1e (unequip) and 0x4d (equip
            // 0x0b); EVERY other case - including the plain pause-menu equip,
            // where the flag is not set - falls through to
            // menu_update_equipped_weapon(). Modes 1 and 2 call it too; only
            // modes > 2 skip. The previous port missed the mode-0 fallthrough,
            // so a weapon equipped from the pause menu never reached
            // equippedWeaponId.
            if (DAT_00ae9f10 == 0) {
                if (((DAT_00ae9f13 & 0x80) != 0) && (0x1C < g_usedItemId)) {
                    if (g_usedItemId < 0x1F) {
                        DAT_00ae9f1e = 0xFF;
                        g_playerEntity.equippedWeaponId = 0;
                    } else if (g_usedItemId == 0x4D) {
                        g_playerEntity.equippedWeaponId = 0x0B;
                        DAT_00ae9f1e = 0xFF;
                    } else {
                        menu_update_equipped_weapon();
                    }
                } else {
                    menu_update_equipped_weapon();
                }
            } else if (DAT_00ae9f10 <= 2) {
                menu_update_equipped_weapon();
            }

            if (DAT_00ae9f1e != 0) {
                // 0x00463c8d: (weaponId, 0xE, g_animationBuffer, g_animObjectBuffer)
                LoadEquippedWeaponAnimation(g_playerEntity.equippedWeaponId, 0xE,
                                            (unsigned int)g_animationBuffer,
                                            (unsigned int)g_animObjectBuffer);
            }

            if ((g_main_state_flags & MSF_MIRROR_ENABLE) != 0) {
                FUN_0048c020(0xE);
            }

            // Wait for fade transitions
            while (((g_main_state_flags & MSF_FADE_ACTIVE) != 0) || (g_spriteAnimActive != 0)) {
                Task_sleep(1);
            }

            menu_restore_game_state();
            Task_exit();
            return;

        case 8:
            // Exit wait (message display mode)
            menu_reset_status_state();
            while (menu_update_status_screen() == 0) {
                Task_sleep(1);
            }
            if (g_spriteAnimActive != 0) {
                Task_sleep(1);
            }
            menu_restore_game_state();
            Task_exit();
            return;
        }

        // 0x00463d33-0x00463d70: Draw inventory and item box
        if ((g_MainMenuState != 7) && (menu_draw_inventory(), DAT_00ae9f10 == 2)) {
                draw_itembox_menu();
        }

        Task_sleep(1);

    } while (true);
}

// ============================================================================
// Stub implementations for menu sub-functions
// These need full decompilation from Ghidra.
// ============================================================================

// (0x00463da0) - Menu exit: fade out, cleanup textures
static void menu_exit_cleanup(void)
{
    FUN_004844b0();
    g_MainMenuState = 7;
    set_fading(2, 0xC00);
    g_spriteAnimR = 0;
    g_spriteAnimG = 0;
    g_spriteAnimB = 0;
    for (int i = 0; i < 3; i++) cleanup_texture_slot(i + 0xC);
    for (int i = 0; i < 8; i++) cleanup_texture_slot(i + 0xF);
    // FUN_00470a40(): empty in the original - call dropped
    FUN_0047d0e0();
}

// (0x00463e10) - Restore game state after menu close
static void menu_restore_game_state(void)
{
    setMenuScreenOffset(320, 240, 0, 0, 0);
    CenterScreenOrigin();
    menu_update_fading_rect();
    RestoreRoomCamera();
    // 0x00463e4a: FUN_0040ac80(i, g_RdtPointer->lights + i). The RDT light array
    // is at +0x0C with a 0x14 stride (Ghidra RDT: LIGHT[3] at offset 12,
    // sizeof(LIGHT) == 20). An earlier revision used base +0x30 and stride 0x2C
    // - the camera record's stride - so restoring the room after a menu close
    // reloaded the D3D lights from the middle of the light array and past its end.
    for (unsigned char i = 3; i > 0; i--) {
        FUN_0040ac80((int)(i - 1), &g_RdtPointer->lights[i - 1]);
    }
    // ambient_light is three shorts; do not narrow to a byte (see cmd_light_set)
    setBackColor((unsigned short)g_RdtPointer->ambient_light_r,
                 (unsigned short)g_RdtPointer->ambient_light_g,
                 (unsigned short)g_RdtPointer->ambient_light_b);
    empty_40ae40(0);
    g_main_state_flags &= ~MSF_MENU_BYTE;
    g_bGameActive = 2;
    Task_Resume(0);
    StMask(0, 2);
}

// (0x00463ec0) - Update equipped weapon based on menu selection
void menu_update_equipped_weapon(void)
{
    if (g_EquippedItemId == 0) {
        if (g_playerEntity.equippedWeaponId != 0) {
            g_playerEntity.equippedWeaponId = 0;
            DAT_00ae9f1e = 0xFF;
        }
    } else {
        unsigned char* slotPtr = (unsigned char*)g_ItemSlotsPointer;
        unsigned char itemId = slotPtr[(g_EquippedItemId - 1) * 2];
        if (g_playerEntity.equippedWeaponId != itemId) {
            g_playerEntity.equippedWeaponId = itemId;
            // PS1 FUN_80053958, the same ADVANCED remap LoadEquippedWeaponAnimation
            // applies: the Beretta M92FS custom EQUIPS AS weapon 2. The original
            // compares against the raw item id first (so re-equipping item 4 over
            // a remapped 2 still reloads), then overwrites, then loads the bank
            // with the remapped id - which is how the custom Beretta gets the
            // Beretta's firing report rather than the DumDum's.
            if (g_bDcMode && (g_main_state_flags2 & MSF2_DC_ADVANCED) != 0
                && itemId == DC_ITEM_BERETTA_CUSTOM) {
                g_playerEntity.equippedWeaponId = ITEM_BERETTA;
            }
            LoadSoundBank(g_playerEntity.equippedWeaponId, g_TimImageBuffer__bitmap);
            DAT_00ae9f1e = 0xFF;
        }
    }
}

// (0x00429cb0) - Update fading rect based on screen offset
static void menu_update_fading_rect(void)
{
    extern RectDrawDesc g_FadingRect;
    if (g_ScreenOffsetX == 160) {
        g_FadingRect.x = -160;
        g_FadingRect.y = -120;
    } else {
        g_FadingRect.x = 0;
        g_FadingRect.y = 0;
    }
}

// (0x00463f20) - Draw inventory HUD (items, equipped weapon, character portrait)
// 0x00463f20
static void menu_draw_inventory(void)
{
    unsigned char* pbVar2;
    unsigned short* puVar3;
    int iVar5;
    unsigned char bVar6;
    unsigned int uVar7;
    unsigned short uVar8;
    unsigned short uVar9;
    int uVar10;
    int uVar11;
    unsigned char local_2;
    char local_1;

    // 0x00463f20-0x00463f80: Draw equipped weapon or no equipped empty slot
    g_TextureDesc.flags = 0x01000040;
    g_TextureDesc.clutX = 0;
    g_TextureDesc.texturePage = 0x1c;
    g_TextureDesc.screenX = 0xa0;
    g_TextureDesc.screenY = 0x92;
    g_TextureDesc.width = 0x28;
    g_TextureDesc.height = 0x1e;
    g_invDepthLayer = 10;

    if (g_EquippedItemId == 0) {
        uVar11 = 1;
        uVar10 = 10;
        g_TextureDesc.texU = 0;
        g_TextureDesc.clutY = 0x1e0;
        g_TextureDesc.texV = 0;
    } else {
        g_TextureDesc.texU = 0x58;
        g_TextureDesc.clutY = 0x1e4;
        g_TextureDesc.texturePage = 0x1d;
        // 0x00463fa1: the original reads [ECX + 0xd21ccf] with ECX = the 1-based
        // equipped slot; 0xd21ccf is the byte just before g_ItemSlotIndices
        // (0xd21cd0), so this aliases g_ItemSlotIndices[slot - 1] - the sheet
        // row of the equipped item. Ghidra named that byte DAT_00d21ccf and an
        // earlier port read a separate all-zero table, always drawing sheet
        // row 0 (the first slot's sprite). DAT_00d21ccf is not a real global.
        g_TextureDesc.texV = g_ItemSlotIndices[g_EquippedItemId - 1] << 5;
        pbVar2 = (unsigned char*)ITEM_SLOTS + (g_EquippedItemId * 2 - 2);
        if (*pbVar2 < 0x6f) {
            uVar11 = 8;
            uVar10 = 1;
        } else {
            g_TextureDesc.texU = 0;
            uVar11 = 1;
            uVar10 = 0x1e;
            g_TextureDesc.clutY = 0x1e0;
            g_TextureDesc.texturePage = 0;
            g_TextureDesc.texV = *pbVar2 * 0x1e - 2;
        }
    }
    display_texture(&g_TextureDesc, 10, uVar10, uVar11);

    // 0x00464050: Draw equipped item quantity
    g_invDepthLayer = 9;
    if (g_EquippedItemId != 0) {
        iVar5 = (int)((unsigned int)g_EquippedItemId * 2 + (unsigned int)(uintptr_t)ITEM_SLOTS);
        g_TextureDesc.texturePage = 0x1c;
        display_item_qty(*(unsigned char*)(iVar5 - 2), *(unsigned char*)(iVar5 - 1), 9);
    }

    // 0x00464090: Calculate starting position index for inventory items
    local_2 = g_TotalHeldItems * 2 + 8;
    if ((DAT_00ae9f19 & 2) == 0) {
        local_2 = g_TotalHeldItems * 2 + 0xc;
    }

    // 0x004640d0: Draw each held inventory item
    g_invDepthLayer = 9;
    bVar6 = g_TotalHeldItems;
    while (bVar6 != 0) {
        g_TextureDesc.texturePage = 0x1d;
        g_TextureDesc.width = 40;
        bVar6 = bVar6 - 1;
        g_TextureDesc.height = 30;
        uVar7 = (unsigned int)bVar6;
        g_TextureDesc.clutY = 0x1e4;
        g_TextureDesc.texU = 88;
        g_TextureDesc.screenY = *(short*)((int)g_inventorySlotsPos + (unsigned int)(unsigned char)(local_2 - 1) * 2);
        local_2 = local_2 - 2;
        g_TextureDesc.screenX = *(short*)((int)g_inventorySlotsPos + (unsigned int)local_2 * 2);
        g_TextureDesc.texV = g_ItemSlotIndices[uVar7] << 5;
        pbVar2 = (unsigned char*)ITEM_SLOTS + uVar7 * 2;
        if (*pbVar2 < 0x6f) {
            uVar11 = 8;
            uVar10 = 1;
        } else {
            uVar11 = 1;
            g_TextureDesc.texU = 0;
            uVar10 = 0x1e;
            g_TextureDesc.texturePage = 0;
            g_TextureDesc.clutY = 0x1e0;
            g_TextureDesc.texV = *pbVar2 * 0x1e - 2;
        }
        display_texture(&g_TextureDesc, (unsigned short)g_invDepthLayer + 1, uVar10, uVar11);

        g_TextureDesc.texturePage = 0x1c;
        pbVar2 = (unsigned char*)ITEM_SLOTS + uVar7 * 2;
        display_item_qty(*pbVar2, *(pbVar2 + 1), g_invDepthLayer);
    }

    // 0x00464200: Draw empty inventory slots
    g_TextureDesc.texU = 0;
    g_TextureDesc.texV = 0;
    bVar6 = 24;
    g_TextureDesc.width = 40;
    g_invDepthLayer = 10;
    g_TextureDesc.height = 30;
    g_TextureDesc.clutY = 0x1e0;
    for (char cVar4 = g_totalInventorySlots - g_TotalHeldItems; cVar4 != 0; cVar4 = cVar4 - 1) {
        g_TextureDesc.screenY = *(short*)((int)g_inventorySlotsPos + (unsigned int)(unsigned char)(bVar6 - 1) * 2);
        bVar6 = bVar6 - 2;
        g_TextureDesc.screenX = *(short*)((int)g_inventorySlotsPos + (unsigned int)bVar6 * 2);
        display_texture(&g_TextureDesc, (unsigned short)g_invDepthLayer, 10, 1);
    }

    // 0x00464290: Draw character portrait
    g_TextureDesc.screenX = 0x16;
    g_TextureDesc.screenY = 0x92;
    g_TextureDesc.width = 30;
    g_TextureDesc.height = 30;
    g_TextureDesc.clutY = 0x1e0;
    g_invDepthLayer = 10;
    g_TextureDesc.texU = (g_playerEntity.id & 1) << 5;
    g_TextureDesc.texV = (g_playerEntity.id & 2) << 4;
    bVar6 = 8;
    display_texture(&g_TextureDesc, 10, 9, 1);

    // 0x00464310: Draw top options menu (Map, File, Radio, Exit)
    g_CurrentMenuFramesDataPtr = (unsigned short*)g_MainMenuTopOptionsPos;
    g_invDepthLayer = 20;
    g_TextureDesc.clutY = 0x1e4;
    do {
        bVar6 = bVar6 - 2;
        load_main_menu_frame_part_tex_area();
        g_TextureDesc.flags = 0x1000000;
        if (DAT_00ae9f10 == 2) {
            if ((DAT_00ae9f23 | 2) == bVar6) {
LAB_004642e7:
                g_TextureDesc.flags = 0x01000040;
            }
        } else if (((g_MainMenuState == 1) && (DAT_00ae9f23 == bVar6)) && (DAT_00ae9f20 < 2)) {
            goto LAB_004642e7;
        }
        if ((bVar6 == 4) && (DAT_00ae9f1f == 0)) {
            g_TextureDesc.texV = 0x40;
        }
        display_texture(&g_TextureDesc, (unsigned short)g_invDepthLayer, 0, 1);

        // 0x00464370: Draw frames
        if (bVar6 == 0) {
            g_TextureDesc.flags = 0x01000040;

            // main frame (left-top-right borders)
            // Health frame
            // equipped weapon frame
            // bottom frames
            // options menu top and bottom borders
            // inventory outer frame bottom border
            g_CurrentMenuFramesDataPtr = (unsigned short*)g_MainMenuFrames2Pos;
            g_invDepthLayer = 0x15;
            do {
                load_main_menu_frame_part_tex_area();
                display_texture(&g_TextureDesc, (unsigned short)g_invDepthLayer, 0, 1);
            } while ((unsigned short*)g_MainMenuFramesPos < g_CurrentMenuFramesDataPtr);

            // Main frame bottom border
            // Inventory bottom inner border
            // Top options menu left and right borders
            g_CurrentMenuFramesDataPtr = (unsigned short*)g_MainMenuFrames3Pos;
            g_invDepthLayer = 10;
            do {
                puVar3 = g_CurrentMenuFramesDataPtr;
                g_CurrentMenuFramesDataPtr = g_CurrentMenuFramesDataPtr - 1;
                g_TextureDesc.flags = (*g_CurrentMenuFramesDataPtr & 0xc0) << 0x10 | 0x1000040;
                bVar6 = *(unsigned char*)((int)puVar3 - 1);
                load_main_menu_frame_part_tex_area();
                if ((bVar6 & 0x80) == 0) {
                    uVar8 = 0;
                    uVar9 = g_TextureDesc.width;
                } else {
                    uVar8 = g_TextureDesc.height;
                    uVar9 = 0;
                }
                bVar6 = bVar6 & 0x7f;
                do {
                    display_texture(&g_TextureDesc, (unsigned short)bVar6 + (unsigned short)g_invDepthLayer, 0, 1);
                    g_TextureDesc.screenX = g_TextureDesc.screenX + uVar9;
                    g_TextureDesc.screenY = g_TextureDesc.screenY + uVar8;
                    bVar6 = bVar6 - 1;
                } while (bVar6 != 0);
            } while ((unsigned short*)g_MainMenuTopOptionsPos < g_CurrentMenuFramesDataPtr);

            // Invntory top, left and right borders
            g_CurrentMenuFramesDataPtr = (unsigned short*)g_MainMenuFrames4Pos;
            if ((DAT_00ae9f19 & 2) == 0) {
                g_CurrentMenuFramesDataPtr = (unsigned short*)g_MainMenuFrames3Pos;
            }
            g_CurrentMenuFramesDataPtr = (unsigned short*)((int)g_CurrentMenuFramesDataPtr + 0x7e);
            local_1 = 9;
            g_invDepthLayer = 0x14;
            do {
                puVar3 = g_CurrentMenuFramesDataPtr;
                g_CurrentMenuFramesDataPtr = g_CurrentMenuFramesDataPtr - 1;
                local_1 = local_1 - 1;
                g_TextureDesc.flags = (*g_CurrentMenuFramesDataPtr & 0xc0) << 0x10 | 0x1000040;
                bVar6 = *(unsigned char*)((int)puVar3 - 1);
                load_main_menu_frame_part_tex_area();
                if ((bVar6 & 0x80) == 0) {
                    uVar8 = 0;
                    uVar9 = g_TextureDesc.width;
                } else {
                    uVar8 = g_TextureDesc.height;
                    uVar9 = 0;
                }
                bVar6 = bVar6 & 0x7f;
                do {
                    // 0x0046448e: unlike the frame-segment loop above (which
                    // draws each tile at count+depth, 0x004643c8), the panel
                    // border tiles draw at the plain 0x14 depth. Adding the
                    // tile counter sank multi-tile runs (the top border row)
                    // behind the frames2 submenu bottom border strip at depth
                    // 21, which then overdraws them - Jill's inventory top
                    // border looked merged with/hidden by the submenu border.
                    display_texture(&g_TextureDesc, (unsigned short)g_invDepthLayer, 0, 1);
                    g_TextureDesc.screenX = g_TextureDesc.screenX + uVar9;
                    g_TextureDesc.screenY = g_TextureDesc.screenY + uVar8;
                    bVar6 = bVar6 - 1;
                } while (bVar6 != 0);
            } while (local_1 != 0);

            // 0x00464510: Draw background black rect
            g_CurrentMenuFramesDataPtr = (unsigned short*)g_inventorySlotsPos;
            g_rect.textureId = 0;
            g_rect.r = 0;
            g_rect.g = 0;
            g_invDepthLayer = 0x1e;
            g_rect.b = 0;
            do {
                g_rect.h = *(short*)((int)g_CurrentMenuFramesDataPtr - 2);
                g_rect.w = *(short*)((int)g_CurrentMenuFramesDataPtr - 4);
                g_rect.y = *(short*)((int)g_CurrentMenuFramesDataPtr - 6);
                g_rect.x = *(short*)((int)g_CurrentMenuFramesDataPtr - 8);
                draw_rect(&g_rect, (unsigned short)g_invDepthLayer, 1);
                g_CurrentMenuFramesDataPtr = (unsigned short*)((int)g_CurrentMenuFramesDataPtr - 8);
            } while ((unsigned short*)DAT_004c2940 < g_CurrentMenuFramesDataPtr);
            return;
        }
    } while (true);
}

// ============================================================================
// display_item_qty (0x004645b0)
// Draw item quantity text (or infinite symbol for special items).
// Reads g_TextureDesc for screenX/Y and modifies it for subsequent draws.
// ============================================================================
static void display_item_qty(unsigned char itemId, unsigned char qty, int depth)
{
    // Only draw for quantifiable items:
    //   Weapons and ammo (2-18, excluding knife), ink ribbon (0x2F),
    //   or infinite weapons (> ITEM_NON_INFINITE_MAX).
    if (!(((itemId < ITEM_EMPTY_BOTTLE && itemId != ITEM_KNIFE) ||
           itemId == ITEM_INK_RIBBONS) ||
           itemId > ITEM_NON_INFINITE_MAX))
        return;

    g_TextureDesc.clutY = 0x1E4;
    g_TextureDesc.screenY = g_TextureDesc.screenY + 0x14;
    g_TextureDesc.height = 8;

    int hasInfRLauncher = Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_INF_R_LAUNCHER);
    
    // Director's Cut (PS1 SLUS_005.51 0x8005436c): the ADVANCED best-ending
    // unlock (flag 0x7A) makes the Colt Python count as unlimited, so its
    // digits give way to the same infinity glyph. The guard wraps the original
    // condition rather than widening it, so the rocket flag's behaviour for
    // every other item is untouched.
    int hasInfPython = dc_is_infinite_colt_python(itemId);

    // Normal numeric quantity display
    // (flag 0x7E grants infinite quantity for certain items like the rocket launcher)
    if (!(hasInfPython != 0 && itemId == ITEM_COLT_PYTHON_MAG)
        && ((hasInfRLauncher == 0 && itemId <= ITEM_NON_INFINITE_MAX) ||
            (itemId != ITEM_ROCKET_LAUNCHER && itemId <= ITEM_NON_INFINITE_MAX)))
    {
        // Clip quantity display for certain weapon types
        if (itemId != ITEM_FLAMETHROWER && itemId < ITEM_CLIP)
            qty = qty & 0x7F;

        sprintf(PRINT_TEXT_BUFFER, "%03d", (unsigned int)qty);

        // Position text relative to the item icon
        if (itemId < ITEM_CLIP)
            g_TextureDesc.screenX = g_TextureDesc.screenX + 4;
        else
            g_TextureDesc.screenX = g_TextureDesc.screenX + 0xE;

        g_TextureDesc.width = 8;

        // Select digit font column based on item type.
        // Different columns in STATUS.tim hold different glyph variants.
        switch (itemId)
        {
        case ITEM_COLT_PYTHON_DUM:
            // Item 4 is the DumDum-rounds Colt Python in the USA build, so it
            // takes the orange ammo column with the other DumDum/acid items.
            // The DC gives id 4 to the Beretta M92FS custom and drops this case
            // from the switch (PS1 SLUS_005.51 0x8005436c leaves only 8/0x0D/
            // 0x11 orange, against SLUS_001.70 0x80054b00's 4/8/0x0D/0x11), so
            // the custom Beretta counts up in green like the plain one.
            // (docs/DC_PORT.md 3i)
            if (g_bDcMode) {
                g_TextureDesc.texU = 0x80; // green digits
                break;
            }
            // fall through - the USA build's DumDum Python
        case ITEM_BAZOOKA_ACID:
        case ITEM_DUM_DUM_ROUNDS:
        case ITEM_ACID_ROUNDS:
            g_TextureDesc.texU = 0x88; // orange digits
            break;
        case ITEM_BAZOOKA_FLAME:
        case ITEM_FLAME_ROUNDS:
            g_TextureDesc.texU = 0xC0; // red digits
            break;
        default:
            g_TextureDesc.texU = 0x80; // green digits
            break;
        }

        char* pch = PRINT_TEXT_BUFFER;
        bool drawnAny = false;
        int remaining = 3;

        do {
            remaining--;
            unsigned char digit = (unsigned char)(*pch & 0xF);  // ASCII '0'-'9' → 0-9

            // Draw digit if: non-zero, OR already drawn one, OR it's the last position
            if (digit != 0 || drawnAny || remaining == 0)
            {
                g_TextureDesc.texV = digit << 3;  // digit × 8 pixels
                display_texture(&g_TextureDesc, (unsigned short)depth, 0, 1);
                drawnAny = true;
            }

            // Advance cursor: always for items beyond weapons (> ITEM_ROCKET_LAUNCHER),
            // otherwise only after first drawn digit (suppresses leading-zero spacing)
            if (drawnAny || itemId > ITEM_ROCKET_LAUNCHER)
                g_TextureDesc.screenX = g_TextureDesc.screenX + 8;

            pch++;
        } while (remaining != 0);
    }
    else
    {
        // Draw infinite (∞) symbol for unlimited-quantity items
        g_TextureDesc.texU = 152;
        g_TextureDesc.texV = 112;
        g_TextureDesc.screenX = g_TextureDesc.screenX + 6;
        g_TextureDesc.width = 10;
        display_texture(&g_TextureDesc, (unsigned short)depth, 0, 1);
    }
}

// ============================================================================
// Menu input, cursor and item action functions
// ============================================================================

// ============================================================================
// Map state block (0x00ac9890 - 0x00ac98a7)
// Shared by the map tab browser (menu_map_state_machine) and the full map
// display (main menu mode 5). Byte layout:
//   +0   state (0-8 open/display/close cycle)
//   +1   sub-state of map_tab_update / map_display_update (0 = init, 1 = run)
//   +2   "you are here" dot blink animation state (0-2)
//   +3   map zoom animation state (0-9)              [DAT_00ac9893]
//   +4   current map layout (0-3)
//   +5   area group (stageId % 5, or 5 for the lab map display)
//   +6   current area id (0-10)
//   +7   current room id (0-0x1f)
//   +8..+0xb  room index per layout (the room whose area matches +6)
//   +0xc move flags (bit 0 = next room exists, bit 1 = previous room)
//   +0xd layout availability bitmask
//   +0xe floor-transition arrow animation counter
//   +0xf map mode/variant (0/1/2/3/4)                [DAT_00ac989f]
//   +0x10 area highlight animation state
//   +0x11 highlight frame counter
//   +0x12 display cycle countdown                    [DAT_00ac98a2]
//   +0x14 map exit flag                              [DAT_00ac98a4]
//   +0x15 highlight frame
//   +0x16 ushort: area-known bitmask (11 areas)      [DAT_00ac98a6]
// ============================================================================
static unsigned char DAT_00ac9890[0x18];            // 0x00ac9890
#define MAP_STATE       (DAT_00ac9890[0x00])
#define MAP_SUBSTATE    (DAT_00ac9890[0x01])
#define MAP_BLINK_STATE (DAT_00ac9890[0x02])
#define MAP_ZOOM_STATE  (DAT_00ac9890[0x03])        // DAT_00ac9893
#define MAP_LAYOUT      (DAT_00ac9890[0x04])        // DAT_00ac9894
#define MAP_GROUP       (DAT_00ac9890[0x05])        // DAT_00ac9895
#define MAP_AREA        (DAT_00ac9890[0x06])        // DAT_00ac9896
#define MAP_ROOM        (DAT_00ac9890[0x07])        // DAT_00ac9897
#define MAP_ROOM_IDX    (&DAT_00ac9890[0x08])       // DAT_00ac9898..0x9b (4)
#define MAP_MOVE_FLAGS  (DAT_00ac9890[0x0c])        // DAT_00ac989c
#define MAP_LAYOUT_MASK (DAT_00ac9890[0x0d])        // DAT_00ac989d
#define MAP_ARROW_ANIM  (DAT_00ac9890[0x0e])        // DAT_00ac989e
#define MAP_MODE        (DAT_00ac9890[0x0f])        // DAT_00ac989f
#define MAP_HL_STATE    (DAT_00ac9890[0x10])        // DAT_00ac98a0
#define MAP_HL_COUNT    (DAT_00ac9890[0x11])        // DAT_00ac98a1
#define MAP_TIMER       (DAT_00ac9890[0x12])        // DAT_00ac98a2
#define MAP_EXIT_FLAG   (DAT_00ac9890[0x14])        // DAT_00ac98a4
#define MAP_HL_FRAME    (DAT_00ac9890[0x15])        // DAT_00ac98a5
#define MAP_AREA_MASK   (*(unsigned short*)&DAT_00ac9890[0x16])  // DAT_00ac98a6

// Map display position values (0x00ac98a8 - 0x00ac98af, 4 shorts).
// Written by the map display functions; nothing in the original reads them
// back (dead stores kept for fidelity).
static short DAT_00ac98a8[4];                       // 0x00ac98a8

// Scratch copy of the zoom sprite used for the flashing "you are here" dot
// (0x00ac3668, 36 bytes). The dot's color multiplier (byte +0x14) is
// overwritten every frame with the blink phase.
static unsigned int DAT_00ac3668[9];                // 0x00ac3668

// "You are here" dot blink phase (0x00ac368c - 0x00ac3694)
static unsigned char  DAT_00ac368c;                 // 0x00ac368c
static unsigned short DAT_00ac3690;                 // 0x00ac3690
static unsigned char  DAT_00ac3694;                 // 0x00ac3694

// File tab state block (0x00ac98b0) - shared by the file book selector
// (FUN_00481ab0 family) and the file reading mode (pickup_list_advance).
static unsigned char DAT_00ac98b0[0x20]; // 0x00ac98b0
#define FILE_STATE      DAT_00ac98b0[0x00]   // 0 = book selector, 1 = reading mode
#define FILE_SUBSTATE   DAT_00ac98b0[0x01]   // selector: 0 = init, 1 = input
#define FILE_REQUEST    DAT_00ac98b0[0x02]   // selector render sub-state / requests
#define FILE_MODE       DAT_00ac98b0[0x03]   // fade mode byte (1 = fade in, 2 = fade out)
#define FILE_SLOT       DAT_00ac98b0[0x05]   // selected file slot (0-7, 0xff = none)
#define FILE_EXITFLAG   DAT_00ac98b0[0x08]   // close the file tab
#define FILE_BUSY       DAT_00ac98b0[0x09]   // input lock
#define FILE_SAVEFLAG   DAT_00ac98b0[0x10]   // reading mode: cursor at the last page

// (0x00420b80) - Update selected item id from the slot under the cursor
static void menu_update_selected_item(void)
{
    unsigned char bVar1 = (DAT_00ae9f23 >> 1) - 4;
    if (bVar1 < g_TotalHeldItems) {
        g_bItemMenuSelectedItemId = *(unsigned char*)(ITEM_SLOTS + (unsigned int)bVar1 * 2);
        return;
    }
    g_bItemMenuSelectedItemId = 0;
}

// (0x00401050) - Equip / unequip the item under the cursor
static void menu_item_toggle_equip(void)
{
    char cVar1 = (char)((DAT_00ae9f23 >> 1) - 3);
    if (cVar1 != g_EquippedItemId) {
        g_EquippedItemId = cVar1;
        return;
    }
    g_EquippedItemId = 0;
}

// (0x00401180 / 0x00401190 / 0x00401390) - Item cannot be used
static int menu_item_use_item_none(unsigned char slot) { return 0; }
static int menu_item_use_none_return0(unsigned char slot) { return 0; }
static void menu_item_combine_noop(unsigned char a, unsigned char b) { }

// (0x00401380) - Item always usable (consumed)
static int menu_item_use_always(unsigned char slot) { return 1; }

// (0x004011a0 / 0x004011e0) - Use item if its flag in g_itemUseFlags is set
static int menu_item_use_if_flag(unsigned char slot)
{
    if (Flg_ck((int)g_itemUseFlags, (unsigned int)(g_bItemMenuSelectedItemId - 0x1b)) != 0) {
        DAT_00ae9f13 = DAT_00ae9f13 | 0x80;
        g_usedItemId = g_bItemMenuSelectedItemId;
        return 1;
    }
    return 0;
}

// (0x00401220) - Red book use check. Dispatch slot 6 covers books 0x3E-0x40
// (red book / Doom Book 1+2), but only the red book passes the ID check.
static int menu_item_use_red_book(unsigned char slot)
{
    if (g_bItemMenuSelectedItemId == ITEM_RED_BOOK) {
        if (Flg_ck((int)g_itemUseFlags, 0x23) != 0) {
            DAT_00ae9f13 = DAT_00ae9f13 | 0x80;
            g_usedItemId = g_bItemMenuSelectedItemId;
            return 1;
        }
    }
    return 0;
}

// (0x00401260) - Heal / status-cure item use (spray, herbs, serum)
static int menu_item_use_heal(unsigned char slot)
{
    unsigned char bVar1;
    unsigned short uVar2;
    int used = 0;

    bVar1 = g_ItemHealTable[g_bItemMenuSelectedItemId];
    if (bVar1 != 0) {
        used = 0;
        if ((bVar1 & 0xf) != 0) {
            unsigned short maxHealth = (unsigned short)g_playerEntity.maxHealth;
            if ((int)g_playerEntity.health < (int)maxHealth) {
                used = 1;
                DAT_00ae9f13 = DAT_00ae9f13 | 0x20;
                unsigned char amount = bVar1 & 0xf;
                if (amount == 1) {
                    g_playerEntity.health = g_playerEntity.health + g_playerEntity.maxHealth / 3;
                    uVar2 = g_playerEntity.health;
                } else if (amount == 2) {
                    g_playerEntity.health = g_playerEntity.health + (short)(((unsigned long long)maxHealth * 2) / 3);
                    uVar2 = g_playerEntity.health;
                } else {
                    uVar2 = maxHealth;
                    if (amount != 3) {
                        uVar2 = g_playerEntity.health;
                    }
                }
                g_playerEntity.health = uVar2;
                if ((int)maxHealth < (int)g_playerEntity.health) {
                    g_playerEntity.health = maxHealth;
                }
            }
        }
        if (((bVar1 & 0xf0) != 0) && ((g_playerEntity.healthStatusFlags & 0x22) != 0)) {
            if ((bVar1 & 0xf0) == 0x10) {
                if ((g_playerEntity.healthStatusFlags & 0x20) != 0) {
                    FUN_00473f10((int*)g_ScenarioFlags2, SCENARIO2_FLAG_YAWN_POISONED);
                    g_playerEntity.healthStatusFlags = g_playerEntity.healthStatusFlags & 0xdf;
                }
            } else {
                if (((bVar1 & 0xf0) != 0x20) || ((g_playerEntity.healthStatusFlags & 2) == 0)) {
                    goto heal_done;
                }
                g_playerEntity.healthStatusFlags = g_playerEntity.healthStatusFlags & 0xfd;
            }
            DAT_00ae9f13 = DAT_00ae9f13 | 0x10;
            used = 1;
        }
    }
heal_done:
    if (used != 0) {
        DAT_00ae9f2e = 2;
        DAT_00ae9f12 = 0xff;
    }
    return used;
}

// (0x00401bf0) - Combine effect 1: transfer ammo quantity (cursor slot wins)
static void menu_use_ammo_combine_a(unsigned char slotA, unsigned char slotB)
{
    unsigned char* pbVar4 = (unsigned char*)(ITEM_SLOTS + (unsigned int)slotA * 2);
    unsigned char* pbVar1 = pbVar4 + 1;
    unsigned char* pbOther = (unsigned char*)(ITEM_SLOTS + 1 + (unsigned int)slotB * 2);
    unsigned short uVar6 = (unsigned short)*pbOther + (*pbVar1 & 0x7f);
    unsigned char bVar2 = g_ItemMaxQty[(unsigned int)*pbVar4 * 4];
    unsigned char bVar5 = (unsigned char)uVar6;
    if (bVar2 < uVar6) {
        *pbVar1 = bVar2;
        *pbOther = (unsigned char)(bVar5 - bVar2);
        return;
    }
    *pbVar1 = bVar5;
    *(unsigned char*)(ITEM_SLOTS + (unsigned int)slotB * 2) = 0;
}

// (0x00401c60) - Combine effect 2: transfer ammo quantity (other slot wins)
static void menu_use_ammo_combine_b(unsigned char slotA, unsigned char slotB)
{
    unsigned char* pbVar4 = (unsigned char*)(ITEM_SLOTS + (unsigned int)slotB * 2);
    unsigned char* pbVar1 = pbVar4 + 1;
    unsigned char* pbOther = (unsigned char*)(ITEM_SLOTS + 1 + (unsigned int)slotA * 2);
    unsigned short uVar6 = (unsigned short)*pbOther + (*pbVar1 & 0x7f);
    unsigned char bVar2 = g_ItemMaxQty[(unsigned int)*pbVar4 * 4];
    unsigned char bVar5 = (unsigned char)uVar6;
    if (bVar2 < uVar6) {
        *pbVar1 = bVar2;
        *pbOther = (unsigned char)(bVar5 - bVar2);
        return;
    }
    *pbVar1 = bVar5;
    *(unsigned char*)(ITEM_SLOTS + (unsigned int)slotA * 2) = 0;
}

// (0x00401cd0) - Combine effect 3: merge quantities (cap 0xFA)
static void menu_use_qty_merge(unsigned char slotA, unsigned char slotB)
{
    unsigned char* pbVar2 = (unsigned char*)(ITEM_SLOTS + 1 + (unsigned int)slotA * 2);
    unsigned short uVar4 = (unsigned short)*(unsigned char*)(ITEM_SLOTS + 1 + (unsigned int)slotB * 2)
                           + (unsigned short)*pbVar2;
    unsigned char bVar3 = (unsigned char)uVar4;
    if (0xfa < uVar4) {
        *pbVar2 = 0xfa;
        *(unsigned char*)(ITEM_SLOTS + 1 + (unsigned int)slotB * 2) = (unsigned char)(bVar3 + 6);
        return;
    }
    *pbVar2 = bVar3;
    *(unsigned char*)(ITEM_SLOTS + (unsigned int)slotB * 2) = 0;
}

// (0x00401d20) - Combine effect 4: set chemical flag
static void menu_use_set_flag(unsigned char slotA, unsigned char slotB)
{
    DAT_00ae9f12 = 2;
    Flg_on((int)g_ScenarioFlags, SCENARIO_FLAG_CHEMICAL_COMBINE);
}

// (0x00401d40) - Combine effect 5: set mode
static void menu_use_set_mode(unsigned char slotA, unsigned char slotB)
{
    DAT_00ae9f12 = 3;
}

// (0x00401d50) - Combine effect 6: ammo transfer A
static void menu_use_ammo_transfer_a(unsigned char slotA, unsigned char slotB)
{
    unsigned char bVar4 = *(unsigned char*)(ITEM_SLOTS + 1 + (unsigned int)slotB * 2);
    unsigned char* pbVar1 = (unsigned char*)(ITEM_SLOTS + (unsigned int)slotA * 2);
    unsigned char bVar5 = g_ItemMaxQty[(unsigned int)*pbVar1 * 4];
    if (bVar5 < bVar4) {
        pbVar1[1] = bVar5;
        *(unsigned char*)(ITEM_SLOTS + 1 + (unsigned int)slotB * 2) =
            *(unsigned char*)(ITEM_SLOTS + 1 + (unsigned int)slotB * 2) - bVar5;
        return;
    }
    pbVar1[1] = bVar4;
    *(unsigned char*)(ITEM_SLOTS + (unsigned int)slotB * 2) = 0;
}

// (0x00401da0) - Combine effect 7: ammo transfer B
static void menu_use_ammo_transfer_b(unsigned char slotA, unsigned char slotB)
{
    unsigned char bVar4 = *(unsigned char*)(ITEM_SLOTS + 1 + (unsigned int)slotA * 2);
    unsigned char* pbVar1 = (unsigned char*)(ITEM_SLOTS + (unsigned int)slotB * 2);
    unsigned char bVar5 = g_ItemMaxQty[(unsigned int)*pbVar1 * 4];
    if (bVar5 < bVar4) {
        pbVar1[1] = bVar5;
        *(unsigned char*)(ITEM_SLOTS + 1 + (unsigned int)slotA * 2) =
            *(unsigned char*)(ITEM_SLOTS + 1 + (unsigned int)slotA * 2) - bVar5;
        return;
    }
    pbVar1[1] = bVar4;
    *(unsigned char*)(ITEM_SLOTS + (unsigned int)slotA * 2) = 0;
}

// (0x00401df0) - Refresh item images after a combine
static void menu_item_combine_refresh(unsigned char slot1, unsigned char slot2,
                                      unsigned char new1, unsigned char new2)
{
    unsigned char bVar1;
    unsigned char refresh1 = 0;
    unsigned char refresh2 = 0;

    bVar1 = *(unsigned char*)(ITEM_SLOTS + (unsigned int)slot1 * 2);
    if ((bVar1 != 0) && (bVar1 != new1)) {
        refresh1 = g_ItemImageTypeTable[g_ItemImageLookupTable[(unsigned int)bVar1 * 4 + 1]];
    }
    bVar1 = *(unsigned char*)(ITEM_SLOTS + (unsigned int)slot2 * 2);
    if ((bVar1 != 0) && (bVar1 != new2)) {
        refresh2 = g_ItemImageTypeTable[g_ItemImageLookupTable[(unsigned int)bVar1 * 4 + 1]];
    }
    if (((((char)DAT_00ae9f1e + 1) & 0x7f) != 0) && ((refresh2 != 0) || (refresh1 != 0))) {
        DAT_00ae9f1e = DAT_00ae9f1e | 0x7f;
        LoadFile((char*)g_ItemMixPixPath, g_TimImageBuffer__bitmap, 0x20);
    }
    if (refresh1 != 0) {
        LoadItemImage((int)refresh1 - 1, (int)g_ItemSlotIndices[slot1], (int)g_TimImageBuffer__bitmap);
    }
    if (refresh2 != 0) {
        LoadItemImage((int)refresh2 - 1, (int)g_ItemSlotIndices[slot2], (int)g_TimImageBuffer__bitmap);
    }
}

// (0x00401960) - Item combine check. Returns:
//   0 = cannot combine, 1 = requires lab room, 2 = herb combo, 3 = herb+herb,
//   4 = combine applied
static int menu_item_check_combine(void)
{
    unsigned char bVar3 = (DAT_00ae9f23 >> 1) - 4;
    unsigned char* pbVar7 = (unsigned char*)(ITEM_SLOTS + (unsigned int)bVar3 * 2);
    unsigned char bVar1 = *pbVar7;
    unsigned char bVar4 = (DAT_00ae9f25 >> 1) - 4;
    unsigned char* pbVar5 = (unsigned char*)(ITEM_SLOTS + (unsigned int)bVar4 * 2);
    unsigned char bVar2 = *pbVar5;

    const unsigned char* pTable = g_ItemCombinePtrs[(char)g_ItemImageLookupTable[(unsigned int)g_bItemMenuSelectedItemId * 4 + 1]];
    const unsigned char* pRec = pTable + 1;
    unsigned char cVar6 = *pTable;
    for (; cVar6 != 0; cVar6--) {
        if (*pRec == bVar2) {
            if (5 < pRec[3]) {
                if (pRec[3] == 6) {
                    if (pbVar7[1] != 0) return 0;
                } else if (pbVar5[1] != 0) {
                    return 0;
                }
            }
            play_sfx(3, 6, 0);
            if ((bVar1 < 0x13) || (0x1a < bVar1)) {
                if ((0x42 < bVar1) && (bVar1 < 0x4c)) return 2;
            } else if ((g_roomId != ROOM_DRUG_STOREHOUSE) || (get_stage_id() != STAGE_GUARDHOUSE)) {
                return 1;
            }
            *(unsigned char*)(ITEM_SLOTS + (unsigned int)bVar3 * 2) = pRec[1];
            *(unsigned char*)(ITEM_SLOTS + (unsigned int)bVar4 * 2) = pRec[2];
            if (pRec[3] != 0) {
                g_CombineEffectFunctions[pRec[3] & 7](bVar3, bVar4);
            }
            // 0x00401ac2-0x00401ad2: the last two arguments are the slots' OLD
            // item ids (bVar1/bVar2, read at the top of this function), NOT the
            // new ones just written. menu_item_combine_refresh reloads a slot's
            // sprite only when its CURRENT id differs from the id passed in, so
            // passing pRec[1]/pRec[2] compared the new id against itself: always
            // equal, never a reload. Mixing chemicals left the old bottle sprite
            // on the slot that had become an EMPTY BOTTLE.
            // (menu_item_apply_combine, the confirmed-move path, already passed
            // the old ids - which is why only the direct mix looked stale.)
            menu_item_combine_refresh(bVar3, bVar4, bVar1, bVar2);
            rearrange_item_slots();
            return 4;
        }
        pRec += 4;
    }
    if ((((0x42 < bVar1) && (bVar1 < 0x4c)) && (0x42 < bVar2)) && (bVar2 < 0x4c)) {
        return 3;
    }
    return 0;
}

// (0x00401b00) - Apply a confirmed combine (called from the move confirmation)
static void menu_item_apply_combine(void)
{
    unsigned char bVar5;
    unsigned char bVar4;
    unsigned char cVar1;
    unsigned char cVar2;
    const unsigned char* pRec;

    if (g_menu_choice_id == 0) {
        play_sfx(3, 6, 0);
        DAT_00ae9f22 = 3;
        bVar5 = (DAT_00ae9f23 >> 1) - 4;
        unsigned char* pcVar6 = (unsigned char*)((unsigned int)bVar5 * 2 + ITEM_SLOTS);
        DAT_00ae9f13 = DAT_00ae9f13 | 8;
        cVar1 = *pcVar6;
        bVar4 = (DAT_00ae9f25 >> 1) - 4;
        cVar2 = *(unsigned char*)(ITEM_SLOTS + (unsigned int)bVar4 * 2);

        pRec = g_ItemCombinePtrs[(char)g_ItemImageLookupTable[(unsigned int)g_bItemMenuSelectedItemId * 4 + 1]] + 1;
        unsigned char cVar3 = *pRec;
        while (cVar3 != cVar2) {
            pRec += 4;
            cVar3 = *pRec;
        }
        *pcVar6 = pRec[1];
        *(unsigned char*)(ITEM_SLOTS + (unsigned int)bVar4 * 2) = pRec[2];
        menu_item_combine_refresh(bVar5, bVar4, cVar1, cVar2);
        rearrange_item_slots();
        menu_update_selected_item();
        return;
    }
    play_sfx(3, 5, 0);
    DAT_00ae9f22 = 2;
}

// (0x00401070) - Item action: use / equip the selected item
static int menu_item_use_item(void)
{
    unsigned char bVar5;
    unsigned char bVar6;
    unsigned char bVar2;

    if (DAT_00ae9f22 != 0) {
        if ((g_menu_choice_id & 0x80) == 0) {
            DAT_00ae9f13 = DAT_00ae9f13 | 4;
        }
        return 0;
    }
    bVar5 = 9;
    bVar6 = (DAT_00ae9f23 >> 1) - 4;
    bVar2 = g_ItemImageLookupTable[0x144];
    while (g_bItemMenuSelectedItemId < bVar2) {
        bVar5 = bVar5 - 1;
        bVar2 = g_ItemImageLookupTable[bVar5 + 0x13b];
    }
    if (g_ItemUseFunctions[bVar5](bVar6) == 0) {
        DAT_00ae9f22 = 1;
        set_message_display((unsigned short)(bVar5 + 0xf7), 0);
        DAT_00ae9f13 = DAT_00ae9f13 & 0xfb;
        return 0;
    }
    play_sfx(3, 6, 0);
    unsigned char* pcVar1 = (unsigned char*)(ITEM_SLOTS + 1 + (unsigned int)bVar6 * 2);
    unsigned char cVar3 = *pcVar1;
    if (cVar3 != 0) {
        *pcVar1 = cVar3 - 1;
        if (*(unsigned char*)(ITEM_SLOTS + (unsigned int)bVar6 * 2 + 1) == 0) {
            *(unsigned char*)(ITEM_SLOTS + (unsigned int)bVar6 * 2) = 0;
            rearrange_item_slots();
            menu_update_selected_item();
        }
    }
    DAT_00ae9f13 = DAT_00ae9f13 | 2;
    FUN_00454fd0(g_bItemMenuSelectedItemId, 0, item_name_left() - g_ScreenOffsetX, 0xba - g_ScreenOffsetY);
    return 0;
}

// (0x004013a0) - Item action: view the selected item's 3D model
static int menu_item_view_model(void)
{
    switch (DAT_00ae9f22) {
    case 0:
        DAT_00ae9f22 = 1;
        menu_load_item_model();
        // fall through
    case 1:
        break;
    case 2:
        goto view_case2;
    case 3:
        goto view_case3;
    default:
        return 0;
    }
    DAT_00ae9f28 = DAT_00ae9f28 + 1;
    if ((DAT_00ae9f28 & 8) == 0) {
        FUN_00454fd0(g_bItemMenuSelectedItemId, 0, item_name_left() - g_ScreenOffsetX, 0xba - g_ScreenOffsetY);
        return 0;
    }
    DAT_00ae9f22 = 2;
    DAT_00ae9f49 = 0;
view_case2:
    if (FUN_0044e1b0() != 0) {
        DAT_00ae9f22 = 3;
view_case3:
        DAT_00ae9f28 = DAT_00ae9f28 - 1;
        if (DAT_00ae9f28 == 0) {
            DAT_00ae9f13 = DAT_00ae9f13 | 4;
        }
        FUN_00454fd0(g_bItemMenuSelectedItemId, 0, item_name_left() - g_ScreenOffsetX, 0xba - g_ScreenOffsetY);
    }
    return 0;
}

// (0x00401470) - Item action: move / combine the selected item
static int menu_item_move_item(void)
{
    unsigned char bVar2;
    unsigned char bVar3;

    bVar2 = DAT_00ae9f23;
    if (DAT_00ae9f22 != 0) {
        bVar2 = DAT_00ae9f25;
    }
    bVar2 = (bVar2 >> 1) - 4;

    switch (DAT_00ae9f22) {
    case 0:
        DAT_00ae9f22 = 1;
        DAT_00ae9f26 = 0xff;
        DAT_00ae9f25 = DAT_00ae9f23;
        DAT_00ae9f29 = 0;
        DAT_00ae9f2a = 0;
        break;

    case 1:
        DAT_00ae9f29 = DAT_00ae9f29 + 4;
        DAT_00ae9f2a = DAT_00ae9f2a + 3;
        if (DAT_00ae9f29 == 0x10) {
            DAT_00ae9f22 = 2;
            DAT_00ae9f13 = DAT_00ae9f13 & 0xf7;
            goto move_case2;
        }
        break;

    case 2:
move_case2:
        if ((dpad_pressed_byte1() & 0x80) != 0) {
            DAT_00ae9f22 = 3;
            DAT_00ae9f26 = 0;
            play_sfx(3, 5, 0);
            goto move_case3;
        }
        if ((dpad_pressed_byte1() & 0x40) == 0) {
            if ((pad_held_byte1() & 0xf0) != 0) {
                if ((pad_held_byte1() & 0xa0) != 0) {
                    DAT_00ae9f25 = DAT_00ae9f25 ^ 2;
                }
                if ((pad_held_byte1() & 0x10) == 0) {
                    if (((pad_held_byte1() & 0x40) != 0) &&
                        (DAT_00ae9f25 = DAT_00ae9f25 + 4,
                         (unsigned int)g_totalInventorySlots * 2 + 6 < (unsigned int)DAT_00ae9f25)) {
                        DAT_00ae9f25 = DAT_00ae9f25 & 2;
                        DAT_00ae9f25 = DAT_00ae9f25 | 8;
                    }
                } else {
                    DAT_00ae9f25 = DAT_00ae9f25 - 4;
                    if ((DAT_00ae9f25 & 0xf8) == 0) {
                        DAT_00ae9f25 = DAT_00ae9f25 & 2;
                        DAT_00ae9f25 = DAT_00ae9f25 | g_totalInventorySlots * 2 + 4;
                    }
                }
                DAT_00ae9f26 = 0;
                play_sfx(3, 4, 0);
            }
        } else if (((DAT_00ae9f23 != DAT_00ae9f25) && (bVar2 < g_TotalHeldItems)) &&
                   (-1 < (char)g_ItemImageLookupTable[(unsigned int)g_bItemMenuSelectedItemId * 4 + 1])) {
            unsigned char uVar1 = (unsigned char)menu_item_check_combine();
            switch (uVar1) {
            case 1:
                DAT_00ae9f22 = 4;
                DAT_00ae9f12 = 1;
                set_message_display(0xf2, 0);
                break;
            case 2:
                DAT_00ae9f22 = 4;
                DAT_00ae9f12 = 4;
                set_message_display(0xf5, 0);
                break;
            case 3:
                DAT_00ae9f22 = 4;
                DAT_00ae9f12 = 5;
                set_message_display(0xf6, 0);
                break;
            case 4:
                DAT_00ae9f22 = 3;
                DAT_00ae9f13 = DAT_00ae9f13 | 8;
                menu_update_selected_item();
            }
            DAT_00ae9f26 = 0;
            break;
        }
        DAT_00ae9f26 = DAT_00ae9f26 - 1;
        break;

    case 3:
move_case3:
        DAT_00ae9f29 = DAT_00ae9f29 - 4;
        DAT_00ae9f2a = DAT_00ae9f2a - 3;
        if (DAT_00ae9f29 == 0) {
            if (DAT_00ae9f12 == 0) {
                if ((DAT_00ae9f13 & 8) == 0) {
                    DAT_00ae9f13 = DAT_00ae9f13 | 4;
                } else {
                    DAT_00ae9f13 = DAT_00ae9f13 | 2;
                }
            } else {
                DAT_00ae9f22 = 4;
                set_message_display((unsigned short)(DAT_00ae9f12 + 0xf1), 0);
            }
        }
        break;

    case 4:
        if ((g_menu_choice_id & 0x80) == 0) {
            switch (DAT_00ae9f12 - 1) {
            case 0:
                DAT_00ae9f22 = 3;
                break;
            case 1:
                DAT_00ae9f13 = DAT_00ae9f13 | 0x40;
                return 0;
            case 2:
                DAT_00ae9f13 = DAT_00ae9f13 | 2;
                break;
            case 3:
                menu_item_apply_combine();
                break;
            case 4:
                DAT_00ae9f22 = 2;
            }
            DAT_00ae9f12 = 0;
        }
    }

    if (DAT_00ae9f12 == 0) {
        bVar3 = g_bItemMenuSelectedItemId;
        if (DAT_00ae9f22 != 3) {
            if (g_TotalHeldItems <= bVar2) goto move_skip_name;
            bVar3 = *(unsigned char*)(ITEM_SLOTS + (unsigned int)bVar2 * 2);
        }
        FUN_00454fd0(bVar3, 0, item_name_left() - g_ScreenOffsetX, 0xba - g_ScreenOffsetY);
    }
move_skip_name:
    if ((DAT_00ae9f22 != 4) || (DAT_00ae9f29 != 0)) {
        // Draw the moving item cursor (4-part slide icon)
        // Original: switchD_0040149e::caseD_0 at 0x004014a5 - same draw.
        g_TextureDesc.flags = 0x01000040;
        g_TextureDesc.texU = 0x6c;
        g_TextureDesc.texturePage = 0x1c;
        g_TextureDesc.texV = 0x60;
        g_TextureDesc.clutX = 0;
        g_TextureDesc.clutY = 0x1e4;
        if ((DAT_00ae9f26 & 0x20) == 0) {
            g_TextureDesc.texU = 0x74;
        }
        g_TextureDesc.width = 4;
        g_TextureDesc.height = 4;
        g_TextureDesc.screenX = *(short*)((int)g_inventorySlotsPos + DAT_00ae9f25 * 2) + 0x12;
        g_TextureDesc.screenY = (*(short*)((int)g_inventorySlotsPos + DAT_00ae9f25 * 2 + 2) - (unsigned short)DAT_00ae9f2a) + 0xd;
        if ((DAT_00ae9f19 & 2) == 0) {
            g_TextureDesc.screenY = (*(short*)((int)g_inventorySlotsPos + DAT_00ae9f25 * 2 + 2) - (unsigned short)DAT_00ae9f2a) + 0x2b;
        }
        display_texture(&g_TextureDesc, 5, 0, 1);
        g_TextureDesc.texV = g_TextureDesc.texV + 8;
        g_TextureDesc.screenY = g_TextureDesc.screenY + (unsigned short)DAT_00ae9f2a * 2;
        display_texture(&g_TextureDesc, 5, 0, 1);
        g_TextureDesc.screenX = g_TextureDesc.screenX - (unsigned short)DAT_00ae9f29;
        g_TextureDesc.texV = g_TextureDesc.texV + 8;
        g_TextureDesc.screenY = *(short*)((int)g_inventorySlotsPos + DAT_00ae9f25 * 2 + 2) + 0xd;
        if ((DAT_00ae9f19 & 2) == 0) {
            g_TextureDesc.screenY = *(short*)((int)g_inventorySlotsPos + DAT_00ae9f25 * 2 + 2) + 0x2b;
        }
        display_texture(&g_TextureDesc, 5, 0, 1);
        g_TextureDesc.texV = g_TextureDesc.texV + 8;
        g_TextureDesc.screenX = g_TextureDesc.screenX + (unsigned short)DAT_00ae9f29 * 2;
        display_texture(&g_TextureDesc, 5, 0, 1);
    }
    return 0;
}

// (0x00420a70) - Draw the menu cursor (item slot or top tab)
static void menu_draw_cursor(void)
{
    g_TextureDesc.flags = 0x01000040;
    g_TextureDesc.texturePage = 0x1c;
    g_TextureDesc.clutX = 0;
    g_TextureDesc.clutY = 0x1e4;
    g_TextureDesc.screenX = *(short*)((int)g_inventorySlotsPos + DAT_00ae9f23 * 2);
    g_TextureDesc.screenY = *(short*)((int)g_inventorySlotsPos + DAT_00ae9f23 * 2 + 2);

    if ((DAT_00ae9f23 & 0xf8) != 0) {
        // Item slot cursor (40x30)
        g_TextureDesc.width = 0x28;
        g_TextureDesc.texU = 0x80;
        g_TextureDesc.texV = 0xe0;
        g_TextureDesc.height = 0x1e;
        if ((DAT_00ae9f18 & 0x20) == 0) {
            g_TextureDesc.texU = 0xa8;
        }
        g_rect.x = 0xce;
        if ((DAT_00ae9f19 & 2) == 0) {
            g_TextureDesc.screenY = g_TextureDesc.screenY + 0x1e;
        }
        display_texture(&g_TextureDesc, 5, 0, 1);
        g_TextureDesc.texV = 0x90;
        return;
    }

    // Top tab cursor (48x16)
    g_TextureDesc.width = 0x30;
    g_TextureDesc.texU = 0;
    g_TextureDesc.texV = 0x50;
    g_TextureDesc.height = 0x10;
    if (g_MainMenuState == 3) {
        g_TextureDesc.screenX = *(short*)((int)g_inventorySlotsPos + (DAT_00ae9f23 | 2) * 2);
    }
    display_texture(&g_TextureDesc, 0x19, 0, 1);
    g_TextureDesc.texV = 0x98;
}

// (0x00454fd0) - Draw the item name text at the given position
void FUN_00454fd0(int itemId, int mode, short x, short y)
{
    unsigned char bVar1;
    unsigned char* pbVar2;
    unsigned char* pbVar4;
    unsigned char bVar3;
    unsigned char item = (unsigned char)itemId;
    unsigned char modeB = (unsigned char)mode;

    if (item == 0) return;

    if ((modeB & 0x80) == 0) {
        bVar3 = modeB >> 4;
        if (bVar3 == 0) bVar3 = 2;
    } else {
        bVar3 = 0x1e;
    }

    // Game-text glyph width: 8px (fontus.tim) or 14px (FONT.TIM), both in an
    // 18-column grid — the Japanese draw_item_name (0x004912c0) is this same
    // loop with texU=(b%18)*14 and a +14 advance.
    const int glyphW = (GetAssetVersion() != 0) ? 14 : 8;

    g_TextureDesc.flags = 0x40;
    g_TextureDesc.height = 0xe;
    g_TextureDesc.clutX = 0x100;
    g_TextureDesc.clutY = (modeB & 0xf) + 0x1e0;
    g_TextureDesc.screenX = x;
    g_TextureDesc.screenY = y;
    g_TextureDesc.width = glyphW;
    g_TextureDesc.colorMulR = 0x80;
    g_TextureDesc.colorMulG = 0x80;
    g_TextureDesc.colorMulB = 0x80;
    g_TextureDesc.pivotX = 0;
    unk_00be1180 = 0;
    g_TextureDesc.pivotY = 0;

    pbVar2 = message_item_name_lookup(item);
    if (*pbVar2 == 7) return;

    do {
        bVar1 = *pbVar2;
        if (bVar1 == 0xf8) {
            g_TextureDesc.texturePage = 0x1e;
            pbVar2 = pbVar2 + 1;
            bVar1 = *pbVar2 / 0x12 + 0xf;
        } else if (bVar1 == 0xf9) {
            pbVar2 = pbVar2 + 1;
            g_TextureDesc.texturePage = 0x1f;
            bVar1 = *pbVar2 / 0x12;
        } else if (bVar1 == 0xfa) {
            pbVar2 = pbVar2 + 1;
            g_TextureDesc.texturePage = 0x1f;
            bVar1 = *pbVar2 / 0x12 + 0xe;
        } else {
            g_TextureDesc.texturePage = 0x1e;
            bVar1 = *pbVar2 / 0x12 + 2;
        }
        pbVar4 = pbVar2 + 1;
        g_TextureDesc.texV = bVar1 * 0xe;
        g_TextureDesc.texU = (unsigned char)(*pbVar2 % 0x12 * glyphW);
        AddTintSprite(&g_TextureDesc, (unsigned short)bVar3);
        g_TextureDesc.screenX = g_TextureDesc.screenX + glyphW;
        pbVar2 = pbVar4;
    } while (*pbVar4 != 7);
}

// (0x00420bb0) - Item action submenu: use/view/move options, animation and
// item name box display
static void menu_item_submenu(void)
{
    unsigned char bVar1;
    unsigned short uVar2;
    unsigned char bVar5;

    switch (DAT_00ae9f21) {
    case 0:
        DAT_00ae9f21 = 1;
        DAT_00ae9f27 = 0x80;
        DAT_00ae9f28 = 0;
        // fall through
    case 1:
        DAT_00ae9f27 = DAT_00ae9f27 + 1;
        if ((DAT_00ae9f27 & 8) != 0) {
            DAT_00ae9f21 = 2;
            DAT_00ae9f13 = DAT_00ae9f13 & 0xfd;
        }
        break;

    case 2:
        if ((dpad_pressed_byte1() & 0x80) == 0) {
            FUN_00454fd0(g_bItemMenuSelectedItemId, 0, item_name_left() - g_ScreenOffsetX, 0xba - g_ScreenOffsetY);
            if ((dpad_pressed_byte1() & 0x40) != 0) {
                if (DAT_00ae9f24 == 0) {
                    if ((g_bItemMenuSelectedItemId < ITEM_CLIP) || (ITEM_NON_INFINITE_MAX < g_bItemMenuSelectedItemId)) {
                        menu_item_toggle_equip();
                        DAT_00ae9f21 = 3;
                        DAT_00ae9f27 = DAT_00ae9f27 & 0x7f;
                        play_sfx(3, 6, 0);
                    } else {
                        DAT_00ae9f21 = 4;
                        DAT_00ae9f22 = 0;
                    }
                } else if ((DAT_00ae9f24 != 0) && (DAT_00ae9f24 < 3)) {
                    DAT_00ae9f21 = 4;
                    DAT_00ae9f22 = 0;
                    DAT_00ae9f13 = DAT_00ae9f13 & 0xfb;
                    play_sfx(3, 6, 0);
                }
                goto submenu_default;
            }
            if ((pad_held_byte1() & 0x50) != 0) {
                if ((pad_held_byte1() & 0x10) == 0) {
                    if (DAT_00ae9f24 < 2) {
                        DAT_00ae9f24 = DAT_00ae9f24 + 1;
                    }
                } else if (DAT_00ae9f24 != 0) {
                    DAT_00ae9f24 = DAT_00ae9f24 - 1;
                }
                play_sfx(3, 4, 0);
            }
            if ((DAT_00ae9f13 & 2) == 0) goto submenu_default;
            DAT_00ae9f27 = DAT_00ae9f27 & 0x7f;
        }
        DAT_00ae9f21 = 3;
        play_sfx(3, 5, 0);

    case 3:
        DAT_00ae9f27 = DAT_00ae9f27 - 1;
        if ((DAT_00ae9f27 & 0x7f) == 0) {
            DAT_00ae9f20 = 1;
            menu_update_selected_item();
            if ((DAT_00ae9f13 & 0x80) != 0) {
                DAT_00ae9f13 = DAT_00ae9f13 | 1;
                return;
            }
        }
        break;

    case 4:
        g_ItemActionFunctions[DAT_00ae9f24]();
        if ((DAT_00ae9f13 & 0x40) != 0) {
            return;
        }
        if ((DAT_00ae9f13 & 2) == 0) {
            if ((DAT_00ae9f13 & 4) != 0) {
                DAT_00ae9f21 = 2;
            }
        } else {
            DAT_00ae9f21 = 3;
            DAT_00ae9f27 = DAT_00ae9f27 & 0x7f;
        }
        // 0x00420d70 - case 4 falls into `default`, which JUMPS PAST the item
        // name draw below. While an item action owns the screen (the examine
        // description, a use prompt) the 0xba line belongs to the message, so
        // drawing the name here painted it on top of the description text.
    default:
        goto submenu_default;
    }

    FUN_00454fd0(g_bItemMenuSelectedItemId, 0, item_name_left() - g_ScreenOffsetX, 0xba - g_ScreenOffsetY);
submenu_default:
    // Draw the item action submenu box with the selected option highlighted
    if ((DAT_00ae9f27 != 0) && ((DAT_00ae9f28 & 8) == 0)) {
        g_TextureDesc.flags = 0x01000040;
        g_TextureDesc.texturePage = 0x1c;
        g_TextureDesc.clutX = 0;
        g_TextureDesc.clutY = 0x1e4;
        g_TextureDesc.screenX = 0x90;
        if (((DAT_00ae9f21 & 1) == 0) && (DAT_00ae9f28 == 0)) {
            g_TextureDesc.texV = 0x60;
            g_TextureDesc.height = 0x18;
            g_TextureDesc.screenY = (unsigned short)DAT_00ae9f24 * 0x18 + 0x39;
            g_TextureDesc.width = 0x30;
            g_TextureDesc.texU = 0x30;
            display_texture(&g_TextureDesc, 0x23, 0, 1);
        }
        g_TextureDesc.screenY = 0x39;
        g_TextureDesc.texU = 0x30;
        g_TextureDesc.texV = 0;
        if (DAT_00ae9f1c == 0) {
            g_TextureDesc.texV = 0x18;
        }
        if ((DAT_00ae9f27 & 0x80) == 0) {
            g_TextureDesc.screenX = g_TextureDesc.screenX + (8 - (unsigned short)DAT_00ae9f27) * 6;
            g_TextureDesc.screenY = (0x1b - (unsigned short)DAT_00ae9f27) * 3;
            g_TextureDesc.texU = (0x10 - DAT_00ae9f27) * 6;
            g_TextureDesc.texV = g_TextureDesc.texV + (8 - DAT_00ae9f27) * 3;
        }
        g_TextureDesc.clutY = 0x1e4;
        uVar2 = 0;
        g_TextureDesc.width = (DAT_00ae9f27 & 0x7f) * 6;
        g_TextureDesc.height = (DAT_00ae9f27 & 0x7f) * 3;
        do {
            if (((DAT_00ae9f27 & 0x80) != 0) && (DAT_00ae9f28 != 0)) {
                if ((short)((DAT_00ae9f28 - uVar2) * 8) < 0) {
                    g_TextureDesc.screenX = 0x90;
                } else {
                    g_TextureDesc.screenX = (DAT_00ae9f28 - uVar2) * 8 + 0x90;
                }
            }
            display_texture(&g_TextureDesc, 0x23, 0, 1);
            bVar1 = g_TextureDesc.texV;
            g_TextureDesc.texV = g_TextureDesc.texV + 0x18;
            if (((char)uVar2 == 0) && (DAT_00ae9f1c != 0)) {
                g_TextureDesc.texV = bVar1 + 0x30;
            }
            g_TextureDesc.screenY = g_TextureDesc.screenY + 0x18;
            bVar5 = (unsigned char)uVar2 + 1;
            uVar2 = (unsigned short)bVar5;
        } while (bVar5 < 3);
    }
}

// Read-only presentation hints for ports. Overview tabs use the inventory
// frame; document pages keep one proportional coordinate system.
bool MenuUsesItemBoxLayout()
{
    return DAT_00ae9f10==2;
}

int MenuMapFilesLayout()
{
    if(DAT_00ae9f10==5)return 1; // map-item display/zoom in the menu frame
    if(g_MainMenuState!=1||DAT_00ae9f20!=2)return 0;
    if(DAT_00ae9f23==0)return 1; // overview and zoom share the menu frame
    if(DAT_00ae9f23==2)return FILE_STATE==0?1:2;
    return 0;
}

bool MenuUsesInventoryLayout()
{
    const bool activeTopTab=g_MainMenuState==1 && DAT_00ae9f20==2 && (DAT_00ae9f23&0xf8)==0;
    return DAT_00ae9f10!=2 && DAT_00ae9f10!=5 && !activeTopTab;
}

// (0x00420880) - Main menu input: cursor navigation, tab and item selection
static void menu_handle_input(void)
{
    if (DAT_00ae9f20 == 0) {
        DAT_00ae9f20 = 1;
        DAT_00ae9f18 = 0;
        menu_update_selected_item();
    } else if (DAT_00ae9f20 == 2) {
        if ((DAT_00ae9f23 & 0xf8) == 0) {
            // Cursor on a top tab: run the tab action
            g_MenuTabActions[DAT_00ae9f23 >> 1]();
        } else {
            menu_item_submenu();
        }
        if ((DAT_00ae9f13 & 0x41) != 0) {
            return;
        }
        if (g_MainMenuState == 7) {
            return;
        }
        // 0x00420a2b - the submenu path draws the cursor and returns; it does
        // NOT fall into the item-name draw at input_draw. Once a submenu owns
        // the screen the 0xba line belongs to whatever it puts there (the
        // examine description, a use prompt), and menu_item_submenu itself
        // draws the name in the states that still want it.
        menu_draw_cursor();
        return;
    } else if (DAT_00ae9f20 != 1) {
        return;
    }

    if ((dpad_pressed_byte1() & 0x40) == 0) {
        // Navigation with the D-pad (held for repeat)
        if ((pad_held_byte1() & 0xf0) != 0) {
            if ((pad_held_byte1() & 0xa0) != 0) {
                DAT_00ae9f23 = DAT_00ae9f23 ^ 2;
            }
            if ((pad_held_byte1() & 0x10) == 0) {
                if (((pad_held_byte1() & 0x40) != 0) &&
                    (DAT_00ae9f23 = DAT_00ae9f23 + 4,
                     (unsigned int)g_totalInventorySlots * 2 + 6 < (unsigned int)DAT_00ae9f23)) {
                    DAT_00ae9f23 = DAT_00ae9f23 & 2;
                }
            } else if ((DAT_00ae9f23 & 0xfc) == 0) {
                DAT_00ae9f23 = DAT_00ae9f23 + g_totalInventorySlots * 2 + 4;
            } else {
                DAT_00ae9f23 = DAT_00ae9f23 - 4;
            }
            menu_update_selected_item();
            DAT_00ae9f18 = 0;
            play_sfx(3, 4, 0);
            goto input_draw;
        }
        DAT_00ae9f18 = DAT_00ae9f18 - 1;
    } else {
        // Confirm pressed
        if ((DAT_00ae9f23 & 0xf8) == 0) {
            if (DAT_00ae9f23 == 4) {
                // Radio tab pre-check: without the radio item, show it as a model
                if (DAT_00ae9f1f == 0) goto input_blink;
                if (Flg_ck((int)g_itemUseFlags, 0x3f) == 0) {
                    DAT_00ae9f49 = 0;
                    g_bItemMenuSelectedItemId = ITEM_COMM_RADIO;
                    menu_load_item_model();
                }
            }
        } else {
            if (g_bItemMenuSelectedItemId == 0) goto input_blink;
            if ((g_bItemMenuSelectedItemId < ITEM_CLIP) || (ITEM_NON_INFINITE_MAX < g_bItemMenuSelectedItemId)) {
                DAT_00ae9f1c = 1;
            } else {
                DAT_00ae9f1c = 0;
            }
        }
        DAT_00ae9f20 = 2;
        DAT_00ae9f21 = 0;
        DAT_00ae9f24 = 0;
        DAT_00ae9f18 = 0;
        play_sfx(3, 6, 0);
    }
input_draw:
    menu_draw_cursor();
    FUN_00454fd0(g_bItemMenuSelectedItemId, 0, item_name_left() - g_ScreenOffsetX, 0xba - g_ScreenOffsetY);
    return;

input_blink:
    DAT_00ae9f18 = DAT_00ae9f18 - 1;
    goto input_draw;
}

// (0x00487100) - Map tab state machine (returns non-zero when exiting)
static int menu_map_state_machine(void)
{
    // Cancel during the shrink/close (zoom 7+) drops back to the browser
    if (((dpad_pressed_byte1() & 0x80) != 0) && (7 <= MAP_ZOOM_STATE)) {
        MAP_STATE = 0;
    }
    switch (MAP_STATE) {
    case 0:
        // Browser: load the map0d..g textures and navigate
        if ((dpad_pressed_byte1() & 0x80) != 0) {
            *(unsigned int*)&DAT_00ac9890[0] = 0;
            play_sfx(3, 5, 0);
            return -1;
        }
        map_tab_update(&DAT_00ac9890[0]);
        if ((dpad_pressed_byte1() & 0x40) != 0) {
            // Cross: enter the full display (substate 0 re-inits the load)
            *(unsigned int*)&DAT_00ac9890[0] = 1;
            return 0;
        }
        break;
    case 1:
        // Full map display
        map_display_update(&DAT_00ac9890[0]);
        if (((dpad_pressed_byte1() & 0x40) != 0) && ((MAP_ZOOM_STATE == 0) || (6 < MAP_ZOOM_STATE))) {
            // Re-zoom from the shrink states: 8 -> 3, 9 -> 2
            if (MAP_ZOOM_STATE == 8) MAP_ZOOM_STATE = 3;
            if (MAP_ZOOM_STATE == 9) MAP_ZOOM_STATE = 2;
            play_sfx(3, 9, 0);
        }
        if ((dpad_pressed_byte1() & 0x80) != 0) {
            // Cancel: start the shrink (zoom state 7) and leave the tab
            MAP_ZOOM_STATE = 7;
            play_sfx(3, 5, 0);
            return -1;
        }
        break;
    default:
        // Other states (e.g. the display's close-path state 6): stay
        break;
    }
    return 0;
}

// (0x00420fd0) - Top tab: Map
static void menu_tab_map(void)
{
    if (menu_map_state_machine() != 0) {
        DAT_00ae9f20 = 1;
        DAT_00ae9f1e = DAT_00ae9f1e & 0x80;
    }
}

// (0x00481980) - File tab state machine (returns non-zero when exiting)
//
// FILE_STATE 0 = file book selector (FUN_00481ab0 drives the book render and
// input); confirming a file (byte1 & 0x40 with a slot selected) sets
// FILE_MODE = 1, which makes the selector's renderer fade to white and set
// FILE_STATE = 1. FILE_STATE 1 = file reading mode (pickup_list_advance
// shows the filem pages); Square (byte1 & 0x80) cancels it and confirming on
// the last page (FILE_SAVEFLAG) closes it too.
// FILE_BUSY is only an INPUT LOCK, never an exit condition: the ONLY thing
// that closes the tab is FILE_EXITFLAG (state[8]), raised by the selector's
// cancel slide-out (FUN_00481d00 case 2) - verified against 0x00481a93:
//   cmp byte [0xac98b8],0 / je -> xor eax,eax / ret
// An earlier port also returned -1 whenever FILE_BUSY was set, which broke
// three things at once:
//   * menu_init_status_screen sets state[9] = 1 when the pause menu opens, so
//     the FIRST frame of the file tab returned -1 and bounced straight back to
//     the tab row (the book only appeared on a second confirm press);
//   * every book change sets state[9] = 1 to lock input during the slide, so
//     pressing left/right closed the tab instead of turning the page;
//   * confirming a file sets state[9] = 1 too.
static int menu_file_state_machine(void)
{
    if (FILE_STATE == 0) {
        // Cancel is checked BEFORE the selector runs (0x004819a0 precedes the
        // call at 0x004819d9). There is no conflict with the book-change keys:
        // cancel is bit 0x80 of the REMAPPED dpad byte 1 (raw byte 0 bit 6 via
        // g_padRemapSubTable0[15]), while the book keys are raw D-pad bits.
        if ((FILE_BUSY == 0) && ((dpad_pressed_byte1() & 0x80) != 0)) {
            if ((FILE_MODE != 1) && (FILE_MODE != 2)) {
                play_sfx(3, 5, 0);
            }
            FILE_REQUEST = 2;
            FILE_BUSY = 1;
        }
        if (GetAssetVersion() != 0) {
            jpn_file_book_advance(&DAT_00ac98b0[0]);
        } else {
            FUN_00481ab0(&DAT_00ac98b0[0]);
        }
        if ((FILE_BUSY == 0) && ((dpad_pressed_byte1() & 0x40) != 0) &&
            (FILE_REQUEST == 1) && (FILE_SLOT != 0xff)) {
            FILE_MODE = 1;
            FILE_BUSY = 1;
            play_sfx(3, 6, 0);
        }
    } else if (FILE_STATE == 1) {
        file_reader_advance(&DAT_00ac98b0[0]);
        if (FILE_BUSY == 0) {
            // Cancel and confirm-on-last-page both start the same slide-out
            // (0x00481a40 / 0x00481a65 - the second test is NOT re-gated on
            // FILE_BUSY in the original either).
            if ((dpad_pressed_byte1() & 0x80) != 0) {
                FILE_REQUEST = 0xd;
                FILE_BUSY = 1;
                play_sfx(3, 5, 0);
            }
            if (((dpad_pressed_byte1() & 0x40) != 0) && (FILE_SAVEFLAG == 1)) {
                FILE_REQUEST = 0xd;
                FILE_BUSY = 1;
                play_sfx(3, 5, 0);
            }
        }
    }

    if (FILE_EXITFLAG != 0) {
        FILE_EXITFLAG = 0;
        return -1;
    }
    return 0;
}

// (0x00420ff0) - Top tab: File (file book / document reader)
static void menu_tab_file(void)
{
    if (menu_file_state_machine() != 0) {
        DAT_00ae9f20 = 1;
        DAT_00ae9f1e = DAT_00ae9f1e & 0x80;
    }
}

// (0x00421010) - Top tab: Radio
static void menu_tab_radio(void)
{
    if (Flg_ck((int)g_itemUseFlags, 0x3f) != 0) {
        DAT_00ae9f20 = 1;
        g_usedItemId = 0x4d;
        DAT_00ae9f13 = DAT_00ae9f13 | 0x81;
        play_sfx(3, 7, 0);
        return;
    }
    if (FUN_0044e1b0() != 0) {
        DAT_00ae9f20 = 1;
        g_bItemMenuSelectedItemId = 0;
    }
}

// (0x00421060) - Top tab: Exit
static void menu_tab_exit(void)
{
    DAT_00ae9f20 = 1;
    menu_exit_cleanup();
}

// The item-view file name for an image type (byte 0 of the item's
// g_ItemImageLookupTable record), i.e. g_ItemModelFileNames[imageType].
//
// The USA table holds 75 records (0..0x4A) and every USA image type is inside
// it. The DC's lookup reaches 0x4B - the Beretta M92FS custom, whose view is
// one of the DC's new ITEM_M2 files - and renames two more (the MOON CREST
// halves). dc_item_model_name supplies those; anything else the DC's lookup
// could produce past the table has no view, and the empty name makes LoadFile
// fail so the viewer stays inert instead of opening an unrelated model.
static const unsigned char* item_model_file_name(unsigned char imageType)
{
    if (g_bDcMode) {
        const unsigned char* dcName = dc_item_model_name(imageType);
        if (dcName != 0) {
            return dcName;
        }
        if ((int)imageType >= ITEM_MODEL_NAME_COUNT) {
            return g_ItemModelFileNames[0];
        }
    }
    return (const unsigned char*)g_ItemModelFileNames + imageType * 8;
}

// (0x00464770) - Load the item's 3D model TIM for the menu display
static void menu_load_item_model(void)
{
    unsigned char bVar2;
    unsigned int uVar3;
    int iVar4;
    unsigned int uVar5;
    char* pcVar6;
    char* pcVar7;
    char* pcVar8;

    if (g_bItemMenuSelectedItemId == SUBMENU_STATE_ID) {
        return;
    }
    SUBMENU_STATE_ID = g_bItemMenuSelectedItemId;
    if (g_bItemMenuSelectedItemId < ITEM_INGRAM) {
        bVar2 = g_ItemImageLookupTable[(unsigned int)g_bItemMenuSelectedItemId * 4];
        if (((short)(char)DAT_00ae9f1e & 0xff7f) ==
            (unsigned short)(unsigned char)g_ItemImageLookupTable[(unsigned int)g_bItemMenuSelectedItemId * 4]) {
            SUBMENU_STATE_ID = g_bItemMenuSelectedItemId;
            return;
        }
    } else {
        DAT_00ae9f1e = 0;
        bVar2 = DAT_00ae9f1e;
    }
    DAT_00ae9f1e = bVar2;

    // Build the path: data root + "item_m2/" + file name + ".ivm"
    // (the original's "./usa/item_m2/" strings are rooted at the game data
    // directory, which in this port expands to GAME_DATA_ROOT).
    strcpy(DAT_008e1cb0, GAME_DATA_ROOT);
    strcat(DAT_008e1cb0, "item_m2/");
    if (g_bItemMenuSelectedItemId == ITEM_INGRAM) {
        pcVar6 = (char*)g_ItemModelFileNameING;
    } else if (g_bItemMenuSelectedItemId == ITEM_MINIMI) {
        pcVar6 = (char*)g_ItemModelFileNameMINI;
    } else {
        pcVar6 = (char*)item_model_file_name(DAT_00ae9f1e);
    }
    strcat(DAT_008e1cb0, pcVar6);
    strcat(DAT_008e1cb0, (const char*)g_ItemModelExtIVM);

    LoadFile(DAT_008e1cb0, g_TimImageBuffer__bitmap, 0x20);
    g_TextureCurrentPage = 0x1c;
    g_TextureBankID = 0x15;
    FUN_00484420(g_TimImageBuffer__bitmap, (void*)0xd024cc);
    DAT_00ae9f1e = DAT_00ae9f1e | 0x80;
}

// (0x004387e0) - Get EKG wave data index
// Scans EKG wave data table counting entries <= current EKG scroll position (DAT_00ae9f38 - 0x35)
static char FUN_004387e0(const unsigned char* data)
{
    char index = 0;
    unsigned char threshold = (unsigned char)((char)DAT_00ae9f38 - 0x35);
    while (*data <= threshold) {
        index++;
        data++;
    }
    return index;
}

// (0x00437fd0) - Draw health bar with EKG animation
// 0x00437fd0
static void menu_draw_health_bar(void)
{
    unsigned char bVar2;
    unsigned int uVar3;
    int iVar4;
    unsigned char bVar5;
    unsigned char bVar7;
    short sVar8;
    char* pcVar1;
    char* pcVar9;

    // EKG primary line data accessors (g_EkgPrimaryLine at 0x00be1198)
    #define EKG_P_FLAGS  (*(unsigned int*)&g_EkgPrimaryLine[0])
    #define EKG_P_X0     (*(short*)&g_EkgPrimaryLine[4])
    #define EKG_P_Y0     (*(short*)&g_EkgPrimaryLine[6])
    #define EKG_P_X1     (*(short*)&g_EkgPrimaryLine[8])
    #define EKG_P_Y1     (*(short*)&g_EkgPrimaryLine[10])
    #define EKG_P_R      (g_EkgPrimaryLine[12])
    #define EKG_P_G      (g_EkgPrimaryLine[13])
    #define EKG_P_B      (g_EkgPrimaryLine[14])

    // EKG secondary line data accessors (g_EkgSecondaryLine at 0x00be1184)
    #define EKG_S_X0     (*(short*)&g_EkgSecondaryLine[4])
    #define EKG_S_Y0     (*(short*)&g_EkgSecondaryLine[6])
    #define EKG_S_X1     (*(short*)&g_EkgSecondaryLine[8])
    #define EKG_S_Y1     (*(short*)&g_EkgSecondaryLine[10])
    #define EKG_S_R      (g_EkgSecondaryLine[12])
    #define EKG_S_G      (g_EkgSecondaryLine[13])
    #define EKG_S_B      (g_EkgSecondaryLine[14])
    #define EKG_S_R2     (g_EkgSecondaryLine[15])
    #define EKG_S_G2     (g_EkgSecondaryLine[16])
    #define EKG_S_B2     (g_EkgSecondaryLine[17])

    switch (DAT_00ae9f2e) {
    case 0:
        DAT_00ae9f2e = 1;
        DAT_00ae9f38 = 0x84;
        DAT_00ae9f3c = 0x84;
        // fall through
    case 1:
        DAT_00ae9f38 = DAT_00ae9f38 + 1;
        *(unsigned int*)&g_EkgPrimaryLine[0] = 0;
        unk_00be1180 = 0;
        if (0x83 < DAT_00ae9f38) {
            DAT_00ae9f38 = 0x35;
            if (g_playerEntity.health == 0) {
                DAT_00ae9f2f = 0;
            } else {
                DAT_00ae9f2f = (unsigned char)((g_playerEntity.health - 1) /
                    (int)(unsigned int)(g_playerEntity.maxHealth >> 2));
                // DC-only clamp (SLUS_005.51 0x80054708). maxHealth >> 2 loses
                // the remainder, so a max that is not a multiple of 4 lets a
                // full bar divide out to 4 - the POISONED row of the colour
                // table and the poisoned face. It bites on exactly the two
                // values the DC modes introduce: Jill ADVANCED 70 (69/17) and
                // Jill TRAINING 150 (149/37). Harmless for the USA values
                // (140, 96, 88 all land on 3), which is why the original PC
                // build has no clamp here.
                if (DAT_00ae9f2f > 3) {
                    DAT_00ae9f2f = 3;
                }
            }
            uVar3 = (unsigned int)DAT_00ae9f2f;
            DAT_00ae9f30 = DAT_004b92c8[uVar3 * 3];
            DAT_00ae9f31 = DAT_004b92c8[uVar3 * 3 + 1];
            DAT_00ae9f32 = DAT_004b92c8[uVar3 * 3 + 2];
            if (((g_playerEntity.healthStatusFlags & 0x22) != 0) &&
               (uVar3 = rand(), (uVar3 & 1) != 0)) {
                DAT_00ae9f2f = 4;
            }
            iVar4 = rand();
            bVar2 = (unsigned char)iVar4 & 3;
            if ((DAT_00ae9f48 & 3) == bVar2) {
                bVar2 = (bVar2 + 1) & 3;
            }
            DAT_00ae9f48 = bVar2 | DAT_00ae9f2f << 2;
            DAT_00ae9f40 = (char*)PTR_DAT_004b9278[DAT_00ae9f48];
        }
        DAT_00ae9f36 = DAT_00ae9f38 + 0xf;
        DAT_00ae9f3c = DAT_00ae9f3c + 1;
        if (0x83 < DAT_00ae9f3c) {
            DAT_00ae9f3c = DAT_00ae9f38 - 0x1f;
            DAT_00ae9f44 = DAT_00ae9f40;
            DAT_00ae9f33 = DAT_00ae9f30;
            DAT_00ae9f34 = DAT_00ae9f31;
            DAT_00ae9f35 = DAT_00ae9f32;
        }
        DAT_00ae9f3a = DAT_00ae9f3c + 0x1f;
        if ((g_playerEntity.healthStatusFlags & 0x22) != 0) {
            DAT_00ae9f2f = 4;
        }

        // Set up texture for health bar face icon
        g_TextureDesc.flags = 0x01000040;
        g_TextureDesc.texU = 0x60;
        g_TextureDesc.screenX = 100;
        g_TextureDesc.screenY = 0xa8;
        g_TextureDesc.width = 0x20;
        g_TextureDesc.height = 8;
        g_TextureDesc.clutX = 0;
        g_TextureDesc.clutY = 0x1e4;
        g_TextureDesc.texturePage = 0x1c;

        // Draw face icon based on health status
        switch (DAT_00ae9f2f) {
        case 0:
            bVar2 = FUN_004387e0((unsigned char*)&DAT_004b92e0);
            if (DAT_004b92f8[bVar2] != 0) {
                g_TextureDesc.texV = DAT_004b92f8[bVar2] * 8 + 0x30;
                goto LAB_004382cb;
            }
            break;
        case 1:
            bVar2 = FUN_004387e0((unsigned char*)&DAT_004b92d8);
            if (DAT_004b92f0[bVar2] != 0) {
                g_TextureDesc.texV = DAT_004b92f0[bVar2] * 8 + 0x18;
                goto LAB_004382cb;
            }
            break;
        case 2:
        case 3:
            g_TextureDesc.texV = 0;
LAB_004382cb:
            display_texture(&g_TextureDesc, 10, 0, 1);
            break;
        case 4:
            bVar2 = FUN_004387e0((unsigned char*)&DAT_004b92e0);
            if (DAT_004b92f8[bVar2] != 0) {
                g_TextureDesc.texV = DAT_004b92f8[bVar2] * 8 + 0x40;
                goto LAB_004382cb;
            }
        }

        // Draw primary EKG line (FUN_00470c60, SpriteRenderer.cpp)
        if ((0x53 < DAT_00ae9f36) && (DAT_00ae9f38 < 0x84)) {
            EKG_P_R = DAT_00ae9f30;
            EKG_P_G = DAT_00ae9f31;
            EKG_P_B = DAT_00ae9f32;
            EKG_P_X0 = DAT_00ae9f36;
            if (0x83 < DAT_00ae9f36) {
                EKG_P_X0 = 0x83;
            }
            sVar8 = DAT_00ae9f38;
            if (DAT_00ae9f38 < 0x54) {
                sVar8 = 0x54;
            }
            // Walk wave data to find segment at current position
            pcVar9 = DAT_00ae9f40;
            char cVar6 = *pcVar9;
            while ((int)EKG_P_X0 < cVar6 + 0x54) {
                pcVar1 = pcVar9 + 4;
                pcVar9 = pcVar9 + 4;
                cVar6 = *pcVar1;
            }
            EKG_P_Y1 = (short)(char)pcVar9[2] + 0xa2;
            EKG_P_Y0 = ((EKG_P_X0 - (short)(unsigned char)*pcVar9) + -0x54) *
                       (short)(char)pcVar9[3] + (short)(char)pcVar9[2] + 0xa2;
            EKG_P_X1 = (short)(unsigned char)*pcVar9 + 0x54;
            if (sVar8 < EKG_P_X1) {
                FUN_00470c60(g_EkgPrimaryLine, 10);
                if (*pcVar9 == 0) goto LAB_0043849d;
                cVar6 = pcVar9[4];
                pcVar9 = pcVar9 + 4;
                while ((int)sVar8 < cVar6 + 0x54) {
                    EKG_P_X0 = (short)(unsigned char)pcVar9[1] + 0x54;
                    EKG_P_X1 = (short)(unsigned char)*pcVar9 + 0x54;
                    EKG_P_Y0 = (short)(char)pcVar9[-2] + 0xa2;
                    EKG_P_Y1 = (short)(char)pcVar9[2] + 0xa2;
                    FUN_00470c60(g_EkgPrimaryLine, 10);
                    cVar6 = pcVar9[4];
                    pcVar9 = pcVar9 + 4;
                }
                EKG_P_X0 = (short)(unsigned char)pcVar9[1] + 0x54;
                EKG_P_Y0 = (short)(char)pcVar9[-2] + 0xa2;
                EKG_P_Y1 = ((sVar8 - (short)(unsigned char)*pcVar9) + -0x54) *
                           (short)(char)pcVar9[3] + (short)(char)pcVar9[2] + 0xa2;
            }
            EKG_P_X1 = sVar8;
            FUN_00470c60(g_EkgPrimaryLine, 10);
        }
LAB_0043849d:
        // Draw secondary EKG line with gradient (FUN_00438800)
        if ((0x53 < DAT_00ae9f3a) && (DAT_00ae9f3c < 0x84)) {
            bVar7 = DAT_00ae9f33 >> 5;
            bVar2 = DAT_00ae9f34 >> 5;
            bVar5 = DAT_00ae9f35 >> 5;
            if (DAT_00ae9f3a < 0x84) {
                EKG_S_R = DAT_00ae9f33;
                EKG_S_G = DAT_00ae9f34;
                EKG_S_B = DAT_00ae9f35;
                EKG_S_X0 = DAT_00ae9f3a;
            } else {
                char cFade = (char)(-0x7d - (char)DAT_00ae9f3a);
                EKG_S_R = DAT_00ae9f33 + bVar7 * cFade;
                EKG_S_G = DAT_00ae9f34 + bVar2 * cFade;
                EKG_S_B = DAT_00ae9f35 + bVar5 * cFade;
                EKG_S_X0 = 0x83;
            }
            sVar8 = DAT_00ae9f3c;
            if (DAT_00ae9f3c < 0x54) {
                sVar8 = 0x54;
            }
            pcVar9 = DAT_00ae9f44;
            char cVar6 = *pcVar9;
            while ((int)EKG_S_X0 < cVar6 + 0x54) {
                pcVar1 = pcVar9 + 4;
                pcVar9 = pcVar9 + 4;
                cVar6 = *pcVar1;
            }
            EKG_S_Y1 = (short)(char)pcVar9[2] + 0xa2;
            EKG_S_Y0 = ((EKG_S_X0 - (short)(unsigned char)*pcVar9) + -0x54) *
                       (short)(char)pcVar9[3] + (short)(char)pcVar9[2] + 0xa2;
            EKG_S_X1 = (short)(unsigned char)*pcVar9 + 0x54;
            if (EKG_S_X1 <= sVar8) {
                EKG_S_X1 = sVar8;
                FUN_00438800(bVar7, bVar2, bVar5);
            } else {
                FUN_00438800(bVar7, bVar2, bVar5);
                if (*pcVar9 != 0) {
                    cVar6 = pcVar9[4];
                    pcVar9 = pcVar9 + 4;
                    while ((int)sVar8 < cVar6 + 0x54) {
                        EKG_S_X0 = (short)(unsigned char)pcVar9[1] + 0x54;
                        EKG_S_X1 = (short)(unsigned char)*pcVar9 + 0x54;
                        EKG_S_Y0 = (short)(char)pcVar9[-2] + 0xa2;
                        EKG_S_Y1 = (short)(char)pcVar9[2] + 0xa2;
                        FUN_00438800(bVar7, bVar2, bVar5);
                        cVar6 = pcVar9[4];
                        pcVar9 = pcVar9 + 4;
                    }
                    EKG_S_X0 = (short)(unsigned char)pcVar9[1] + 0x54;
                    EKG_S_Y0 = (short)(char)pcVar9[-2] + 0xa2;
                    EKG_S_Y1 = ((sVar8 - (short)(unsigned char)*pcVar9) + -0x54) *
                               (short)(char)pcVar9[3] + (short)(char)pcVar9[2] + 0xa2;
                    EKG_S_X1 = sVar8;
                    FUN_00438800(bVar7, bVar2, bVar5);
                }
            }
        }
        break;

    case 2:
        DAT_00ae9f2e = 3;
        DAT_00ae9f36 = 0xaf;
        // fall through
    case 3:
        EKG_P_X1 = 0x83;
        EKG_P_R = 0;
        EKG_P_G = 0;
        EKG_P_X0 = 0x54;
        EKG_P_FLAGS = 0x50000000;
        EKG_P_B = 0;
        if (0x83 < DAT_00ae9f36) {
            char cCount;
            if (DAT_00ae9f36 < 0x92) {
                EKG_P_Y0 = 0x92;
                cCount = 0xf - (-0x6e - (char)DAT_00ae9f36);
                DAT_00ae9f2f = (-0x6e - (char)DAT_00ae9f36) * -0x10 - 1;
            } else {
                cCount = 0xf;
                DAT_00ae9f2f = 0xff;
                EKG_P_Y0 = DAT_00ae9f36;
            }
            // Sweep tint. The original writes 0x00be11a4/a5/a6 - the PRIMARY
            // line's own RGB, i.e. the struct handed to FUN_00470c60 below.
            // (Writing the secondary line's bytes instead leaves the primary
            //  carrying the health-status colour case 1 last stored, so a heal
            //  swept orange/red instead of green.)
            //   0x20 clear           -> blue
            //   0x20 set, 0x10 clear -> green  (healing item)
            //   0x20 | 0x10          -> white
            if ((DAT_00ae9f13 & 0x20) != 0) {
                EKG_P_G = DAT_00ae9f2f;
                if ((DAT_00ae9f13 & 0x10) != 0) {
                    EKG_P_R = DAT_00ae9f2f;
                    EKG_P_B = DAT_00ae9f2f;
                }
            } else {
                EKG_P_B = DAT_00ae9f2f;
            }
            do {
                EKG_P_Y1 = EKG_P_Y0;
                FUN_00470c60(g_EkgPrimaryLine, 10);
                if (EKG_P_R != 0) {
                    EKG_P_R = EKG_P_R - 0x10;
                }
                if (EKG_P_G != 0) {
                    EKG_P_G = EKG_P_G - 0x10;
                }
                if (EKG_P_B != 0) {
                    EKG_P_B = EKG_P_B - 0x10;
                }
                EKG_P_Y0 = EKG_P_Y0 + 1;
                cCount = cCount - 1;
            } while ((EKG_P_Y0 < 0xb0) && (cCount != 0));
            DAT_00ae9f36 = DAT_00ae9f36 - 2;
            return;
        }
        DAT_00ae9f13 = DAT_00ae9f13 & 0xcf;
        DAT_00ae9f2e = 0;
        DAT_00ae9f12 = 0;
    }

    #undef EKG_P_FLAGS
    #undef EKG_P_X0
    #undef EKG_P_Y0
    #undef EKG_P_X1
    #undef EKG_P_Y1
    #undef EKG_P_R
    #undef EKG_P_G
    #undef EKG_P_B
    #undef EKG_S_X0
    #undef EKG_S_Y0
    #undef EKG_S_X1
    #undef EKG_S_Y1
    #undef EKG_S_R
    #undef EKG_S_G
    #undef EKG_S_B
    #undef EKG_S_R2
    #undef EKG_S_G2
    #undef EKG_S_B2
}

// ============================================================================
// Map screen
// ============================================================================
// The map submenu has two faces:
//   - Map tab (from the inventory menu): menu_map_state_machine runs a
//     browser showing the map0d..g layouts with floor/room navigation
//     (map_tab_update), then switches to the full map display.
//   - Map display (menu opened with the map item, mode 5): the menu cycle
//     menu_update_map_animation runs the zoom-in animation over the base
//     map, then the map01..09 textures with per-room marker tinting, the
//     flashing "you are here" dot and the area highlight.
// The state block at 0x00ac9890 is shared by both faces.
// ============================================================================

// --- Map data tables (original .data) ---

// Map tab layout file names (0x004d2fc0, 4 x 0x24). The original
// ".\usa\item_m2\map0d.tim" strings are rooted at GAME_DATA_ROOT here.
static const char* const g_MapTabFileNames[4] = {
    "item_m2/map0d.tim",    // layout 0 Mansion only
    "item_m2/map0c.tim",    // layout 1 Mansion and Courtyard
    "item_m2/map00.tim",    // layout 2 Mansion, Courtyard and Guardhouse
    "item_m2/map0e.tim"     // layout 3 Mansion, Courtyard, Guardhouse and Fountain (Lab entrance)
};

// Map display layout file names (0x004d3050, 11 x 0x24), indexed by area.
static const char* const g_MapFileNames[11] = {
    "item_m2/map01.tim", // Mansion 1F
    "item_m2/map02.tim", // Mansion 2F
    "item_m2/map03.tim", // Mansion B1
    "item_m2/map04.tim", // Courtyard
    "item_m2/map05.tim", // Underground
    "item_m2/map06.tim", // Laboratory B1
    "item_m2/map07.tim", // Laboratory B2
    "item_m2/map08.tim", // Laboratory B3
    "item_m2/map09.tim", // Laboratory B4
    "item_m2/map0a.tim", // Guardhouse 1F
    "item_m2/map0b.tim"  // Guardhouse B1
};

// Map background image (0x004d349c)
static const char* const g_MapBlueFileName = "item_m2/Map_blue.tim";

// Room count per area group (0x004d31e8), groups 0-5
static const unsigned char g_MapRoomCounts[6] = { 32, 31, 19, 18, 24, 0 };

// Area per (layout, room) (0x004d31f0); 0xFF = no room. The left/right
// adjacency reads (DAT_004d31ef / DAT_004d31f1) are this table shifted by
// one byte, i.e. the previous/next room's area.
static const unsigned char g_MapAreaTable[16] = {
    2, 0, 1, 0xFF,
    4, 3, 0xFF, 0xFF,
    6, 5, 0xFF, 0xFF,
    10, 9, 8, 7
};

// Area -> layout (0x004d3200). Entry 6 is the original DAT_004d3206 (2),
// the layout the map display falls back to when the map item grants the lab.
static const unsigned char g_MapAreaLayouts[16] = {
    0, 0, 0, 1, 1, 2, 2, 3, 3, 3, 3, 0, 0, 0, 0, 0
};

// Area -> area group (0x004d3210)
static const unsigned char g_MapAreaGroups[16] = {
    0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 4, 0, 3, 2, 2
};

// Per-layout room count (0x004d321c). For layout 1 this is the index of the
// trailing 0xFF "exit" room, so the last real room resolves through the
// 0xFF path in map_update_layout_state.
static const unsigned char g_MapLayoutRoomCount[4] = { 0, 3, 2, 2 };

// Per-layout floor-number offset (0x004d3220)
static const unsigned char g_MapLayoutOffset[4] = { 4, 4, 4, 4 };

// --- Map sprite descriptors (original .data) ---

// 0x004d3228 - the base map image (tab browser + display backdrop)
static TextureDesc g_MapBaseDesc = {
    0x01000000, 33, 32, 160, 72, 0x15, 0x00, 0x00, 0, 0x1fc,
    0x80, 0x80, 0x80, 0, 0, 0, 0, 0
};

// 0x004d324c - room markers (one per layout, bright/dim by layout)
static TextureDesc g_MapSprites[8] = {
    { 0x01000000, 121, 32, 56, 64, 0x15, 0xa0, 0x00, 0, 0x1fc, 0, 0, 0, 0, 0, 0, 0, 0 },  // 0: marker 0
    { 0x01000000, 73, 32, 80, 64, 0x15, 0x00, 0x48, 0, 0x1fc, 0, 0, 0, 0, 0, 0, 0, 0 },   // 1: marker 1
    { 0x01000000, 49, 32, 40, 64, 0x15, 0x50, 0x48, 0, 0x1fc, 0, 0, 0, 0, 0, 0, 0, 0 },   // 2: marker 2
    { 0x01000000, 121, 32, 28, 64, 0x15, 0xc8, 0x48, 0, 0x1fc, 0, 0, 0, 0, 0, 0, 0, 0 },  // 3: marker 3
    { 0x01000000, 57, 104, 80, 16, 0x15, 0x78, 0x48, 0, 0x1fc, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0 },  // 4: 0x004d32dc "you are here" dot
    { 0x01000000, 137, 104, 24, 16, 0x15, 0xe8, 0x60, 0, 0x1fc, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0 }, // 5: 0x004d3300 floor glyph
    { 0x01000000, 145, 96, 8, 8, 0x15, 0xa0, 0x40, 0, 0x1fc, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0 },   // 6: 0x004d3324 left arrow
    { 0x01000000, 145, 120, 8, 8, 0x15, 0xb0, 0x40, 0, 0x1fc, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0 }   // 7: 0x004d3348 right arrow
};

// 0x004d3370 / 0x004d3394 / 0x004d33b8 - zoom sprites (scaleX/Y fix16.12,
// animated by map_display_animate). The map01..09 TIMs are 128x128 (8bpp
// width field is in 16-bit words), so the original 0x80/0x50/0x50 widths and
// 0x50 pivots are correct for the PC textures.
// zoom0 uses slot 0xd -> page 0x1C, whose CLUT base is 0x1E0 + pageOffset.
// The display loads pass pageOffset 0x1f -> clut base 0x1ff for the LATER
// pages, but the first loads land on pages whose CLUT base reads back 0
// (the map TIMs' CLUT is consumed into the texture itself), so the original
// desc clut 0x1ff made AddSprite_Ex reject zoom0 (clutIdx 511 out of range)
// and the map never drew - the [MAP] log showed r=0 for zoom0. The desc is
// retargeted to the page's actual CLUT base (0) so the map sprite is accepted.
static TextureDesc g_MapZoomDesc[3] = {
    // clutY is 0x1ff, not 0 - all three records at 0x004d3370 / 0x394 /
    // 0x3b8 (stride 0x24) carry it. display_texture derives the palette as
    // `clutY - g_TexturePageClutBase[slot]` and rejects anything outside
    // 0..7, so a 0 here gave 0 - 0x1ff = -511 and the base map layer silently
    // returned 0: the map drew its blue grid and floor label (entries 1 and 2,
    // which had the right tint) but never the room layout.
    { 0x51000000, 16, 0, 128, 100, 0x15, 0x00, 0x88, 0, 0x1ff, 0x80, 0x80, 0x80, 0, 80, 50, 0x1000, 0x1000 },
    { 0x41000000, 0, 0, 80, 104, 0x15, 0x00, 0x88, 0, 0x1ff, 0x80, 0x80, 0x80, 0, 80, 52, 0x1000, 0x1000 },
    { 0x41000000, 0, 0, 80, 104, 0x15, 0x00, 0x88, 0, 0x1ff, 0x80, 0x80, 0x80, 0, 0, 52, 0x1000, 0x1000 }
};

// 0x004d33e0 - floor-transition animation sprites (drawn at zoom state 6)
static TextureDesc g_MapFloorAnim[4] = {
    { 0x51000000, 49, 24, 128, 1, 0x15, 0x00, 0x80, 0, 0x1ff, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0 },
    { 0x01000000, 33, 24, 80, 1, 0x15, 0x00, 0x80, 0, 0x1ff, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0 },
    { 0x01000000, 113, 24, 80, 1, 0x15, 0x00, 0x80, 0, 0x1ff, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0 },
    { 0x51000000, 49, 24, 128, 1, 0x15, 0x00, 0x80, 0, 0x1ff, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0 }
};

// 0x004d3470 - area name label
static TextureDesc g_MapLabelDesc = {
    0x01000000, 0, 0, 8, 8, 0x15, 0x00, 0xf0, 0, 0x1ff, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0
};

// 0x004d3494 - "you are here" dot blink phase (rewritten every frame)
static unsigned char g_MapDotBlink;

// (0x004885c0) - Is the map area known (visited/cleared)?
// Area flag mapping: 0,1->0x7c; 3->0x7e; 4->0x7f; 5,6->0x80; 8,9->0x81.
// Areas 2, 7, 10 and out-of-range values (the 0xFF "no room" table entries)
// are never known - the original's out-of-range path reads uninitialized
// stack and returns 0 in practice.
static int map_area_known(unsigned char area)
{
    // 0x004885c0: the original picks an index off a jump table and adds 0x7c to
    // it, so consecutive areas do NOT share a flag - area 1 is 0x7d, which the
    // port had as 0x7c (a copy of area 0's). Verified target by target:
    //   0 -> 0   1 -> 1   2 -> ret 0   3 -> 2   4 -> 3   5 -> 4
    //   6 -> 4   7 -> ret 0   8 -> 5   9 -> 5   10 -> ret 0
    int idx;
    switch (area) {
    case 0: idx = MAP_INDEX_MANSION_1F; break;
    case 1: idx = MAP_INDEX_MANSION_2F; break;
    case 2: return 0;
    case 3: idx = MAP_INDEX_COURTYARD; break;
    case 4: idx = MAP_INDEX_UNDERGROUND; break;
    case 5:
    case 6: idx = MAP_INDEX_GUARDHOUSE; break;
    case 7: return 0;
    case 8:
    case 9: idx = MAP_INDEX_LABORATORY; break;
    default: return 0;      // area 10 returns 0; >10 is out of the table
    }
    return Flg_ck((int)g_RoomFlags, idx + ROOM_FLAG_MAP_BASE);
}

// (0x00487540) - Map tab: draw the base map, markers, dot and floor glyph
static void map_draw_browser(unsigned char* state)
{
    display_texture(&g_MapBaseDesc, 0x32, 0xc, 1);
    for (int i = 0; i < 4; i++) {
        if ((state[0xd] & (1 << i)) != 0) {
            display_texture(&g_MapSprites[i], 0x2d, 0xc, 1);
        }
    }
    g_MapSprites[4].texV = (unsigned char)(state[4] * 0x10 + 0x48);
    display_texture(&g_MapSprites[4], 0x2d, 0xc, 1);
    g_MapSprites[5].texV =
        (unsigned char)((6 - state[8 + state[4]] - g_MapLayoutOffset[state[4]]) * 0x10);
    display_texture(&g_MapSprites[5], 0x2d, 0xc, 1);

    unsigned char c = state[0x11] + 1;
    state[0x11] = c;
    bool blink = (c & 0x30) != 0;
    if ((state[0xc] & 1) != 0) {
        g_MapSprites[6].texU = (unsigned char)(blink * 8 - 0x60);
        display_texture(&g_MapSprites[6], 0x2d, 0xc, 1);
    }
    if ((state[0xc] & 2) != 0) {
        g_MapSprites[7].texU = (unsigned char)(blink * 8 - 0x50);
        display_texture(&g_MapSprites[7], 0x2d, 0xc, 1);
    }
}

// (0x00487430) - Map tab: update the current room/area state and colors
static void map_update_layout_state(unsigned char* state)
{
    int layout = (int)(char)state[4];
    unsigned int room = (unsigned int)state[8 + layout];
    state[0xc] = 0;
    state[6] = g_MapAreaTable[layout * 4 + room];

    // Right room exists: not the last room of the layout, and the next
    // room's area is known (DAT_004d31f1 = area table shifted by one)
    if (g_MapLayoutRoomCount[layout] - room != 1) {
        unsigned char nextArea = g_MapAreaTable[layout * 4 + room + 1];
        if ((1 << (nextArea & 0x1f) & MAP_AREA_MASK) == 0) {
            if (map_area_known(nextArea) == 0) goto no_right_room;
        }
        state[0xc] = state[0xc] | 1;
    }
no_right_room:
    // Left room exists (DAT_004d31ef = area table shifted back by one)
    if (room != 0) {
        unsigned char prevArea = g_MapAreaTable[layout * 4 + room - 1];
        if ((1 << (prevArea & 0x1f) & MAP_AREA_MASK) == 0) {
            if (map_area_known(prevArea) == 0) goto no_left_room;
        }
        state[0xc] = state[0xc] | 2;
    }
no_left_room:
    // Marker colors: the current layout's marker stays bright, the rest dim
    for (int i = 0; i < 4; i++) {
        unsigned char v = (state[4] == i) ? 0x80 : 0x40;
        g_MapSprites[i].colorMulR = v;
        g_MapSprites[i].colorMulG = v;
        g_MapSprites[i].colorMulB = v;
    }
    g_MapBaseDesc.colorMulR = 0x80;
    g_MapBaseDesc.colorMulG = 0x80;
    g_MapBaseDesc.colorMulB = 0x80;
    // Dot, floor glyph and arrow sprites
    for (int i = 4; i < 8; i++) {
        g_MapSprites[i].colorMulR = 0x80;
        g_MapSprites[i].colorMulG = 0x80;
        g_MapSprites[i].colorMulB = 0x80;
    }
    map_draw_browser(state);
}

// (0x00487230) - Map tab: load the map0d..g texture for the current layout
static void map_tab_load_map(unsigned char* state)
{
    // Highest set layout bit (0-3) of the available-layout mask
    unsigned int uVar1 = 0;
    unsigned int uVar2 = 0;
    do {
        if ((state[0xd] & (1 << uVar1)) != 0) uVar2 = uVar1;
        uVar1++;
    } while (uVar1 < 4);

    if ((int)SUBMENU_STATE_ID - (int)uVar2 != 0x80) {
        SUBMENU_STATE_ID = (unsigned char)(uVar2 + 0x80);
        strcpy(DAT_008e1cb0, GAME_DATA_ROOT);
        strcat(DAT_008e1cb0, g_MapTabFileNames[uVar2]);
        // Map files load to 0x00d022ac = g_TimImageBuffer__bitmap + 0x10000, not
        // the buffer base (0x0048726d / 0x00487280 push 0xd022ac for both the
        // LoadFile destination and the LoadTexturePage source). Loading at the
        // base overwrote the first 40 KB of the shared TIM staging buffer, which
        // the original deliberately leaves alone.
        void* mapBuf = (void*)((unsigned char*)g_TimImageBuffer__bitmap + 0x10000);
        LoadFile(DAT_008e1cb0, mapBuf, 0x20);
        DAT_00ac98a8[0] = 0x140;    // dead stores, see declaration
        DAT_00ac98a8[1] = 0x0001;
        DAT_00ac98a8[2] = 0x0080;
        DAT_00ac98a8[3] = 0x0088;
        LoadTexturePage(mapBuf, 0x15, 0x1c, 0xc, 0, 0, 0, 0);
    }
    map_update_layout_state(state);
}

// (0x004872d0) - Map tab: navigate rooms (up/down) and layouts (left/right)
static void map_tab_navigate(unsigned char* state)
{
    if ((state[0xf] & 1) == 0) {
        // Up: next room in the layout path (gated by the move flags)
        if (((pad_held_byte1() & 0x10) != 0) && ((state[0xc] & 1) != 0)) {
            state[8 + state[4]] = state[8 + state[4]] + 1;
            play_sfx(3, 4, 0);
        }
        // Down: previous room
        if (((pad_held_byte1() & 0x40) != 0) && ((state[0xc] & 2) != 0)) {
            state[8 + state[4]] = state[8 + state[4]] - 1;
            play_sfx(3, 4, 0);
        }
        // Right: next layout (mansion 4-layout wrap: 0<->3, 1<->2)
        if ((pad_held_byte1() & 0x80) != 0) {
            if ((state[0xd] & 8) == 0) {
                unsigned char next = state[4] + 1;
                if ((state[4] == 2) || ((state[0xd] & (1 << (next & 0x1f))) == 0))
                    goto tab_layout_done;
                state[4] = next;
            } else {
                switch (state[4]) {
                case 0: state[4] = 3; break;
                case 1: state[4] = 2; break;
                case 2: state[4] = 2; break;
                case 3: state[4] = 1; break;
                }
            }
            play_sfx(3, 4, 0);
        }
tab_layout_done:
        // Left: previous layout
        if ((pad_held_byte1() & 0x20) != 0) {
            if ((state[0xd] & 8) != 0) {
                switch (state[4]) {
                case 0:
                case 3: state[4] = 0; break;
                case 1: state[4] = 3; break;
                case 2: state[4] = 1; break;
                }
                play_sfx(3, 4, 0);
                map_update_layout_state(state);
                return;
            }
            unsigned char prev = state[4] - 1;
            if ((state[4] != 0) && ((state[0xd] & (1 << (prev & 0x1f))) != 0)) {
                state[4] = prev;
                play_sfx(3, 4, 0);
            }
        }
    }
    map_update_layout_state(state);
}

// (0x00487200) - Map tab update dispatcher (sub-state machine)
static void map_tab_update(unsigned char* state)
{
    if (state[1] == 0) {
        map_tab_load_map(state);
        state[1] = state[1] + 1;
        return;
    }
    if (state[1] == 1) {
        map_tab_navigate(state);
    }
}

// (0x00487ec0) - Map display: draw the zoom sprites, the flashing dot, the
// floor-transition arrows and the area highlight
static void map_display_draw(unsigned char* state)
{
    // TEMP DEBUG: report the zoom sprite draw results once
    static int s_dbgOnce = 1;
    if (s_dbgOnce) {
        s_dbgOnce = 0;
        dbg_printf("[MAP] zoom0 slot0xd r=%d | zoom1 slot0xf r=%d | zoom2 slot0xf r=%d\n",
            AddSprite_Ex(&g_MapZoomDesc[0], 0x28, 0xd, 1),
            AddSprite_Ex(&g_MapZoomDesc[1], 0x2a, 0xf, 1),
            AddSprite_Ex(&g_MapZoomDesc[2], 0x2a, 0xf, 1));
        for (int s = 0x1B; s <= 0x1F; s++) {
            dbg_printf("[MAP] page %02X srv=%p w=%d h=%d bpp=%d ox=%d oy=%d depth=%d clut=%d\n",
                s, g_TexturePageSRV[s], g_TexturePageWidth[s], g_TexturePageHeight[s],
                g_TexturePageBpp[s], g_TexturePageOriginX[s], g_TexturePageOriginY[s],
                g_TexturePageId[s], g_TexturePageClutBase[s]);
        }
        dbg_printf("[MAP] zoom0: flags=%08X xy=(%d,%d) wh=(%d,%d) texUV=(%d,%d) depth=%d clut=%d\n",
            g_MapZoomDesc[0].flags, g_MapZoomDesc[0].screenX, g_MapZoomDesc[0].screenY,
            g_MapZoomDesc[0].width, g_MapZoomDesc[0].height,
            g_MapZoomDesc[0].texU, g_MapZoomDesc[0].texV, g_MapZoomDesc[0].texturePage,
            g_MapZoomDesc[0].clutY);
    } else {
        AddSprite_Ex(&g_MapZoomDesc[0], 0x28, 0xd, 1);
        AddSprite_Ex(&g_MapZoomDesc[1], 0x2a, 0xf, 1);
        AddSprite_Ex(&g_MapZoomDesc[2], 0x2a, 0xf, 1);
    }

    // Flashing "you are here" dot: copy the zoom sprite and tint it with the
    // blink phase (the original copies 36 bytes including the padding; the
    // 9th dword is zero here and never read)
    for (int i = 0; i < 8; i++) ((unsigned int*)DAT_00ac3668)[i] = ((unsigned int*)&g_MapZoomDesc[0])[i];
    DAT_00ac3668[8] = 0;
    unsigned char blink = (unsigned char)(g_MapDotBlink << 2);
    ((unsigned char*)DAT_00ac3668)[0x14] = blink;   // colorMulR
    AddSprite_Ex((TextureDesc*)DAT_00ac3668, 0x26, 0xe, 1);

    // Floor-transition arrows while the zoom is parked at state 6
    if (state[3] == 6) {
        if (state[0xe] < 0xd0) {
            unsigned char a = state[0xe] >> 1;
            g_MapFloorAnim[0].screenY = (short)(a + 0x18);
            g_MapFloorAnim[0].height = 1;
            g_MapFloorAnim[0].texV = (unsigned char)(a - 0x7c);
            g_MapFloorAnim[1].screenY = (short)(a + 0x18);
            g_MapFloorAnim[1].height = 1;
            g_MapFloorAnim[1].texV = (unsigned char)(a - 0x78);
            g_MapFloorAnim[2].screenY = (short)(a + 0x18);
            g_MapFloorAnim[2].height = 1;
            g_MapFloorAnim[2].texV = (unsigned char)(a - 0x78);
            if (g_MapFloorAnim[0].texV > 0x8d) display_texture(&g_MapFloorAnim[0], 0x23, 0xd, 1);
            if (g_MapFloorAnim[1].texV > 0x8d) display_texture(&g_MapFloorAnim[1], 0x24, 0xf, 1);
            if (g_MapFloorAnim[2].texV > 0x8d) display_texture(&g_MapFloorAnim[2], 0x25, 0xf, 1);
            g_MapFloorAnim[3].screenY = (short)(a + 0x18);
            g_MapFloorAnim[3].colorMulR = blink;
            g_MapFloorAnim[3].height = 1;
            g_MapFloorAnim[3].texV = (unsigned char)(a - 0x7c);
            if (g_MapFloorAnim[3].texV > 0x8d) display_texture(&g_MapFloorAnim[3], 0x22, 0xe, 1);
        }
        state[0xe] = state[0xe] + 1;
        if (0x118 < state[0xe]) state[0xe] = 0;
    }

    // Area highlight animation (label sprite)
    if ((state[0x10] > 1) && (state[0xf] != 0)) {
        unsigned char frame = (state[0x15] / 3) & 7;
        switch (state[0x10]) {
        case 2:
            play_sfx(3, 10, 0);
            state[0x10] = 3;
            // fall through
        case 3:
            state[0x15] = state[0x15] + 2;
            if (0x17 <= state[0x15]) {
                state[0x10] = 4;
                state[0x15] = 0x17;
            }
            break;
        case 4:
            state[0x15] = state[0x15] - 1;
            if (state[0x15] == 0) {
                state[0x10] = 5;
                state[0x15] = 0x14;
            }
            break;
        case 5:
            frame = 0;
            state[0x15] = state[0x15] - 1;
            if (state[0x15] == 0) {
                state[0x10] = 2;
                state[0x15] = 0;
            }
            break;
        }
        if (state[0xf] / 3 == 0) {
            g_MapLabelDesc.screenX = 0x38;
            g_MapLabelDesc.screenY = 0x34;
        } else {
            g_MapLabelDesc.screenX = 0x86;
            g_MapLabelDesc.screenY = 0x43;
        }
        g_MapLabelDesc.texU = (unsigned char)(frame << 3);
        display_texture(&g_MapLabelDesc, 0x21, 0xd, 1);
    }
}

// (0x00487a30) - Map display: blink the dot and run the zoom animation
static void map_display_animate(unsigned char* state)
{
    // "You are here" dot blink (only when the area group matches)
    if (g_MapAreaGroups[state[6]] == state[5]) {
        switch (state[2]) {
        case 0:
            DAT_00ac368c = 0x50;
            state[2] = 1;
            break;
        case 1:
            DAT_00ac368c = DAT_00ac368c + 2;
            if (0xa0 < DAT_00ac368c) state[2] = 2;
            break;
        case 2:
            DAT_00ac368c = DAT_00ac368c - 2;
            if (DAT_00ac368c < 0x50) state[2] = 1;
            break;
        }
        DAT_00ac3690 = (unsigned short)((0x80 << 8) | (DAT_00ac368c >> 3));
        g_MapDotBlink = (unsigned char)(DAT_00ac3690 & 0x1f);
        DAT_00ac98a8[0] = (short)(state[7] * 6 + 0x10);     // dead stores
        DAT_00ac98a8[1] = 0x1ff;
        DAT_00ac98a8[2] = 1;
        DAT_00ac98a8[3] = 1;
    }

    // Map zoom state machine (fix16.12 scale, pivot-relative)
    switch (state[3]) {
    case 0:
        for (int i = 0; i < 3; i++) {
            g_MapZoomDesc[i].scaleX = 1;
            g_MapZoomDesc[i].scaleY = 200;
        }
        g_MapZoomDesc[0].flags = g_MapZoomDesc[0].flags & 0xefffffff;
        state[3] = 1;
        play_sfx(3, 9, 0);
        // fall through
    case 1:
        g_MapZoomDesc[0].screenX = 0x10;
        g_MapZoomDesc[0].screenY = 2;
        g_MapZoomDesc[0].width = 0x80;
        g_MapZoomDesc[1].screenX = 0;
        g_MapZoomDesc[1].screenY = 0;
        g_MapZoomDesc[1].width = 0x51;
        g_MapZoomDesc[2].screenX = 0;
        g_MapZoomDesc[2].screenY = 0;
        g_MapZoomDesc[2].width = 0x51;
        state[3] = 2;
        map_display_draw(state);
        return;
    case 2:
        for (int i = 0; i < 3; i++) {
            g_MapZoomDesc[i].scaleX = (short)(g_MapZoomDesc[i].scaleX * 3);
        }
        if (g_MapZoomDesc[0].scaleX < 0xfff) break;
        for (int i = 0; i < 3; i++) g_MapZoomDesc[i].scaleX = 0xfff;
        state[3] = 3;
        map_display_draw(state);
        return;
    case 3:
        for (int i = 0; i < 3; i++) {
            g_MapZoomDesc[i].scaleY = (short)(g_MapZoomDesc[i].scaleY + 900);
        }
        if (g_MapZoomDesc[0].scaleY < 0x15dc) break;
        for (int i = 0; i < 3; i++) g_MapZoomDesc[i].scaleY = 0x15dc;
        state[3] = 4;
        map_display_draw(state);
        return;
    case 4:
        for (int i = 0; i < 3; i++) {
            g_MapZoomDesc[i].scaleY = (short)(g_MapZoomDesc[i].scaleY - 300);
        }
        if (g_MapZoomDesc[0].scaleY > 0x1000) break;
        for (int i = 0; i < 3; i++) {
            g_MapZoomDesc[i].scaleX = 0x1000;
            g_MapZoomDesc[i].scaleY = 0x1000;
        }
        g_MapZoomDesc[0].screenX = 0x80;
        g_MapZoomDesc[0].screenY = 0x4e;
        g_MapZoomDesc[0].width = 0x80;
        g_MapZoomDesc[1].screenX = 0x70;
        g_MapZoomDesc[1].screenY = 0x4c;
        g_MapZoomDesc[1].width = 0x50;
        g_MapZoomDesc[2].screenX = 0x70;
        g_MapZoomDesc[2].screenY = 0x4c;
        g_MapZoomDesc[2].width = 0x50;
        DAT_00ac3694 = 8;
        state[3] = 5;
        map_display_draw(state);
        return;
    case 5:
        DAT_00ac3694 = DAT_00ac3694 - 1;
        if (DAT_00ac3694 == 0) {
            g_MapZoomDesc[0].flags = g_MapZoomDesc[0].flags | 0x10000000;
            state[3] = 6;
            DAT_00ac3694 = 10;
            map_display_draw(state);
            return;
        }
        break;
    case 6:
        // Wait for the area highlight to start (substate 1 -> 2)
        if (state[0x10] == 1) {
            DAT_00ac3694 = DAT_00ac3694 - 1;
            if (DAT_00ac3694 == 0) {
                state[0x10] = 2;
                map_display_draw(state);
                return;
            }
        }
        break;
    case 7:
        g_MapZoomDesc[0].flags = g_MapZoomDesc[0].flags & 0xefffffff;
        g_MapZoomDesc[0].screenX = 0x10;
        g_MapZoomDesc[0].screenY = 2;
        g_MapZoomDesc[0].width = 0x80;
        g_MapZoomDesc[1].screenX = 0;
        g_MapZoomDesc[1].screenY = 0;
        g_MapZoomDesc[1].width = 0x51;
        g_MapZoomDesc[2].screenX = 0;
        g_MapZoomDesc[2].screenY = 0;
        g_MapZoomDesc[2].width = 0x51;
        for (int i = 0; i < 3; i++) g_MapZoomDesc[i].scaleX = 0xfff;
        state[0xe] = 0;
        if (state[0x10] != 0) state[0x10] = 1;
        state[3] = 8;
        // fall through
    case 8:
        for (int i = 0; i < 3; i++) {
            g_MapZoomDesc[i].scaleY = (short)(g_MapZoomDesc[i].scaleY - 0x28a);
        }
        if (g_MapZoomDesc[0].scaleY > 1) break;
        for (int i = 0; i < 3; i++) g_MapZoomDesc[i].scaleY = 200;
        state[3] = 9;
        // fall through
    case 9:
        for (int i = 0; i < 3; i++) {
            g_MapZoomDesc[i].scaleX = (short)(g_MapZoomDesc[i].scaleX / 2);
        }
        if (g_MapZoomDesc[0].scaleX > 1) break;
        for (int i = 0; i < 3; i++) g_MapZoomDesc[i].scaleX = 1;
        if ((state[0xf] & 1) != 0) {
            *(unsigned int*)&state[0] = 0x106;   // close path: state 6, substate 1
        } else {
            *(unsigned int*)&state[0] = 0x100;   // re-open: state 0
        }
        map_display_draw(state);
        return;
    }
    map_display_draw(state);
}

// (0x004876b0) - Map display: load the layout map and tint the room markers
static void map_display_load_textures(unsigned char* state)
{
    if ((int)(char)state[6] - (int)SUBMENU_STATE_ID == -0x84) {
        // Texture already loaded for this area
    } else {
        SUBMENU_STATE_ID = (unsigned char)(state[6] + 0x84);
        strcpy(DAT_008e1cb0, GAME_DATA_ROOT);
        strcat(DAT_008e1cb0, g_MapFileNames[state[6]]);
        // 0x004876da / 0x00487867 / 0x004879d0: the map file loads to
        // 0x00d022ac == g_TimImageBuffer__bitmap + 0x10000, and that same
        // pointer is the LoadTexturePage source. Every marker offset below is
        // absolute and assumes it: +0x10014 == 0x00d022c0 == load + 0x14, the
        // TIM's first CLUT entry (0x00487809 reads word [edx*2 + 0xd022c0]).
        void* mapBuf = (void*)((unsigned char*)g_TimImageBuffer__bitmap + 0x10000);
        size_t fileSize = LoadFile(DAT_008e1cb0, mapBuf, 0x20);

        unsigned int group = g_MapAreaGroups[state[6]];
        unsigned int roomCount = g_MapRoomCounts[group];

        // The "room-marker patch section" is simply the map TIM's own CLUT, so
        // it is always present: load + 0x14 is CLUT entry 0, load + 0x22 is
        // entry 7, load + 0x2e is entry 13 (6 entries = 12 bytes per room),
        // load + 0x214 is the image block's length field and load + 0x220 the
        // 8bpp pixel data. Recolouring those entries is how unvisited rooms are
        // hidden. An earlier revision tested `fileSize > 0x10220` and concluded
        // the section was absent - that was the wrong base (the file was being
        // loaded 64 KB below these offsets), and it silently disabled all marker
        // tinting. The 41504-byte map files clear 0x220 with 0xa000 of pixels,
        // which is exactly what the count field reports.
        bool hasMarkerSection = (fileSize != (size_t)-1) && (fileSize > 0x220);
        if (hasMarkerSection) {
            int known = map_area_known(state[6]);
            if (known == 0) {
                // 12-byte marker header block at +0x10022 (just before the
                // first room entry at +0x1002e)
                unsigned int* pFirst = (unsigned int*)((unsigned char*)g_TimImageBuffer__bitmap + 0x10022);
                pFirst[0] = 0; pFirst[1] = 0; pFirst[2] = 0;
            }
            // Tint the room markers in the loaded TIM buffer: visited rooms
            // keep their colour, unvisited go transparent, the current room
            // is blanked (its marker is shown by the highlight page).
            unsigned int* pEntry = (unsigned int*)((unsigned char*)g_TimImageBuffer__bitmap + 0x1002e);
            for (unsigned int room = 0; room < roomCount; room++) {
                if (Flg_ck((int)g_RoomFlags, g_StageRoomFlagOffset[group] + room) == 0) {
                    if (known == 0) {
                        pEntry[0] = 0; pEntry[1] = 0; pEntry[2] = 0;
                    } else {
                        *(unsigned int*)((unsigned char*)pEntry + 2) = 0;
                        *(unsigned short*)((unsigned char*)pEntry + 6) = 0;
                    }
                } else if ((group == state[5]) && (state[7] == room)) {
                    *(unsigned int*)((unsigned char*)pEntry + 2) = 0;
                    *(unsigned short*)((unsigned char*)pEntry + 6) = 0;
                }
                pEntry += 3;
            }
            // Drop dead marker indices (their colour-table entry is zero)
            // Port safety net only - the original (0x004877ef) trusts the
            // count field. Clamp against the pixel data's real extent.
            int count = *(int*)((unsigned char*)g_TimImageBuffer__bitmap + 0x10214) - 0xc;
            if (count > (int)(fileSize - 0x220)) count = (int)(fileSize - 0x220);
            for (int i = 0; i < count; i++) {
                unsigned char* idx = (unsigned char*)g_TimImageBuffer__bitmap + 0x10220 + i;
                if (*(short*)((unsigned char*)g_TimImageBuffer__bitmap + (unsigned int)*idx * 2 + 0x10014) == 0) {
                    *idx = 0;
                }
            }
        }
        DAT_00ac98a8[0] = 0x140;    // dead stores
        DAT_00ac98a8[1] = 0x188;
        DAT_00ac98a8[2] = 0x80;
        DAT_00ac98a8[3] = 0x70;
        LoadTexturePage(mapBuf, 0x15, 0x1f, 0xd, 4, 0, 0x88, 0);

        // Reload: the highlight page shows only the current room's marker
        LoadFile(DAT_008e1cb0, mapBuf, 0x20);
        if (hasMarkerSection) {
            int known = map_area_known(state[6]);
            if (known == 0) {
                unsigned int* pFirst = (unsigned int*)((unsigned char*)g_TimImageBuffer__bitmap + 0x10022);
                pFirst[0] = 0; pFirst[1] = 0; pFirst[2] = 0;
            }
            unsigned int* pEntry = (unsigned int*)((unsigned char*)g_TimImageBuffer__bitmap + 0x1002e);
            for (unsigned int room = 0; room < roomCount; room++) {
                if ((Flg_ck((int)g_RoomFlags, g_StageRoomFlagOffset[group] + room) == 0) ||
                    (group != state[5]) || (state[7] != room)) {
                    pEntry[0] = 0; pEntry[1] = 0; pEntry[2] = 0;
                } else {
                    *(unsigned int*)((unsigned char*)pEntry + 2) = 0x1f001f;
                    *(unsigned short*)((unsigned char*)pEntry + 6) = 0x1f;
                }
                pEntry += 3;
            }
            // Port safety net only - the original (0x004877ef) trusts the
            // count field. Clamp against the pixel data's real extent.
            int count = *(int*)((unsigned char*)g_TimImageBuffer__bitmap + 0x10214) - 0xc;
            if (count > (int)(fileSize - 0x220)) count = (int)(fileSize - 0x220);
            for (int i = 0; i < count; i++) {
                unsigned char* idx = (unsigned char*)g_TimImageBuffer__bitmap + 0x10220 + i;
                if (*(short*)((unsigned char*)g_TimImageBuffer__bitmap + (unsigned int)*idx * 2 + 0x10014) == 0) {
                    *idx = 0;
                }
            }
        }
        DAT_00ac98a8[0] = 0x140;
        DAT_00ac98a8[1] = 0x188;
        DAT_00ac98a8[2] = 0x80;
        DAT_00ac98a8[3] = 0x70;
        LoadTexturePage(mapBuf, 0x15, 0x1f, 0xe, 4, 0, 0x88, 0);

        strcpy(DAT_008e1cb0, GAME_DATA_ROOT);
        strcat(DAT_008e1cb0, g_MapBlueFileName);
        LoadFile(DAT_008e1cb0, mapBuf, 0x20);
        LoadTexturePage(mapBuf, 0x15, 0x1f, 0xf, 4, 0, 0x88, 0);
    }

    // Start the area highlight for the map variants that show it
    if ((state[0xf] == 2) && (state[6] == 0)) state[0x10] = 1;
    else if ((state[0xf] == 4) && (state[6] == 6)) state[0x10] = 1;
    else state[0x10] = 0;
    map_display_animate(state);
}

// (0x00487640) - Map display update dispatcher + dim the browser sprites
static void map_display_update(unsigned char* state)
{
    if (state[1] == 0) {
        map_display_load_textures(state);
        state[1] = state[1] + 1;
    } else if (state[1] == 1) {
        map_display_animate(state);
    }
    // Dim everything for the display view (base map 0x40, sprites 0x30)
    g_MapBaseDesc.colorMulR = 0x40;
    g_MapBaseDesc.colorMulG = 0x40;
    g_MapBaseDesc.colorMulB = 0x40;
    for (int i = 0; i < 8; i++) {
        g_MapSprites[i].colorMulR = 0x30;
        g_MapSprites[i].colorMulG = 0x30;
        g_MapSprites[i].colorMulB = 0x30;
    }
    map_draw_browser(state);
}

// (0x00488430) - Build the area-known bitmask and the layout availability mask
static void map_build_area_mask(unsigned char* state)
{
    MAP_AREA_MASK = 0;
    // Areas reached through visited rooms (per area group and room range)
    for (unsigned int group = 0; group < 5; group++) {
        for (unsigned int room = 0; room < g_MapRoomCounts[group]; room++) {
            if (Flg_ck((int)g_RoomFlags, g_StageRoomFlagOffset[group] + room) == 0) continue;
            switch (group) {
            case 0: MAP_AREA_MASK |= 1; break;
            case 1:
                if ((room < ROOM_MANSION_B1_PASSAGE_1) || (room == 0x1d)) MAP_AREA_MASK |= 2;
                else MAP_AREA_MASK |= 4;
                break;
            case 2:
                if ((room < ROOM_ITEM_CHAMBER) || (room == ROOM_ELEVATOR_TO_LABORATORY)) MAP_AREA_MASK |= 8;
                else MAP_AREA_MASK |= 0x10;
                break;
            case 3:
                if (room < ROOM_WATER_TANK_ENTRY) MAP_AREA_MASK |= 0x20;
                else MAP_AREA_MASK |= 0x40;
                break;
            case 4:
                if ((room < ROOM_LAB_LADDER_ROOM) || (room == 0x16)) MAP_AREA_MASK |= 0x80;
                else if (room < ROOM_LAB_B3_O_PASSAGE) MAP_AREA_MASK |= 0x100;
                else if (room < ROOM_MAIN_LAB) MAP_AREA_MASK |= 0x200;
                else MAP_AREA_MASK |= 0x400;
                break;
            }
        }
    }
    // Areas granted by the map-clear flags (0x7c-0x81)
    for (unsigned int area = 0; area < 0xb; area++) {
        if (map_area_known(area) != 0) MAP_AREA_MASK |= 1 << area;
    }
    // Layout availability mask
    MAP_LAYOUT_MASK = 0;
    for (unsigned int area = 0; area < 0xb; area++) {
        if ((MAP_AREA_MASK & (1 << area)) != 0) {
            MAP_LAYOUT_MASK |= 1 << (g_MapAreaLayouts[area] & 0x1f);
        }
    }
}

// (0x00488950) - Compute MAP_MODE: the map screen's OBJECTIVE HIGHLIGHT.
// Not a map-data variant selector despite the old name - MAP_MODE's only real
// consumer is map_display_animate, which blinks one area with a label sprite
// (mode 1/2 -> area 0 Mansion 1F, mode 3/4 -> area 6 guardhouse). Inputs come
// in pairs: an "objective outstanding" bit in g_ScenarioFlags2 and an
// "acknowledged" bit in g_ScenarioFlags; the highlight runs while the first is
// set and the second still clear. Full trace in docs/SCENARIO_FLAGS.md.
static void map_update_objective_highlight(void)
{
    if ((g_main_state_flags & MSF_CHAR_VARIANT) != 0) {
        if (Flg_ck((int)g_ScenarioFlags2, SCENARIO2_FLAG_PROGRESS_23) != 0) {
            if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_PROGRESS_49) == 0) { MAP_MODE = 1; return; }
        }
        MAP_MODE = 0;
        return;
    }
    if (Flg_ck((int)g_ScenarioFlags2, SCENARIO2_FLAG_SERUM_OBJ1_CHRIS) != 0) {
        if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_PROGRESS_49) == 0) { MAP_MODE = 1; return; }
    }
    if (Flg_ck((int)g_ScenarioFlags2, SCENARIO2_FLAG_SERUM_OBJ2_CHRIS) != 0) {
        if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_PROGRESS_4A) == 0) {
            if ((get_stage_id() != STAGE_MANSION_1F) &&
                (get_stage_id() != STAGE_MANSION_2F)) {
                if (Flg_ck((int)g_ScenarioFlags2, SCENARIO2_FLAG_PLANT42_OBJ) != 0) { MAP_MODE = 1; return; }
                MAP_MODE = 0;
                return;
            }
            MAP_MODE = 1;
            return;
        }
    }
    if (Flg_ck((int)g_ScenarioFlags2, SCENARIO2_FLAG_PLANT42_OBJ) != 0) {
        if (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_PROGRESS_48) == 0) { MAP_MODE = 3; return; }
    }
    MAP_MODE = 0;
}

// (0x00488660) - Mark a FILE as collected (RoomFlags bit 0x82 + file index).
// The file index is itemId - 0x5F; pickup_item_seen reads these bits back for
// the FILE tab. This was named map_set_room_flag, which was a misnomer - the
// rooms-visited bits are a different block (0x00..0x7B, written by
// room_set_visited_flag 0x00488570). See BioCard.h.
static void file_set_collected_flag(int fileIndex)
{
    Flg_on((int)g_RoomFlags, fileIndex + ROOM_FLAG_FILE_BASE);
}

// (0x00488160) - Initialize map screen with room data (menu open, modes 0/6)
static void menu_init_map_screen(void)
{
    MAP_ROOM_IDX[0] = 1;
    MAP_ROOM_IDX[1] = 1;
    MAP_ROOM_IDX[2] = 1;
    MAP_ROOM_IDX[3] = 3;
    *(unsigned int*)&DAT_00ac9890[0] = 0;   // state, substate, blink, zoom
    MAP_ROOM = g_roomId;
    MAP_GROUP = g_stageId % 5; // get zero-indexed absolute stage ID
    switch (MAP_GROUP) {
    case STAGE_MANSION_1F:
        if (((g_roomId == ROOM_GALLERY) && (2 < g_roomCameraId)) && (g_roomCameraId < 7)) {
            MAP_AREA = 0;
            MAP_ROOM = 0x1d;
        } else if ((g_roomId == ROOM_MANSION_BAR) && ((g_roomCameraId == 3) || (g_roomCameraId == 4))) {
            MAP_AREA = 0;
            MAP_ROOM = 0x1e;
        } else if ((g_roomId == ROOM_MANSION_1F_ELEVATOR_FRONT) && (g_roomCameraId == 0)) {
            MAP_AREA = 2;
            MAP_ROOM = 0x1e;
            MAP_GROUP = 1;
        } else {
            MAP_AREA = 0;
        }
        break;
    case STAGE_MANSION_2F:
        if ((g_roomId == ROOM_SMALL_DINING) && ((g_roomCameraId == 2) || (g_roomCameraId == 5))) {
            MAP_AREA = 1;
            MAP_ROOM = 0x1d;
        } else if ((g_roomId == ROOM_LESSON_ROOM) && (3 < g_roomCameraId) && (g_roomCameraId < 8)) {
            MAP_AREA = 0;
            MAP_ROOM = 0x1f;
            MAP_GROUP = 0;
        } else if (g_roomId < ROOM_MANSION_B1_PASSAGE_1) {
            MAP_AREA = 1;
        } else {
            MAP_AREA = 2;
        }
        break;
    case STAGE_COURTYARD:
        if (((g_roomId == ROOM_BOULDER_1_PASSAGE) && (4 < g_roomCameraId)) && (g_roomCameraId < 7)) {
            MAP_AREA = 4;
            MAP_ROOM = 0x11;
        } else if ((g_roomId == ROOM_BOULDER_2_PASSAGE) && (g_roomCameraId == 6)) {
            MAP_AREA = 4;
            MAP_ROOM = 0x12;
        } else if (g_roomId < ROOM_ITEM_CHAMBER) {
            MAP_AREA = 3;
        } else {
            MAP_AREA = 4;
        }
        break;
    case STAGE_GUARDHOUSE:
        if (g_roomId < ROOM_WATER_TANK_ENTRY) MAP_AREA = 5;
        else MAP_AREA = 6;
        break;
    case STAGE_LABORATORY:
        if ((g_roomId == ROOM_MORGUE) && (g_roomCameraId < 4)) {
            MAP_AREA = 9;
            MAP_ROOM = 0x16;
        } else if (g_roomId < ROOM_LAB_LADDER_ROOM) {
            MAP_AREA = 7;
        } else if (g_roomId < ROOM_LAB_B3_O_PASSAGE) {
            MAP_AREA = 8;
        } else {
            MAP_AREA = 9;
            if (0x12 < g_roomId) MAP_AREA = 10;
        }
        break;
    }
    map_build_area_mask(&DAT_00ac9890[0]);
    // Room index per layout: the room whose area matches the current one
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (g_MapAreaTable[i * 4 + j] == MAP_AREA) {
                MAP_ROOM_IDX[i] = (unsigned char)j;
            }
        }
    }
    map_update_objective_highlight();
    if (MAP_MODE != 0) MAP_MODE = MAP_MODE + 1;
    MAP_LAYOUT = g_MapAreaLayouts[MAP_AREA];
    MAP_HL_STATE = 0;
}

// (0x004886b0) - Update map screen animation (returns non-zero when complete)
static int menu_update_map_animation(void)
{
    unsigned char timer = MAP_TIMER;
    switch (MAP_STATE) {
    case 0:
        MAP_TIMER = 10;
        MAP_STATE = MAP_STATE + 1;
        // fall through
    case 1:
        timer = MAP_TIMER;
        MAP_TIMER = MAP_TIMER - 1;
        if (timer == 0) {
            MAP_STATE = MAP_STATE + 1;
            MAP_TIMER = 5;
            play_sfx(3, 6, 0);
        }
        break;
    case 2:
        // Map tab browser phase (loads the map0d..g texture)
        map_tab_update(&DAT_00ac9890[0]);
        timer = MAP_TIMER;
        MAP_TIMER = MAP_TIMER - 1;
        if (timer == 0) {
            MAP_STATE = MAP_STATE + 1;
            MAP_TIMER = 0x14;
            MAP_ZOOM_STATE = 0;
            MAP_BLINK_STATE = 0;
            MAP_SUBSTATE = 0;
        }
        break;
    case 3:
        // Full map display: zoom in, then park at room 6
        map_display_update(&DAT_00ac9890[0]);
        timer = MAP_TIMER;
        if ((MAP_ZOOM_STATE == 6) && (MAP_TIMER = MAP_TIMER - 1, timer == 0)) {
            MAP_STATE = MAP_STATE + 1;
            MAP_HL_STATE = 2;
        }
        break;
    case 4:
        map_display_update(&DAT_00ac9890[0]);
        if (((dpad_pressed_byte1() & 0x80) != 0) || ((pad_held_byte1() & 8) != 0)) {
            MAP_ZOOM_STATE = 7;
            MAP_STATE = MAP_STATE + 1;
            play_sfx(3, 5, 0);
        }
        break;
    case 5:
        map_display_update(&DAT_00ac9890[0]);
        break;
    case 6:
        MAP_TIMER = 5;
        MAP_STATE = MAP_STATE + 1;
        play_sfx(3, 5, 0);
        // fall through
    case 7:
        // Close animation returns to the map tab browser
        map_tab_update(&DAT_00ac9890[0]);
        timer = MAP_TIMER;
        MAP_TIMER = MAP_TIMER - 1;
        if (timer == 0) {
            MAP_STATE = MAP_STATE + 1;
            MAP_TIMER = 5;
        }
        break;
    case 8:
        MAP_TIMER = MAP_TIMER - 1;
        if (timer == 0) {
            MAP_EXIT_FLAG = 1;
            play_sfx(3, 5, 0);
        }
        break;
    }
    return (MAP_EXIT_FLAG == 0) - 1;
}

// (0x00488880) - Initialize map display (menu mode 5 - map item)
static void menu_init_map_display(void)
{
    map_update_objective_highlight();
    *(unsigned int*)&DAT_00ac9890[0] = 0;
    if (MAP_MODE != 1) {
        MAP_LAYOUT = g_MapAreaLayouts[6];   // DAT_004d3206: layout 2 (lab map)
        MAP_GROUP = 5;
        MAP_AREA = 6;
    } else {
        MAP_LAYOUT = g_MapAreaLayouts[0];   // layout 0 (mansion 1F)
        MAP_GROUP = 1;
        MAP_AREA = 0;
    }
    MAP_ROOM_IDX[2] = (MAP_MODE == 1);
    MAP_ROOM_IDX[1] = 1;
    MAP_ROOM_IDX[0] = 1;
    MAP_ROOM = 0;
    MAP_ROOM_IDX[3] = 3;
    MAP_MOVE_FLAGS = 0;
    MAP_LAYOUT_MASK = 0;
    MAP_ARROW_ANIM = 0;
    MAP_HL_STATE = 0;
    MAP_HL_COUNT = 0;
    MAP_TIMER = 0;
    DAT_00ac9890[0x13] = 0;
    MAP_EXIT_FLAG = 0;
    MAP_HL_FRAME = 0;
    MAP_AREA_MASK = 0;
    map_build_area_mask(&DAT_00ac9890[0]);
}

// ============================================================================
// Pickup-message screen (main_menu state 8, msf bit 0x100)
//
// This is the "you got the item" screen that follows a key/desk pickup: the
// item list slides in with the picked item highlighted, the player confirms,
// the message 0xc6 ("Picked up the X") plays, and the entry is consumed.
// Previously menu_update_status_screen returned 0 forever, so state 8 spun in
// an infinite loop and the game softlocked after any pickup.
//
// State block DAT_00ac98b0 (0x14 bytes):
//   +0  state (0=fade in, 1=list)      +1  substate
//   +2  render state (FUN_004823a0)    +3  spare
//   +4  item slot byte (i>>3)          +5  item slot bit (i&7)
//   +6  list cursor                    +7  character (0/1)
//   +8  pickup-complete flag           +9  list-active flag
//   +0xc slide position (short)        +0xe slide2 position (short)
//   +0x10 "no more items" flag         +0x11 blink phase byte
//   +0x12 blink counter                +0x13 button-hold counter
// ============================================================================

// 0x004d2a08 - per-character key-item list (16 entries each; 0xfe = "seen"
// marker, 0xff = empty). Initialized by pickup_screen_init_table. The port
// also declares a const DAT_004d2a08 at line 175; this is the writable
// runtime copy the screen mutates (the original's is in .data and modified).
static unsigned char g_pickupKeyItemList[32] = {};
static const unsigned char g_keyItemListInit[32] = {
    /* Chris */ 0x0f, 0x02, 0xfe, 0x03, 0x05, 0x06, 0x07, 0x08,
                0x09, 0x0a, 0x0c, 0x0d, 0x0e, 0xff, 0xff, 0xff,
    /* Jill  */ 0x0f, 0x02, 0xfe, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0xff,
};
// DAT_00ac98b0 (0x20 bytes) is declared at line 1101 as the save-file dialog
// state block; the pickup screen reuses the same original address range.
static unsigned char DAT_004d2a34;              // pickup fade ramp (rect r/g/b)
static unsigned char DAT_004d2a35;
static unsigned char DAT_004d2a36;
static unsigned char DAT_004d2a38;              // arrow texture loaded flag

// 0x004d2a28 - fullscreen fade rect (r/g/b animated by FUN_00482800)
// textureId is 0x60000000, not 0x60: the original dword at 0x004d2a28 is
// 0x60000000, which GetTextureVariant reads as variant 3 = "fade to black"
// (black overlay, alpha = max(r,g,b)). With 0x60 the variant came out 0, i.e.
// an OPAQUE fill of the literal r/g/b - so the 0xff the ramp drives it to
// painted the screen solid WHITE instead of solid black.
static RectDrawDesc g_pickupFadeRect = { 0x60000000, 0, 0, 0x140, 0xf0, 0, 0, 0 };

// 0x004d2620 - per-item stack counts for the list navigation. The original
// reads it unguarded by the item id (0xfe/0xff ids hit the following .data,
// which is mostly zero - reproduced by the zero padding).
static const unsigned char g_itemMaxCounts[0x28] = {
    4, 6, 11, 4, 2, 6, 7, 7, 10, 5, 5, 4, 3, 3, 3, 4, 8, 8,
    24, 52, 32, 20, 64, 40, 16, 0, 16, 16, 32, 32, 32, 32,
    64, 1, 0, 1, 128, 0, 104, 0,
};

// 0x004d2648 - texture-page offsets for the arrow/file loads (stride 0xc)
static const unsigned int g_pickupPageOffsets[8] = {
    0x1c, 0x01c00140, 0x00080010, 0x1c, 0x01000140, 0x00c00080, 0x1d, 0,
};

// 0x004d2350 - per-file page backgrounds (filem_l0.pix .. filem_z0.pix,
// filem_00.pix, fixed 0x2a-byte entries as the original indexes them).
// Indexed by the byte tables at 0x004d2a08 (book slots) with *0x2a.
// The original's literal ".\\usa\\item_m2\\" prefix is the GAME_DATA_ROOT
// at compile time (see system/AssetPath.h).
static const char g_filemPixNames[16][0x2a] = {
    GAME_DATA_ROOT "item_m2/filem_l0.pix",
    GAME_DATA_ROOT "item_m2/filem_m0.pix",
    GAME_DATA_ROOT "item_m2/filem_n0.pix",
    GAME_DATA_ROOT "item_m2/filem_o0.pix",
    GAME_DATA_ROOT "item_m2/filem_p0.pix",
    GAME_DATA_ROOT "item_m2/filem_q0.pix",
    GAME_DATA_ROOT "item_m2/filem_r0.pix",
    GAME_DATA_ROOT "item_m2/filem_s0.pix",
    GAME_DATA_ROOT "item_m2/filem_t0.pix",
    GAME_DATA_ROOT "item_m2/filem_u0.pix",
    GAME_DATA_ROOT "item_m2/filem_v0.pix",
    GAME_DATA_ROOT "item_m2/filem_w0.pix",
    GAME_DATA_ROOT "item_m2/filem_x0.pix",
    GAME_DATA_ROOT "item_m2/filem_y0.pix",
    GAME_DATA_ROOT "item_m2/filem_z0.pix",
    GAME_DATA_ROOT "item_m2/filem_00.pix",
};

// 0x004d2308 - file book cover names (file000.tim / file001.tim), fixed
// 0x24-byte entries. The original indexes them with *0x24.
static const char g_fileCoverNames[2][0x24] = {
    GAME_DATA_ROOT "item_m2/file000.tim",
    GAME_DATA_ROOT "item_m2/file001.tim",
};

// Static TextureDescs for the list screen, transcribed byte-for-byte from
// 0x004d2888 / 0x004d2988 / 0x004d29ac / 0x004d29d0.
static unsigned char DAT_004d2888[0x20] = {
    0x00,0x00,0x00,0x00, 0x18,0x00,0x18,0x00, 0x00,0x01,0xc0,0x00,
    0x15,0x00,0x00,0x00, 0x00,0x00,0xfd,0x01, 0x80,0x80,0x80,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};
// 0x004d2988 - "previous page" arrow (8x8 at texU 0x00, screen 0x18,0x74)
static unsigned char DAT_004d2988[0x24] = {
    0x00,0x00,0x00,0x01, 0x18,0x00,0x74,0x00, 0x08,0x00,0x08,0x00,
    0x15,0x00,0x00,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};
// 0x004d29ac - "next page" arrow (8x8 at texU 0x10, screen 0x110,0x74)
static unsigned char DAT_004d29ac[0x24] = {
    0x00,0x00,0x00,0x01, 0x10,0x01,0x74,0x00, 0x08,0x00,0x08,0x00,
    0x15,0x00,0x10,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};
// 0x004d29d0 - the "EXIT" label on the last page. Note this one lives on the
// MENU UI page (tim 0x1c, texU 0x80 / texV 0x88, clut 0x1e4), not the arror
// page, and its base colour is 0x40 (the renderer raises it to 0x50 while the
// last-page flag is set).
static unsigned char DAT_004d29d0[0x24] = {
    0x00,0x00,0x00,0x01, 0x18,0x01,0x6f,0x00, 0x18,0x00,0x10,0x00,
    0x1c,0x00,0x80,0x88, 0x00,0x00,0xe4,0x01, 0x40,0x40,0x40,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};

// ============================================================================
// File book selector textures (FUN_00481d00). Transcribed byte-for-byte from
// 0x004d2668 / 0x004d268c / 0x004d27d0 / 0x004d27f4 / 0x004d2818 /
// 0x004d283c / 0x004d2860. All reference texture page 0x15 (the arror.tim
// page loaded by pickup_load_texture mode 1 into SRV slot 0x1c) and the
// 0x1fc CLUT base (pageOffset 0x1c + 0x1E0).
// ============================================================================

// 0x004d2668 - the book page texture (160x104, drawn at slide pos + 0x20/+0x18)
static unsigned char DAT_004d2668[0x24] = {
    0x00,0x00,0x00,0x01, 0x20,0x00,0x18,0x00, 0xa0,0x00,0x68,0x00,
    0x15,0x00,0x00,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};

// 0x004d268c + i*0x24 - per-slot page icons (4x8 at texU 0xa8). X alternates
// every two slots (0x8c/0x8d/0x8e/0x8f), Y steps by 9 from 0x29. The renderer
// overwrites X/Y per frame, so only the base positions matter.
static unsigned char g_fileSlotDescs[8][0x24] = {
    { 0x00,0x00,0x00,0x01, 0x8c,0x00,0x29,0x00, 0x04,0x00,0x08,0x00,
      0x15,0x00,0xa8,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
    { 0x00,0x00,0x00,0x01, 0x8c,0x00,0x32,0x00, 0x04,0x00,0x08,0x00,
      0x15,0x00,0xa8,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
    { 0x00,0x00,0x00,0x01, 0x8d,0x00,0x3b,0x00, 0x04,0x00,0x08,0x00,
      0x15,0x00,0xa8,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
    { 0x00,0x00,0x00,0x01, 0x8d,0x00,0x44,0x00, 0x04,0x00,0x08,0x00,
      0x15,0x00,0xa8,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
    { 0x00,0x00,0x00,0x01, 0x8e,0x00,0x4d,0x00, 0x04,0x00,0x08,0x00,
      0x15,0x00,0xa8,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
    { 0x00,0x00,0x00,0x01, 0x8e,0x00,0x56,0x00, 0x04,0x00,0x08,0x00,
      0x15,0x00,0xa8,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
    { 0x00,0x00,0x00,0x01, 0x8f,0x00,0x5f,0x00, 0x04,0x00,0x08,0x00,
      0x15,0x00,0xa8,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
    { 0x00,0x00,0x00,0x01, 0x8f,0x00,0x68,0x00, 0x04,0x00,0x08,0x00,
      0x15,0x00,0xa8,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
};

// 0x004d27d0 - the cursor arrow (4x8 at texU 0xa0); X/Y set per frame
static unsigned char DAT_004d27d0[0x24] = {
    0x00,0x00,0x00,0x01, 0x00,0x00,0x00,0x00, 0x04,0x00,0x08,0x00,
    0x15,0x00,0xa0,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};

// 0x004d27f4 / 0x004d2818 - the "other book" indicator icons (8x8 at
// texU 0xb0 / 0xc0). The current book's icon is hidden; the other blinks
// via the texU animation at DAT_004d2802 / 0x004d2826.
static unsigned char DAT_004d27f4[0x24] = {
    0x00,0x00,0x00,0x01, 0x3c,0x00,0x48,0x00, 0x08,0x00,0x08,0x00,
    0x15,0x00,0xb0,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};
static unsigned char DAT_004d2818[0x24] = {
    0x00,0x00,0x00,0x01, 0x9c,0x00,0x48,0x00, 0x08,0x00,0x08,0x00,
    0x15,0x00,0xc0,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};

// 0x004d283c / 0x004d2860 - up/down scroll arrows (8x8 at texU 0xd0 / 0xe0).
// X/Y are set per frame (0x004d2840/2842 and 0x004d2864/2866) and the
// texU blinks via DAT_004d284a / 0x004d286e.
static unsigned char DAT_004d283c[0x24] = {
    0x00,0x00,0x00,0x01, 0x00,0x00,0x00,0x00, 0x08,0x00,0x08,0x00,
    0x15,0x00,0xd0,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};
static unsigned char DAT_004d2860[0x24] = {
    0x00,0x00,0x00,0x01, 0x00,0x00,0x00,0x00, 0x08,0x00,0x08,0x00,
    0x15,0x00,0xe0,0x00, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};

// 0x00482c50 - initialize the key-item list table
static void pickup_screen_init_table(void)
{
    for (int i = 0; i < 32; i++) g_pickupKeyItemList[i] = g_keyItemListInit[i];
}

// 0x00488680 - has this file been collected? The entry is a file index into the
// 0x82 block, not a room id. 0xfe/0xff list entries are "not applicable" and
// report 0.
static int pickup_item_seen(unsigned char entry)
{
    if (entry == 0xff) return 0;
    if (entry == 0xfe) return 0;
    return Flg_ck((int)g_RoomFlags, entry + ROOM_FLAG_FILE_BASE);
}

// 0x00482800 - pickup fade ramp: steps the overlay brightness by 0x20 per
// call toward 0xff (mode 0) or 0 (mode 1); mode 2/3 jump instantly. Returns
// 0xffffffff when the ramp reaches its target. DAT_004d2a34-36 ARE the fade
// rect's r/g/b fields, so this doubles as the overlay draw.
static int pickup_fade_update(int mode)
{
    unsigned int result = 0;
    switch (mode) {
    case 0:
        result = 0;
        {
            unsigned int v = (unsigned int)DAT_004d2a34;
            DAT_004d2a34 = (unsigned char)(v + 0x20);
            DAT_004d2a35 = DAT_004d2a34;
            DAT_004d2a36 = DAT_004d2a34;
            if (0xfe < v + 0x20) {
                result = 0xffffffff;
                DAT_004d2a36 = 0xff;
                DAT_004d2a35 = 0xff;
                DAT_004d2a34 = 0xff;
            }
        }
        break;
    case 1:
        result = 0;
        {
            unsigned int v = (unsigned int)DAT_004d2a34;
            DAT_004d2a34 = (unsigned char)(v - 0x20);
            DAT_004d2a35 = DAT_004d2a34;
            DAT_004d2a36 = DAT_004d2a34;
            if ((int)(v - 0x20) < 1) {
                DAT_004d2a36 = 0;
                DAT_004d2a35 = 0;
                DAT_004d2a34 = 0;
                result = 0xffffffff;
            }
        }
        break;
    case 2:
        // Reading mode: hold the overlay at full strength. Variant 3 makes
        // that an OPAQUE BLACK fullscreen fill (0xff is the alpha, not a
        // colour), which is what the document pages are read against - their
        // own background colour is the CLUT's 0x0000 transparent key.
        DAT_004d2a36 = 0xff;
        DAT_004d2a35 = 0xff;
        DAT_004d2a34 = 0xff;
        break;
    case 3:
        DAT_004d2a36 = 0;
        DAT_004d2a35 = 0;
        DAT_004d2a34 = 0;
        break;
    }
    g_pickupFadeRect.r = DAT_004d2a34;
    g_pickupFadeRect.g = DAT_004d2a35;
    g_pickupFadeRect.b = DAT_004d2a36;
    // blend 3 -> depth 548, exactly as the original (0x004828bf). The
    // renderer interleaves overlays in this depth range with the command
    // sprites, so 548 lands between the pause-menu frame (1060-1140) that it
    // must cover and the reading page/arrows/EXIT (500-516) that draw over it.
    draw_rect(&g_pickupFadeRect, 3, 1);
    return (int)result;
}

// 0x00482be0 - mark the picked item "seen" in the key-item list: the first
// 0xfe entry whose room flag is already raised becomes 0 (Chris/Jill half
// selected by state[7]).
static void pickup_mark_seen(unsigned char* state)
{
    for (int i = 0; i < 0x10; i++) {
        unsigned char entry = g_pickupKeyItemList[i + state[7] * 0x10];
        if (entry == 0xfe) {
            int seen = pickup_item_seen(0);
            if (seen != 0) {
                g_pickupKeyItemList[i + state[7] * 0x10] = 0;
                return;
            }
            seen = pickup_item_seen(1);
            if (seen != 0) {
                g_pickupKeyItemList[i + state[7] * 0x10] = 1;
                return;
            }
        }
    }
}

// 0x00482710 - load a pickup-screen texture file and page it in. mode 0 loads
// the file only; mode 1/2 additionally LoadTexturePage with the page offset at
// g_pickupPageOffsets[mode*0xc/4] (0x1c / 0x1d).
//
// The original loads into 0x00d022ac (g_TimImageBuffer__bitmap + 0x10000),
// NOT the PAK buffer - verified against the 0x004827c0 disassembly, which
// pushes 0xd022ac for both the unpack destination and the LoadTexturePage
// source. The PAK buffer (0x00aea0d0) is only for the filem_*.pix container.
static void pickup_load_texture(const char* path, unsigned int texId, int mode)
{
    void* loadBuffer = (void*)((int)g_TimImageBuffer__bitmap + 0x10000);
    if (SUBMENU_STATE_ID != texId) {
        SUBMENU_STATE_ID = (unsigned char)texId;
        // empty_483510(): returns 0 in the original - call dropped
        LoadFile(path, loadBuffer, 0x20);
        if (mode == 0) {
            // 0x00482783 / 0x0048278d: mode 0 only clears the loaded flag and
            // then FALLS THROUGH to the same slot-0xd page load as mode 1.
            // That is how the file book covers (file000/001.tim) get paged
            // into SRV slot 0x1c (clut 0x1fc) - the whole book UI (page,
            // cursor, slot icons, book indicators, scroll arrows) is baked
            // into the cover at texU 0xa0-0xe8. Skipping the page load (the
            // port's earlier structure) left the book selector with nothing
            // to draw.
            DAT_004d2a38 = 0;
        } else if (mode != 1) {
            LoadTexturePage(loadBuffer, 0x15,
                            (short)g_pickupPageOffsets[(mode * 0xc) / 4],
                            0xe, 0, 0, 0, 0);
            DAT_004d2a38 = 1;
            return;
        }
        LoadTexturePage(loadBuffer, 0x15,
                        (short)g_pickupPageOffsets[0xc / 4],
                        0xd, 0, 0, 0, 0);
    }
}

// 0x004827c0 - unpack one page of the filem PAK file and page it in. The
// decompressed page (a TIM image) lands at g_TimImageBuffer__bitmap + 0x10000
// and LoadTexturePage parses THAT, not the PAK buffer (0x004827c0 disasm:
// PUSH 0xd022ac before both calls).
static void pickup_unpack_list(int index)
{
    void* pageBuffer = (void*)((int)g_TimImageBuffer__bitmap + 0x10000);
    unpack_pakfile_(g_bgPakLoadBuffer + *(int*)(g_bgPakLoadBuffer + index * 4),
                    pageBuffer);
    LoadTexturePage(pageBuffer, 0x15, (short)g_pickupPageOffsets[6],
                    0xe, 0, 0, 0, 0);
}

// 0x004823a0 - the list renderer state machine (defined below)
static void pickup_screen_render(unsigned char* state);

// 0x00482290 - pickup list init: cursor to 0, clear the no-more flag, run the
// renderer from state 0 (loads the arrow texture)
static void pickup_list_init(unsigned char* state)
{
    state[6] = 0;
    state[0x10] = 0;
    pickup_screen_render(state);
}

// 0x004822b0 - pickup list input: up/down moves the cursor through the item
// list; confirm selects (b9=1, render state 5/8 slide-out) or, at the last
// item, raises the no-more flag (c0=1) so the caller can finish.
static void pickup_list_input(unsigned char* state)
{
    unsigned char count = g_itemMaxCounts[g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + state[5]]];
    unsigned char* hold;

    if (state[9] != 0) goto pickup_input_draw;

    hold = &state[0x13];
    if (((unsigned short)g_button_pressed_id & 0xa000) == 0) {
        *hold = 0;
    } else {
        *hold = *hold + 1;
    }
    if ((((unsigned short)g_PlayerPadHeld & 0x2000) != 0) ||
        ((((unsigned short)g_button_pressed_id & 0x2000) != 0 && (0x14 < *hold)))) {
        if ((int)state[6] + 1U < (unsigned int)count) {
            state[9] = 1;
            state[2] = 5;
            play_sfx(3, 8, 0);
        } else {
            if (state[0x10] != 0) goto pickup_input_mark;
            state[0x10] = 1;
            play_sfx(3, 4, 0);
        }
    }
pickup_input_mark:
    if ((((unsigned short)g_PlayerPadHeld & 0x8000) != 0) ||
        ((((unsigned short)g_button_pressed_id & 0x8000) != 0 && (0x14 < *hold)))) {
        if (state[0x10] == 1) {
            state[0x10] = 0;
            play_sfx(3, 4, 0);
        } else {
            if ((int)state[6] - 1 < 0) {
                state[6] = 0;
                goto pickup_input_draw;
            }
            state[9] = 1;
            state[2] = 8;
            play_sfx(3, 8, 0);
        }
    }
pickup_input_draw:
    pickup_screen_render(state);
}

// 0x00482250 - pickup list advance: first call initializes (b1 0->1), then
// runs the input each frame. Also forces the fade overlay to full black so
// the list renders over it.
static void pickup_list_advance(unsigned char* state)
{
    pickup_fade_update(2);
    if (state[1] == 0) {
        pickup_list_init(state);
        state[1] = state[1] + 1;
        return;
    }
    if (state[1] != 1) {
        return;
    }
    pickup_list_input(state);
}

// 0x00482b80 - reset the pickup-message state block
static void menu_reset_status_state(void)
{
    pickup_screen_init_table();
    for (int i = 0; i < 0x14; i++) DAT_00ac98b0[i] = 0;
}

// 0x004828f0 - initialize the status/pickup screen
static void menu_init_status_screen(void)
{
    pickup_screen_init_table();
    menu_reset_status_state();
    DAT_00ac98b0[9] = 1;
}

// 0x00482910 - update the pickup-message screen; returns non-zero (0xffffffff)
// when the pickup completes and the menu can close.
static int menu_update_status_screen(void)
{
    unsigned char* state = DAT_00ac98b0;

    if (state[0] == 0) {
        if ((state[1] == 0) && (pickup_fade_update(0) != 0)) {
            state[0] = 1;
            state[7] = (g_main_state_flags & MSF_CHAR_VARIANT) != 0;
            unsigned char itemId = *(unsigned char*)(*(unsigned char**)((int)g_pRoomActionEntry + 8) + 8);
            unsigned char uVar3 = itemId - 0x5f;
            file_set_collected_flag(uVar3);
            pickup_mark_seen(state);
            play_sfx(3, 6, 0);
            for (unsigned int i = 0; i < 0x10; i++) {
                if (g_pickupKeyItemList[i + state[7] * 0x10] == uVar3) {
                    state[4] = (unsigned char)(i >> 3);
                    state[5] = (unsigned char)(i & 7);
                    state[6] = 0;
                    state[9] = 1;
                    state[0x10] = 0;
                }
            }
        }
        if (state[1] == 1) {
            if (pickup_fade_update(1) != 0) {
                state[8] = 1;
            }
            if (state[3] == 2) {
                g_playerEntity.animationId = 1;
                g_playerEntity.animFrameId = 0;
                g_playerEntity.action_behavior = 0;
                g_playerEntity.action_state = 0;
                g_playerEntity.unk_8c = 0;
                g_playerEntity.animation_frame_id = 0;
                g_playerEntity.unk_bf = 0;
                g_playerEntity.attackAnim = 0;
                Joint_move(0, g_playerEntity.animHeader, g_playerEntity.animBase, 0x400);
                state[3] = 0;
                g_selectedItemId = *(unsigned char*)(*(unsigned char**)((int)g_pRoomActionEntry + 8) + 8);
                set_message_display(0xc6, 0);
            }
        }
    } else if ((state[0] == 1) && (file_reader_advance(state), state[9] == 0)) {
        if ((dpad_pressed_byte1() & 0x80) != 0) {
            state[2] = 0xd;
            state[9] = 1;
            play_sfx(3, 5, 0);
        }
        if (((dpad_pressed_byte1() & 0x40) != 0) && (state[0x10] == 1)) {
            state[2] = 0xd;
            state[9] = 1;
            play_sfx(3, 5, 0);
        }
    }

    if ((state[8] == 1) && ((g_menu_choice_id & 0x80) == 0)) {
        // Pickup complete: consume the entry and clear its flags.
        unsigned char* evt = (unsigned char*)g_pRoomActionEntry;
        unsigned char* record = *(unsigned char**)(evt + 8);
        ((unsigned char*)g_item_model_table[record[10]])[0] = 0;
        *evt = 0;
        FUN_00473f10((int*)&g_roomItemsFlags, record[0x14]);
        g_pickedItemId = record[8];
        state[8] = 0;
        return 0xffffffff;
    }
    return 0;
}

// 0x004823a0 - pickup list renderer state machine: loads the list textures,
// slides the list in/out per the cursor, and draws the item name strip.
static void pickup_screen_render(unsigned char* state)
{
    switch (state[2]) {
    case 0:
        pickup_load_texture(GAME_DATA_ROOT "item_m2/arror.tim", 0x96, 1);
        *(unsigned short*)(state + 0xc) = 0x128;
        state[9] = 1;
        state[2] = 1;
        *(unsigned short*)(state + 0xe) = 0;
        // fall through
    case 1:
        // The file to read = table[(book + char*2)*8 + slot] - the book is
        // state[4] (0x004823e7 in the original: MOVSX EAX,[ESI+4]), which the
        // Ghidra decomp mislabeled as state[1]. The port's earlier state[1]
        // made book 2's files load regardless of the selected book.
        LoadFile((const char*)&g_filemPixNames[g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + state[5]]],
                 g_bgPakLoadBuffer, 0x20);
        state[2] = 2;
        return;
    case 2:
        pickup_unpack_list((int)state[6]);
        *(unsigned short*)(state + 0xc) = 0x128;
        state[2] = 3;
        *(unsigned short*)(state + 0xa) = 300;
        // fall through
    case 3:
        {
            short sVar5 = *(short*)(state + 0xa) / 2;
            short sVar4 = *(short*)(state + 0xc) - sVar5;
            *(short*)(state + 0xa) = sVar5;
            *(short*)(state + 0xc) = sVar4;
            if (sVar4 < 1) {
                *(unsigned short*)(state + 0xc) = 0;
                *(unsigned short*)(state + 0xa) = 1;
                state[2] = state[2] + 1;
            }
        }
        break;
    case 4:
        state[9] = 0;
        break;
    case 5:
        *(unsigned short*)(state + 0xc) = 0;
        *(unsigned short*)(state + 0xa) = 1;
        state[2] = state[2] + 1;
        // fall through
    case 6:
        {
            short sVar5 = *(short*)(state + 0xc) - *(short*)(state + 0xa);
            *(short*)(state + 0xc) = sVar5;
            *(short*)(state + 0xa) = *(short*)(state + 0xa) * 2;
            if (sVar5 < -0x127) {
                state[2] = state[2] + 1;
                state[6] = state[6] + 1;
            }
        }
        break;
    case 7:
        state[2] = 2;
        break;
    case 8:
        *(unsigned short*)(state + 0xc) = 0;
        *(unsigned short*)(state + 0xa) = 1;
        state[2] = state[2] + 1;
        // fall through
    case 9:
        {
            short sVar5 = *(short*)(state + 0xc) + *(short*)(state + 0xa);
            *(short*)(state + 0xc) = sVar5;
            *(short*)(state + 0xa) = *(short*)(state + 0xa) * 2;
            if (0x127 < sVar5) {
                state[2] = state[2] + 1;
                state[6] = state[6] - 1;
            }
        }
        break;
    case 0x0a:
        state[2] = state[2] + 1;
        break;
    case 0x0b:
        pickup_unpack_list((int)state[6]);
        *(unsigned short*)(state + 0xc) = 0xfed8;
        state[2] = state[2] + 1;
        *(unsigned short*)(state + 0xa) = 300;
        // fall through
    case 0x0c:
        {
            short sVar5 = *(short*)(state + 0xa) / 2;
            short sVar4 = *(short*)(state + 0xc) + sVar5;
            *(short*)(state + 0xa) = sVar5;
            *(short*)(state + 0xc) = sVar4;
            if (-1 < sVar4) {
                state[2] = 4;
                *(unsigned short*)(state + 0xc) = 0;
            }
        }
        break;
    case 0x0d:
        *(unsigned short*)(state + 0xc) = 0;
        *(unsigned short*)(state + 0xa) = 1;
        state[2] = state[2] + 1;
        // fall through
    case 0x0e:
        {
            short sVar5 = *(short*)(state + 0xc) - *(short*)(state + 0xa);
            *(short*)(state + 0xc) = sVar5;
            *(short*)(state + 0xa) = *(short*)(state + 0xa) * 2;
            if (!(-0x128 < sVar5)) {
                state[2] = state[2] + 1;
            }
        }
        break;
    case 0x0f:
    case 0x10:
        state[2] = state[2] + 1;
        break;
    case 0x11:
        // The cover to load is the book the reader came from (state[4]),
        // not state[1] - the original indexes 0x4d2308 with state[4]*0x24.
        file_load_book_cover(state[4]);
        state[2] = state[2] + 1;
        break;
    case 0x12:
        // b0=0, b1=1, b2=1, b3=2 - the "done" handoff to menu_update_status_screen
        *(unsigned int*)state = 0x02010100;
        break;
    }

    // Item-name strip position follows the slide (0x0048258e also clears the
    // strip's texV before the draw)
    *(short*)(DAT_004d2888 + 4) = *(short*)(state + 0xc) + 0x18;
    *(short*)(DAT_004d2888 + 6) = *(short*)(state + 0xe) + 0x18;
    DAT_004d2888[0x0f] = 0;
    display_texture((TextureDesc*)DAT_004d2888, 0, 0xe, 1);

    if (state[2] == 4) {
        unsigned char blink = state[0x12] + 1;
        state[0x12] = blink;
        state[0x11] = ((blink & 0x30) == 0);
        // On the last page (state[0x10] = "no more pages") the arrows stay
        // lit - 0x00482607 compares state[0x10], which the Ghidra decomp
        // mislabeled as state[4].
        if (state[0x10] == 1) {
            state[0x12] = 0;
            state[0x11] = 1;
        }
        DAT_004d2988[0xe] = (unsigned char)(state[0x11] << 3);
        DAT_004d29ac[0xe] = (unsigned char)(state[0x11] * 8 + 0x10);
        if (state[6] != 0) {
            display_texture((TextureDesc*)DAT_004d2988, 0, 0xd, 1);
        }
        display_texture((TextureDesc*)DAT_004d29ac, 0, 0xd, 1);
        if ((unsigned int)g_itemMaxCounts[g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + state[5]]] -
            (unsigned int)state[6] == 1) {
            // 0x00482685 compares state[0x10] (the last-page flag), not
            // state[4]: the EXIT label lights up (0x50) once the reader has
            // acknowledged the last page and dims (0x20) until then.
            if (state[0x10] == 1) {
                DAT_004d29d0[0x14] = 0x50;
            } else {
                DAT_004d29d0[0x14] = 0x20;
            }
            DAT_004d29d0[0x15] = DAT_004d29d0[0x14];
            DAT_004d29d0[0x16] = DAT_004d29d0[0x14];
            draw_texture((TextureDesc*)DAT_004d29d0, 1);
        }
    }
}

// ============================================================================
// Japanese (Biohazard.exe) file subsystem
//
// The FILE tab is shared with the USA build, but the Japanese reader draws a
// scanned "file" backdrop behind each document instead of the USA's plain page
// art, and stores every page as its own uncompressed TIM rather than one LZW
// page-pack per document. The book selector is otherwise identical (same cover
// art and descriptors); only its title x table and scroll-arrow x move.
//
// Reader page model: each TEXTM_<letter><n>.TIM is 128x256 and packs TWO
// 256x128 pages, so state[6] counts half-pages. The page texture's texV is 0
// for the first page of a TIM and 0x80 for the second, and a new TIM is paged
// in only when state[6]/2 advances. All addresses below are Biohazard.exe.
// ============================================================================

// 0x004af1d8 - per-file FILEI backdrop index (into g_jpnFileiNames). The FILEI
// string table skips filei01/02/05, so this index, not the file id, selects it.
static const unsigned char g_jpnFileiIndex[16] = {
    1, 0, 2, 3, 8, 12, 3, 5, 7, 0, 4, 11, 6, 9, 10, 13,
};
// 0x004af1e8 - per-file reading length in half-pages. Both 2n and 2n-1 values
// display n pages; the parity only decides which half of the last TIM is last.
static const unsigned char g_jpnFilePageCount[16] = {
    4, 6, 11, 3, 2, 6, 7, 7, 8, 5, 5, 4, 2, 2, 2, 5,
};
// 0x004af1f8 - per-file title x offset (the USA table is g_itemMaxCounts+0x10).
static const unsigned char g_jpnFileTitleX[16] = {
    42, 42, 42, 21, 42, 21, 49, 28, 0, 42, 14, 42, 35, 35, 35, 63,
};
// 0x004af5c0 - per-file start index into the TEXTM page table.
static const unsigned char g_jpnFilePageStart[16] = {
    0, 2, 5, 11, 13, 14, 17, 21, 25, 29, 32, 35, 37, 38, 39, 40,
};

// 0x004ae9d0 - the 43 per-page TIM names, fixed 0x24-byte entries in the order
// the original indexes them (L M N O P Q R S T U V W X Y Z 00, page by page).
static const char g_jpnTextmNames[43][0x24] = {
    GAME_DATA_ROOT_JPN "item_m2/textm_l0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_l1.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_m0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_m1.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_m2.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_n0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_n1.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_n2.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_n3.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_n4.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_n5.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_o0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_o1.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_p0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_q0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_q1.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_q2.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_r0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_r1.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_r2.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_r3.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_s0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_s1.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_s2.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_s3.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_t0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_t1.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_t2.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_t3.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_u0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_u1.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_u2.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_v0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_v1.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_v2.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_w0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_w1.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_x0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_y0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_z0.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_00.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_01.tim",
    GAME_DATA_ROOT_JPN "item_m2/textm_02.tim",
};

// 0x004aefe0 - the per-file backdrop TIMs, fixed 0x24-byte entries. The string
// table has no filei01/02/05; those tree entries are unused by the reader.
static const char g_jpnFileiNames[14][0x24] = {
    GAME_DATA_ROOT_JPN "item_m2/filei03.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei04.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei06.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei07.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei08.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei09.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei10.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei11.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei12.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei13.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei14.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei15.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei16.tim",
    GAME_DATA_ROOT_JPN "item_m2/filei17.tim",
};

// 0x004af450 - the reader page texture (256x128). texV is toggled to 0x80 on
// odd half-pages so the two pages packed in each TEXTM TIM alternate.
static unsigned char DAT_004af450[0x24] = {
    0x00,0x00,0x00,0x01, 0x18,0x00,0x34,0x00, 0x00,0x01,0x80,0x00,
    0x15,0x00,0x00,0x00, 0x00,0x00,0xfd,0x01, 0x80,0x80,0x80,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};

// 0x004af478 - six fixed backdrop frame/scrap pieces drawn over the page
// (stride 0x24); their colour is ramped 0..0x80 by DAT_004af48c.
static unsigned char g_jpnReaderFrame[6][0x24] = {
    { 0x00,0x00,0x00,0x51, 0x30,0x00,0x30,0x00, 0xd0,0x00,0x78,0x00,
      0x15,0x00,0x00,0x00, 0x00,0x00,0xfc,0x01, 0x00,0x00,0x00,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
    { 0x00,0x00,0x00,0x51, 0x30,0x00,0xa8,0x00, 0x30,0x00,0x18,0x00,
      0x15,0x00,0xd0,0x00, 0x00,0x00,0xfc,0x01, 0x00,0x00,0x00,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
    { 0x00,0x00,0x00,0x51, 0x60,0x00,0xa8,0x00, 0x30,0x00,0x18,0x00,
      0x15,0x00,0xd0,0x18, 0x00,0x00,0xfc,0x01, 0x00,0x00,0x00,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
    { 0x00,0x00,0x00,0x51, 0x90,0x00,0xa8,0x00, 0x30,0x00,0x18,0x00,
      0x15,0x00,0xd0,0x30, 0x00,0x00,0xfc,0x01, 0x00,0x00,0x00,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
    { 0x00,0x00,0x00,0x51, 0xc0,0x00,0xa8,0x00, 0x30,0x00,0x18,0x00,
      0x15,0x00,0xd0,0x48, 0x00,0x00,0xfc,0x01, 0x00,0x00,0x00,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
    { 0x00,0x00,0x00,0x51, 0xf0,0x00,0xa8,0x00, 0x10,0x00,0x18,0x00,
      0x15,0x00,0xd0,0x60, 0x00,0x00,0xfc,0x01, 0x00,0x00,0x00,0x00,
      0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 },
};

// 0x004af550 / 0x004af574 - previous / next page arrows. texU blinks.
static unsigned char DAT_004af550[0x24] = {
    0x00,0x00,0x00,0x01, 0x18,0x00,0x74,0x00, 0x08,0x00,0x08,0x00,
    0x15,0x00,0xe0,0x60, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};
static unsigned char DAT_004af574[0x24] = {
    0x00,0x00,0x00,0x01, 0x10,0x01,0x74,0x00, 0x08,0x00,0x08,0x00,
    0x15,0x00,0xf0,0x60, 0x00,0x00,0xfc,0x01, 0x80,0x80,0x80,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};
// 0x004af598 - the EXIT label on the last page.
static unsigned char DAT_004af598[0x24] = {
    0x00,0x00,0x00,0x01, 0x18,0x01,0x6f,0x00, 0x18,0x00,0x10,0x00,
    0x1c,0x00,0x80,0x88, 0x00,0x00,0xe4,0x01, 0x40,0x40,0x40,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
};

static unsigned char DAT_004af48c;   // 0x004af48c - backdrop fade (0 -> 0x80 -> 0)
static unsigned char DAT_004af600;   // 0x004af600 - a TEXTM page has been paged in
static unsigned char DAT_00c2e93d;   // 0x00c2e93d - JPN "texture already loaded" id

// 0x0042c510 - JPN file texture loader. mode 0/1 page the image into SRV 0x1c
// (clut 0xd); mode 2 pages it into 0x1d (clut 0xe) and marks the page loaded.
// FUN_00437dd0(0) is an empty stub in the original and is dropped.
static void jpn_file_load_texture(const char* path, unsigned int texId, int mode)
{
    void* loadBuffer = (void*)((int)g_TimImageBuffer__bitmap + 0x10000);
    if ((unsigned int)DAT_00c2e93d != texId) {
        DAT_00c2e93d = (unsigned char)texId;
        LoadFile(path, loadBuffer, 0x20);
        if (mode == 0) {
            DAT_004af600 = 0;
        } else if (mode != 1) {
            LoadTexturePage(loadBuffer, 0x15, 0x1d, 0xe, 0, 0, 0, 0);
            DAT_004af600 = 1;
            return;
        }
        LoadTexturePage(loadBuffer, 0x15, 0x1c, 0xd, 0, 0, 0, 0);
    }
}

// The shared book selector loads its cover through this; the JPN build resets
// the reader's "page loaded" flag here (jpn_file_load_texture mode 0) so no
// stale page is drawn while the reader fades in.
static void file_load_book_cover(unsigned char book)
{
    if (GetAssetVersion() != 0) {
        jpn_file_load_texture(g_fileCoverNames[book], book + 0x90, 0);
    } else {
        pickup_load_texture(g_fileCoverNames[book], book + 0x90, 0);
    }
}

// 0x0042c080 - JPN file reading renderer/state machine. Same shape as the USA
// pickup_screen_render (0x004823a0), but pages are per-page TEXTM TIMs and a
// FILEI backdrop + frame is drawn behind the page.
static void jpn_file_screen_render(unsigned char* state)
{
    switch (state[2]) {
    case 0:
        {
            unsigned char entry = g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + state[5]];
            unsigned char fi = g_jpnFileiIndex[entry];
            jpn_file_load_texture(g_jpnFileiNames[fi], fi + 0x96, 1);
        }
        DAT_004af48c = 0;
        *(unsigned short*)(state + 0xe) = 0;
        state[2] = state[2] + 1;
        *(unsigned short*)(state + 0xc) = 0x128;
        state[9] = 1;
        // fall through
    case 1:
        DAT_004af48c = DAT_004af48c + 8;
        if (DAT_004af48c < 0x80) return;
        DAT_004af48c = 0x80;
        state[2] = state[2] + 1;
        return;
    case 2:
        if (state[6] % 2 == 0) {
            unsigned char entry = g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + state[5]];
            int idx = state[6] / 2 + g_jpnFilePageStart[entry];
            jpn_file_load_texture(g_jpnTextmNames[idx], idx + 0xb4, 2);
        }
        *(unsigned short*)(state + 0xc) = 0x128;
        *(unsigned short*)(state + 0xa) = 300;
        state[2] = state[2] + 1;
        // fall through
    case 3:
        {
            short sVar5 = *(short*)(state + 0xa) / 2;
            short sVar4 = *(short*)(state + 0xc) - sVar5;
            *(short*)(state + 0xa) = sVar5;
            *(short*)(state + 0xc) = sVar4;
            if (sVar4 < 1) {
                *(unsigned short*)(state + 0xc) = 0;
                *(unsigned short*)(state + 0xa) = 1;
                state[2] = state[2] + 1;
            }
        }
        break;
    case 4:
        state[9] = 0;
        break;
    case 5:
        *(unsigned short*)(state + 0xc) = 0;
        *(unsigned short*)(state + 0xa) = 1;
        state[2] = state[2] + 1;
        // fall through
    case 6:
        {
            short sVar5 = *(short*)(state + 0xc) - *(short*)(state + 0xa);
            *(short*)(state + 0xc) = sVar5;
            *(short*)(state + 0xa) = *(short*)(state + 0xa) * 2;
            if (sVar5 < -0x127) {
                state[2] = state[2] + 1;
                state[6] = state[6] + 1;
            }
        }
        break;
    case 7:
        state[2] = 2;
        break;
    case 8:
        *(unsigned short*)(state + 0xc) = 0;
        *(unsigned short*)(state + 0xa) = 1;
        state[2] = state[2] + 1;
        // fall through
    case 9:
        {
            short sVar5 = *(short*)(state + 0xc) + *(short*)(state + 0xa);
            *(short*)(state + 0xc) = sVar5;
            *(short*)(state + 0xa) = *(short*)(state + 0xa) * 2;
            if (0x127 < sVar5) {
                state[2] = state[2] + 1;
                state[6] = state[6] - 1;
            }
        }
        break;
    case 0x0a:
        state[2] = state[2] + 1;
        break;
    case 0x0b:
        if (state[6] % 2 != 0) {
            unsigned char entry = g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + state[5]];
            int idx = state[6] / 2 + g_jpnFilePageStart[entry];
            jpn_file_load_texture(g_jpnTextmNames[idx], idx + 0xb4, 2);
        }
        *(unsigned short*)(state + 0xc) = 0xfed8;
        *(unsigned short*)(state + 0xa) = 300;
        state[2] = state[2] + 1;
        // fall through
    case 0x0c:
        {
            short sVar5 = *(short*)(state + 0xa) / 2;
            short sVar4 = *(short*)(state + 0xc) + sVar5;
            *(short*)(state + 0xa) = sVar5;
            *(short*)(state + 0xc) = sVar4;
            if (-1 < sVar4) {
                state[2] = 4;
                *(unsigned short*)(state + 0xc) = 0;
            }
        }
        break;
    case 0x0d:
        *(unsigned short*)(state + 0xc) = 0;
        *(unsigned short*)(state + 0xa) = 1;
        state[2] = state[2] + 1;
        // fall through
    case 0x0e:
        {
            short sVar5 = *(short*)(state + 0xc) - *(short*)(state + 0xa);
            *(short*)(state + 0xc) = sVar5;
            *(short*)(state + 0xa) = *(short*)(state + 0xa) * 2;
            if (sVar5 < -0x127) {
                state[2] = state[2] + 1;
            }
        }
        break;
    case 0x0f:
        DAT_004af48c = DAT_004af48c - 8;
        if (DAT_004af48c == 0) {
            DAT_004af48c = 0;
            state[2] = state[2] + 1;
        }
        break;
    case 0x10:
        state[2] = state[2] + 1;
        break;
    case 0x11:
        // Reload the cover of the book the reader came from.
        if (GetAssetVersion() != 0) {
            jpn_file_load_texture(g_fileCoverNames[state[4]], state[4] + 0x90, 0);
        }
        state[2] = state[2] + 1;
        break;
    case 0x12:
        *(unsigned int*)state = 0x02010100;
        break;
    }

    // Page texture follows the slide; texV selects the half of the TEXTM TIM.
    *(short*)(DAT_004af450 + 4) = *(short*)(state + 0xc) + 0x18;
    *(short*)(DAT_004af450 + 6) = *(short*)(state + 0xe) + 0x34;
    if (DAT_004af600 != 0) {
        if (state[6] % 2 == 0) {
            DAT_004af450[0x0f] = 0;
        } else {
            DAT_004af450[0x0f] = 0x80;
        }
        display_texture((TextureDesc*)DAT_004af450, 0, 0xe, 1);
    }

    // Backdrop/frame pieces, faded in with DAT_004af48c.
    for (int i = 0; i < 6; i++) {
        unsigned char* desc = g_jpnReaderFrame[i];
        desc[0x14] = DAT_004af48c;
        desc[0x15] = DAT_004af48c;
        desc[0x16] = DAT_004af48c;
        display_texture((TextureDesc*)desc, 1, 0xd, 1);
    }

    if (state[2] == 4) {
        unsigned char blink = state[0x12] + 1;
        state[0x12] = blink;
        state[0x11] = ((blink & 0x30) == 0);
        if (state[0x10] == 1) {
            state[0x12] = 0;
            state[0x11] = 1;
        }
        DAT_004af550[0x0e] = (unsigned char)(state[0x11] * 8 - 0x20);
        DAT_004af574[0x0e] = (unsigned char)(state[0x11] * 8 - 0x10);
        if (state[6] != 0) {
            display_texture((TextureDesc*)DAT_004af550, 0, 0xd, 1);
        }
        display_texture((TextureDesc*)DAT_004af574, 0, 0xd, 1);
        if ((unsigned int)g_jpnFilePageCount[g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + state[5]]] -
            (unsigned int)state[6] == 1) {
            if (state[0x10] == 1) {
                DAT_004af598[0x14] = 0x50;
            } else {
                DAT_004af598[0x14] = 0x20;
            }
            DAT_004af598[0x15] = DAT_004af598[0x14];
            DAT_004af598[0x16] = DAT_004af598[0x14];
            draw_texture((TextureDesc*)DAT_004af598, 1);
        }
    }
}

// 0x0042bf70 - JPN reader init: reset the half-page counter and last-page flag.
static void jpn_file_list_init(unsigned char* state)
{
    state[6] = 0;
    state[0x10] = 0;
    jpn_file_screen_render(state);
}

// 0x0042bf90 - JPN reader input. Same as the USA pickup_list_input but the
// length comes from g_jpnFilePageCount and is counted in half-pages.
static void jpn_file_list_input(unsigned char* state)
{
    unsigned char count = g_jpnFilePageCount[g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + state[5]]];
    unsigned char* hold;

    if (state[9] != 0) goto jpn_input_draw;

    hold = &state[0x13];
    if (((unsigned short)g_button_pressed_id & 0xa000) == 0) {
        *hold = 0;
    } else {
        *hold = *hold + 1;
    }
    if ((((unsigned short)g_PlayerPadHeld & 0x2000) != 0) ||
        ((((unsigned short)g_button_pressed_id & 0x2000) != 0 && (0x14 < *hold)))) {
        if ((int)state[6] + 1U < (unsigned int)count) {
            state[9] = 1;
            state[2] = 5;
            play_sfx(3, 8, 0);
        } else {
            if (state[0x10] != 0) goto jpn_input_mark;
            state[0x10] = 1;
            play_sfx(3, 4, 0);
        }
    }
jpn_input_mark:
    if ((((unsigned short)g_PlayerPadHeld & 0x8000) != 0) ||
        ((((unsigned short)g_button_pressed_id & 0x8000) != 0 && (0x14 < *hold)))) {
        if (state[0x10] == 1) {
            state[0x10] = 0;
            play_sfx(3, 4, 0);
        } else {
            if ((int)state[6] - 1 < 0) {
                state[6] = 0;
                goto jpn_input_draw;
            }
            state[9] = 1;
            state[2] = 8;
            play_sfx(3, 8, 0);
        }
    }
jpn_input_draw:
    jpn_file_screen_render(state);
}

// 0x0042bf30 - JPN reader dispatch (hides the page behind the black overlay,
// then init on the first call and input thereafter).
static void jpn_file_list_advance(unsigned char* state)
{
    pickup_fade_update(2);
    if (state[1] == 0) {
        jpn_file_list_init(state);
        state[1] = state[1] + 1;
        return;
    }
    if (state[1] != 1) {
        return;
    }
    jpn_file_list_input(state);
}

// Dispatch to the correct reader advance. The pause-menu File tab and the room
// file pickup both drive the reader through here.
static void file_reader_advance(unsigned char* state)
{
    if (GetAssetVersion() != 0) {
        jpn_file_list_advance(state);
    } else {
        pickup_list_advance(state);
    }
}

// 0x0042b7c0 / 0x0042b9e0 / 0x0042b870 / 0x0042b790 - the JPN book selector is
// byte-identical to the USA one apart from its title x table and scroll-arrow x
// (handled where FUN_00481d00 draws them), so the USA selector is reused. This
// wrapper only marks the JPN entry point and reports which sub-step to run.
static void jpn_file_book_advance(unsigned char* state)
{
    FUN_00481ab0(state);
}

// ============================================================================
// File book selector (0x00481ab0 family)
//
// The FILE tab (menu_file_state_machine / 0x00481980) drives this while
// FILE_STATE (state[0]) is 0: a book selector showing Book 1 or Book 2
// (file000.tim / file001.tim covers). Each book has 8 file slots; a slot is
// only selectable when its room flag is raised (pickup_item_seen). Pressing
// confirm (byte1 & 0x40) with a slot selected sets the MODE byte (state[3])
// to 1, which makes the renderer fade to white and set FILE_STATE = 1 - the
// handoff to the file READING mode (pickup_list_advance / pickup_screen_render,
// which also handles files found in rooms via menu_update_status_screen).
//
// State block (0x00ac98b0 - the same block as the FILE_* macros):
//   +0x00 FILE_STATE (0 = book, 1 = reading)   +0x01 sub-step (0/1)
//   +0x02 REQUEST/sub-state                    +0x03 MODE byte (fade mode)
//   +0x04 book index (0/1)                     +0x05 selected slot (0-7, ff)
//   +0x07 Chris/Jill half (g_main_state_flags bit 0x800000)
//   +0x08 EXITFLAG                             +0x09 BUSY
//   +0x0a slide speed (short)                  +0x0c slide X (short)
//   +0x0e slide Y (short)                      +0x11 blink phase
//   +0x12 blink counter                        +0x13 button hold counter
// ============================================================================

// (0x00481d00) - File book renderer + state machine. Sub-states:
//   0 = slide in, 1 = display, 2 = cancel slide out (EXITFLAG on completion),
//   3-7 = book change right (slide, next book, cover, slide back),
//   8-12 = book change left. MODE 1/2 = fade in/out (FILE_STATE handoff).
// The render block runs in the display state (slots, cursor, title, arrows).
static void FUN_00481d00(unsigned char* state)
{
    switch (state[2]) {
    case 0:
        // Slide in: Y eases from 0xff81 (-127) up to 0
        {
            short speed = *(short*)(state + 0xa) / 2;
            short y = *(short*)(state + 0xe) + speed;
            *(short*)(state + 0xa) = speed;
            *(short*)(state + 0xe) = y;
            if (y < 0) break;
            *(short*)(state + 0xe) = 0;
            *(short*)(state + 0xa) = 1;
            state[2] = state[2] + 1;
        }
        break;
    case 1:
        // Display: release the busy lock so input runs
        state[9] = 0;
        *(short*)(state + 0xa) = 1;
        break;
    case 2:
        // Cancel: slide out, then raise the exit flag and clear the block.
        // (The disassembly writes state[8]=1 and a DWORD 0 at state+0; the
        // decompiler's "state[2]=1; state[0]=0" reading was wrong.)
        {
            short speed = *(short*)(state + 0xa);
            short y = *(short*)(state + 0xe) - speed;
            *(short*)(state + 0xe) = y;
            *(short*)(state + 0xa) = speed * 2;
            if (y > -0x68) break;
            state[8] = 1;
            *(unsigned int*)state = 0;
        }
        break;
    case 3:
        // Book change right: lock input and start the slide
        state[9] = 1;
        *(short*)(state + 0xa) = 1;
        state[2] = state[2] + 1;
        // fall through
    case 4:
        // Slide the current book out to the left
        {
            short speed = *(short*)(state + 0xa);
            short x = *(short*)(state + 0xc) - speed;
            *(short*)(state + 0xc) = x;
            *(short*)(state + 0xa) = speed * 2;
            if (x > -0x78) break;
            *(short*)(state + 0xc) = 0x7f;
            *(short*)(state + 0xa) = 0x80;
            state[2] = state[2] + 1;
        }
        break;
    case 5:
        // Enter the next book: find the first seen file slot
        state[4] = state[4] + 1;
        {
            unsigned char book = state[4];
            int i = 0;
            while (i < 8) {
                if (pickup_item_seen(g_pickupKeyItemList[((int)book + state[7] * 2) * 8 + i]) != 0) {
                    state[5] = (unsigned char)i;
                    break;
                }
                if (i == 7) {
                    state[5] = 0xff;
                }
                i = i + 1;
            }
            state[2] = state[2] + 1;
        }
        break;
    case 6:
        // Load the new book cover
        file_load_book_cover(state[4]);
        state[2] = state[2] + 1;
        break;
    case 7:
        // Slide the new book in from the right
        {
            short speed = *(short*)(state + 0xa) / 2;
            short x = *(short*)(state + 0xc) - speed;
            *(short*)(state + 0xa) = speed;
            *(short*)(state + 0xc) = x;
            if (x > 0) break;
            state[2] = 1;
            *(short*)(state + 0xc) = 0;
        }
        break;
    case 8:
        // Book change left: lock input and start the slide
        state[9] = 1;
        *(short*)(state + 0xa) = 1;
        state[2] = state[2] + 1;
        // fall through
    case 9:
        // Slide the current book out to the right
        {
            short speed = *(short*)(state + 0xa);
            short x = *(short*)(state + 0xc) + speed;
            *(short*)(state + 0xc) = x;
            *(short*)(state + 0xa) = speed * 2;
            if (x < 0x78) break;
            *(short*)(state + 0xc) = 0xff81;
            *(short*)(state + 0xa) = 0x80;
            state[2] = state[2] + 1;
        }
        break;
    case 10:
        // Enter the previous book: find the first seen file slot
        state[4] = state[4] - 1;
        {
            unsigned char book = state[4];
            int i = 0;
            while (i < 8) {
                if (pickup_item_seen(g_pickupKeyItemList[((int)book + state[7] * 2) * 8 + i]) != 0) {
                    state[5] = (unsigned char)i;
                    break;
                }
                if (i == 7) {
                    state[5] = 0xff;
                }
                i = i + 1;
            }
            state[2] = state[2] + 1;
        }
        break;
    case 11:
        // Load the previous book cover
        file_load_book_cover(state[4]);
        state[2] = state[2] + 1;
        break;
    case 12:
        // Slide the previous book in from the left
        {
            short speed = *(short*)(state + 0xa) / 2;
            short x = *(short*)(state + 0xc) + speed;
            *(short*)(state + 0xa) = speed;
            *(short*)(state + 0xc) = x;
            if (x < 0) break;
            state[2] = 1;
            *(short*)(state + 0xc) = 0;
        }
        break;
    }

    // MODE byte (state[3]) 1 = fade in and switch to reading mode, 2 = fade
    // out after reading. Both hold the busy lock while fading.
    if (state[3] == 1) {
        if (pickup_fade_update(0) != 0) {
            *(unsigned int*)state = 1;   // FILE_STATE = 1 -> reading mode
        }
        state[9] = 1;
    } else if (state[3] == 2) {
        if (pickup_fade_update(1) != 0) {
            state[3] = 0;
            *(short*)(state + 0xc) = 0;
            *(short*)(state + 0xe) = 0;
        }
        state[9] = 1;
    }

    // ---- Display render (always draws the page; the rest is gated) ----
    if (state[2] == 1) {
        // Blink animation and the "other book" indicator icons. The current
        // book's icon is hidden; the other one blinks by animating its texU.
        unsigned char blink = state[0x12] + 1;
        state[0x12] = blink;
        state[0x11] = ((blink & 0x30) == 0);
        DAT_004d27f4[0x0e] = (unsigned char)(state[0x11] * 8 - 0x50);
        DAT_004d2818[0x0e] = (unsigned char)(state[0x11] * 8 - 0x40);
        if (state[4] != 0) {
            display_texture((TextureDesc*)DAT_004d27f4, 0x28, 0xd, 1);
        }
        if (state[4] != 1) {
            display_texture((TextureDesc*)DAT_004d2818, 0x28, 0xd, 1);
        }
    }

    // The book page texture follows the slide position
    *(short*)(DAT_004d2668 + 4) = *(short*)(state + 0xc) + 0x20;
    *(short*)(DAT_004d2668 + 6) = *(short*)(state + 0xe) + 0x18;
    display_texture((TextureDesc*)DAT_004d2668, 0x28, 0xd, 1);

    if (state[5] != 0xff) {
        // Per-slot page icons for the files that have been found
        for (int i = 0; i < 8; i++) {
            unsigned char entry = g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + i];
            if (pickup_item_seen(entry) != 0) {
                unsigned char* desc = g_fileSlotDescs[i];
                *(short*)(desc + 4) = *(short*)(state + 0xc) + (i >> 1) + 0x8c;
                *(short*)(desc + 6) = *(short*)(state + 0xe) + i * 9 + 0x29;
                display_texture((TextureDesc*)desc, 0x28, 0xd, 1);
            }
        }
        // Cursor (the arror.tim arrow)
        *(short*)(DAT_004d27d0 + 4) = (short)((state[5] >> 1) + *(short*)(state + 0xc) + 0x8c);
        *(short*)(DAT_004d27d0 + 6) = (short)(state[5] * 9 + *(short*)(state + 0xe) + 0x29);
        display_texture((TextureDesc*)DAT_004d27d0, 0x27, 0xd, 1);

        if (state[2] == 1) {
            // File title text (message id = filem index + 0x5f). The x offset
            // table DAT_004d2630 aliases g_itemMaxCounts[0x10] in the USA build;
            // the JPN build has its own table (0x004af1f8) and draws the title
            // one pixel higher (0xc0 vs 0xc1).
            unsigned char entry = g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + state[5]];
            short titleX = (GetAssetVersion() != 0)
                               ? (short)g_jpnFileTitleX[entry]
                               : (short)g_itemMaxCounts[entry + 0x10];
            short titleY = (GetAssetVersion() != 0) ? (short)0xc0 : (short)0xc1;
            FUN_00454fd0(entry + 0x5f, 0x80,
                         (short)(titleX - g_ScreenOffsetX + 0x1e),
                         titleY - g_ScreenOffsetY);

            // Up/down scroll arrows (positions and blink texU). The JPN build
            // places them at x 0x6f instead of 0x67.
            short arrowX = (GetAssetVersion() != 0) ? (short)0x6f : (short)0x67;
            *(short*)(DAT_004d283c + 4) = (short)(arrowX - g_ScreenOffsetX);
            DAT_004d283c[0x0e] = (unsigned char)(state[0x11] * 8 - 0x30);
            *(short*)(DAT_004d283c + 6) = (short)(0xb7 - g_ScreenOffsetY);
            *(short*)(DAT_004d2860 + 4) = (short)(arrowX - g_ScreenOffsetX);
            DAT_004d2860[0x0e] = (unsigned char)(state[0x11] * 8 - 0x20);
            *(short*)(DAT_004d2860 + 6) = (short)(0xcf - g_ScreenOffsetY);

            // Up arrow shows when a seen file exists above the cursor
            for (int i = 0; i < 8; i++) {
                int cand = (int)state[5] - 1 - i;
                if (cand < 0) break;
                if (pickup_item_seen(g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + cand]) != 0) {
                    display_texture((TextureDesc*)DAT_004d283c, 0xa, 0xd, 1);
                    break;
                }
            }
            // Down arrow shows when a seen file exists below the cursor
            for (int i = 0; i < 8; i++) {
                int cand = (int)state[5] + 1 + i;
                if (cand > 7) break;
                if (pickup_item_seen(g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + cand]) != 0) {
                    display_texture((TextureDesc*)DAT_004d2860, 0xa, 0xd, 1);
                    break;
                }
            }
        }
    }
}

// (0x00481ae0) - File book init: reset the state, pick the Chris/Jill half,
// mark new files seen, select the first seen slot, load the book cover.
static void FUN_00481ae0(unsigned char* state)
{
    *(short*)(state + 0xc) = 0;
    state[8] = 0;
    state[4] = 0;
    *(short*)(state + 0xa) = 0x80;
    *(short*)(state + 0xe) = 0xff81;
    state[7] = (g_main_state_flags & MSF_CHAR_VARIANT) != 0;
    pickup_mark_seen(state);

    // First seen file slot of book 1 (0xff = none found yet)
    int i = 0;
    do {
        if (pickup_item_seen(g_pickupKeyItemList[state[7] * 0x10 + i]) != 0) {
            state[5] = (unsigned char)i;
            break;
        }
        if (i == 7) {
            state[5] = 0xff;
        }
        i = i + 1;
    } while (i < 8);

    DAT_004d2a34 = 0;
    DAT_004d2a35 = 0;
    DAT_004d2a36 = 0;
    file_load_book_cover(state[4]);
    FUN_00481d00(state);
}

// (0x00481b90) - File book input: cursor up/down (Triangle/Cross held, with
// auto-repeat) through the seen slots; Circle/Square switch books. The X
// confirm and Square cancel are handled by menu_file_state_machine.
static void FUN_00481b90(unsigned char* state)
{
    if (state[9] == 0) {
        if (state[5] != 0xff) {
            unsigned char* hold = &state[0x13];
            if (((g_button_pressed_id >> 8) & 0x50) == 0) {
                *hold = 0;
            } else {
                *hold = *hold + 1;
            }
            unsigned char dir = 0;
            if (((g_PlayerPadHeld >> 8) & 0x10) != 0) dir = 1;
            if (((g_PlayerPadHeld >> 8) & 0x40) != 0) dir = 2;
            if ((0x14 < *hold) && (*hold % 3 == 0)) {
                // Auto-repeat every 3rd frame after 20 held frames
                if (((g_button_pressed_id >> 8) & 0x10) != 0) dir = 1;
                if (((g_button_pressed_id >> 8) & 0x40) != 0) dir = 2;
            }
            if (dir == 1) {
                // Up: nearest seen file above the cursor
                for (int i = 0; i < 8; i++) {
                    int cand = (int)state[5] - 1 - i;
                    if (cand < 0) break;
                    if (pickup_item_seen(g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + cand]) != 0) {
                        state[5] = (unsigned char)cand;
                        play_sfx(3, 4, 0);
                        break;
                    }
                }
            }
            if (dir == 2) {
                // Down: nearest seen file below the cursor
                for (int i = 0; i < 8; i++) {
                    int cand = (int)state[5] + 1 + i;
                    if (cand > 7) break;
                    if (pickup_item_seen(g_pickupKeyItemList[((int)state[4] + state[7] * 2) * 8 + cand]) != 0) {
                        state[5] = (unsigned char)cand;
                        play_sfx(3, 4, 0);
                        break;
                    }
                }
            }
        }
        // Next / previous book: RIGHT (raw byte1 0x20) and LEFT (raw byte1
        // 0x80). The raw pad word's byte 1 is the D-pad in PSX order -
        // 0x10 = up, 0x20 = right, 0x40 = down, 0x80 = left - which is exactly
        // what g_padRemapSubTable0[0..3] (0x1000/0x2000/0x4000/0x8000 -> dpad
        // bits 0-3, with up cancelling down and right cancelling left) maps.
        // These bits do NOT collide with confirm/cancel: those live in the
        // REMAPPED word (dpad byte1 0x40/0x80 <- raw byte 0 bits 7/6).
        if (((g_PlayerPadHeld >> 8) & 0x20) != 0) {
            if ((int)state[4] + 1 <= 1) {
                state[2] = 3;
                play_sfx(3, 4, 0);
            } else {
                state[4] = 1;
            }
        }
        if (((g_PlayerPadHeld >> 8) & 0x80) != 0) {
            if ((int)state[4] - 1 < 0) {
                state[4] = 0;
            } else {
                state[2] = 8;
                play_sfx(3, 4, 0);
            }
        }
    }
    FUN_00481d00(state);
}

// (0x00481ab0) - File book selector dispatcher: state[1] 0 = init, 1 = input
static void FUN_00481ab0(unsigned char* state)
{
    if (state[1] == 0) {
        FUN_00481ae0(state);
        state[1] = state[1] + 1;
        return;
    }
    if (state[1] != 1) {
        return;
    }
    FUN_00481b90(state);
}

// ============================================================================
// Item box storage screen (main_menu mode 2, state 3) - FUN_004941f0
//
// Two cursors: the player inventory row (DAT_00ae9f23, the shared menu
// cursor) and the 48-slot box grid (DAT_00ae9f24). Confirm swaps the item
// between the two; L1/R1 page the grid with a slide animation. Previously
// this was an empty stub returning 0, so state 3 never exited and the box
// softlocked after opening.
// ============================================================================

// 0x00420b80 - refresh the displayed item from the player cursor slot
static void itembox_refresh_item(void)
{
    unsigned char slot = (DAT_00ae9f23 >> 1) - 4;
    if (slot < g_TotalHeldItems) {
        g_bItemMenuSelectedItemId = *((unsigned char*)g_ItemSlotsPointer + (unsigned int)slot * 2);
        return;
    }
    g_bItemMenuSelectedItemId = 0;
}

// 0x00420a70 - draw the menu cursor frame at the current cursor position
static void itembox_draw_cursor(void)
{
    // Cursor positions come from the 0x004c2960 table: 4 menu-tab positions
    // then the 8 inventory slots as (x, y) short pairs. The original reads
    // word[0x4c2960 + cursor*2] for X and word[0x4c2960 + cursor*2 + 2] for Y
    // (assembly 0x00420a9e/0x00420aac); an earlier port read
    // g_MenuFrameDataBlock + cursor + 0x148, which pointed at the tab-button
    // frame parts and dropped the *2 scale, so the cursor drew at garbage
    // positions.
    g_TextureDesc.flags = 0x01000040;
    g_TextureDesc.texturePage = 0x1c;
    g_TextureDesc.clutX = 0;
    g_TextureDesc.clutY = 0x1e4;
    g_TextureDesc.screenX = *(short*)(g_inventorySlotsPos + (unsigned int)DAT_00ae9f23 * 2);
    g_TextureDesc.screenY = *(short*)(g_inventorySlotsPos + (unsigned int)DAT_00ae9f23 * 2 + 2);
    if ((DAT_00ae9f23 & 0xf8) != 0) {
        g_TextureDesc.width = 0x28;
        g_TextureDesc.texU = 0x80;
        g_TextureDesc.texV = 0xe0;
        g_TextureDesc.height = 0x1e;
        if ((DAT_00ae9f18 & 0x20) == 0) {
            g_TextureDesc.texU = 0xa8;
        }
        g_rect.x = 0xce;
        if ((DAT_00ae9f19 & 2) == 0) {
            g_TextureDesc.screenY = g_TextureDesc.screenY + 0x1e;
        }
        draw_texture(&g_TextureDesc, 5);
        g_TextureDesc.texV = 0x90;
        return;
    }
    g_TextureDesc.width = 0x30;
    g_TextureDesc.texU = 0;
    g_TextureDesc.texV = 0x50;
    g_TextureDesc.height = 0x10;
    if (g_MainMenuState == 3) {
        g_TextureDesc.screenX = *(short*)(g_inventorySlotsPos + (unsigned int)(DAT_00ae9f23 | 2) * 2);
    }
    draw_texture(&g_TextureDesc, 0x19);
    g_TextureDesc.texV = 0x98;
}

// 0x00443040 - draw an item icon into the box grid from the shared item image.
// The original places each row of 8 as a 2x4 mini-grid: x = (slot&1)*20,
// y = ((slot&~1)<<4) + 0x50 (verified against the disassembly).
static void itembox_draw_slot_icon(int imgType, int slot)
{
    LoadImage((int)g_TimImageBuffer__bitmap + imgType * 0x4b0, 0xc, slot + 0xf, 1,
              (short)((slot & 1) * 0x14), (short)(((slot & 0xfe) << 4) + 0x50),
              0x14, 0x1e, 1);
}

// 0x004941f0 - item box interaction; returns non-zero when the menu closes
static int menu_itembox_interaction(void)
{
    if ((pad_held_byte1() & 8) != 0) {
        play_sfx(3, 5, 0);
        return 1;
    }
    switch (DAT_00ae9f20) {
    case 0:
        DAT_00ae9f20 = 1;
        DAT_00ae9f24 = 0;
        DAT_00ae9f18 = 0;
        itembox_refresh_item();
        // fall through
    case 1:
        if ((dpad_pressed_byte1() & 0x80) != 0) {
            play_sfx(3, 5, 0);
            return 1;
        }
        if ((dpad_pressed_byte1() & 0x40) != 0) {
            play_sfx(3, 6, 0);
            if ((DAT_00ae9f23 & 0xf8) == 0) {
                return 1;
            }
            DAT_00ae9f20 = 2;
            DAT_00ae9f26 = 0x0f;
            DAT_00ae9f21 = 0;
            DAT_00ae9f18 = 0;
            DAT_00ae9f27 = 0;
            DAT_00ae9f28 = 0;
        } else {
            if ((pad_held_byte1() & 0xf0) == 0) {
                DAT_00ae9f18 = DAT_00ae9f18 - 1;
            } else {
                if (((pad_held_byte1() & 0xa0) != 0) && ((DAT_00ae9f23 & 0xf8) != 0)) {
                    DAT_00ae9f23 = DAT_00ae9f23 ^ 2;
                }
                if ((pad_held_byte1() & 0x10) == 0) {
                    if ((pad_held_byte1() & 0x40) != 0) {
                        DAT_00ae9f23 = DAT_00ae9f23 + 4;
                        if ((unsigned int)g_totalInventorySlots * 2 + 6 < (unsigned int)DAT_00ae9f23) {
                            DAT_00ae9f23 = (DAT_00ae9f23 & 2) | 4;
                        }
                    }
                } else {
                    DAT_00ae9f23 = DAT_00ae9f23 - 4;
                    if ((DAT_00ae9f23 & 0xfc) == 0) {
                        DAT_00ae9f23 = (DAT_00ae9f23 & 2) | (g_totalInventorySlots * 2 + 4);
                    }
                }
                itembox_refresh_item();
                DAT_00ae9f18 = 0;
                play_sfx(3, 4, 0);
            }
        }
        break;
    case 2:
        if ((dpad_pressed_byte1() & 0x80) != 0) {
            DAT_00ae9f20 = 1;
            play_sfx(3, 5, 0);
            break;
        }
        if ((dpad_pressed_byte1() & 0x40) != 0) {
            if (g_itemboxSlots[DAT_00ae9f24].Id != 0 || g_bItemMenuSelectedItemId != 0) {
                DAT_00ae9f20 = 1;
                play_sfx(3, 6, 0);
                unsigned char slot = (DAT_00ae9f23 >> 1) - 4;
                unsigned int playerIdx = (unsigned int)slot;
                if ((unsigned int)g_EquippedItemId - playerIdx == 1) {
                    g_EquippedItemId = 0;
                }
                unsigned int boxIdx = (unsigned int)DAT_00ae9f24;
                unsigned char boxItem = g_itemboxSlots[boxIdx].Id;
                unsigned char boxQty = g_itemboxSlots[boxIdx].qty;
                unsigned char* playerSlot = (unsigned char*)g_ItemSlotsPointer + playerIdx * 2;
                g_itemboxSlots[boxIdx].Id = playerSlot[0];
                g_itemboxSlots[boxIdx].qty = playerSlot[1];
                playerSlot[0] = boxItem;
                playerSlot[1] = boxQty;
                if (g_TotalHeldItems <= slot) {
                    unsigned char freeIdx = 0;
                    if ((g_ItemSlotsBitmask & 1) != 0) {
                        do {
                            freeIdx++;
                        } while ((g_ItemSlotsBitmask & (1u << (freeIdx & 0x1f))) != 0);
                    }
                    g_ItemSlotIndices[playerIdx] = freeIdx;
                    g_ItemSlotsBitmask |= 1u << (freeIdx & 0x1f);
                }
                if ((boxItem != 0) && (boxItem < 0x6f)) {
                    LoadItemImage((int)g_ItemImageLookupTable[(unsigned int)boxItem * 4] - 1,
                                  (int)g_ItemSlotIndices[playerIdx], (int)g_TimImageBuffer__bitmap);
                }
                rearrange_item_slots();
                itembox_refresh_item();
                unsigned char newBoxItem = g_itemboxSlots[DAT_00ae9f24].Id;
                if ((newBoxItem != 0) && (newBoxItem < 0x6f)) {
                    itembox_draw_slot_icon((int)g_ItemImageLookupTable[(unsigned int)newBoxItem * 4] - 1,
                                           DAT_00ae9f24 & 7);
                }
            }
            break;
        }
        if (((pad_held_byte1() & 0x50) == 0) && (DAT_00ae9f28 == 0)) break;
        if (((unsigned short)g_button_pressed_id & 0x5000) == 0) {
            DAT_00ae9f26 = 0x0f;
            DAT_00ae9f27 = 0;
            break;
        }
        DAT_00ae9f20 = 3;
        DAT_00ae9f28 = 1;
        play_sfx(2, 0x21, 0);
        if (((unsigned short)g_button_pressed_id & 0x1000) == 0) {
            // R1 (or next page): slide forward. The original does NOT advance
            // DAT_00ae9f24 here - only `next` is computed for the icon blit;
            // the cursor moves +1 when the slide finishes (state 3). The port
            // used to store `next` into DAT_00ae9f24 AND the slide end added
            // +1 again, so R1 paged two slots per press.
            SUBMENU_STATE_ID = 0;
            DAT_00ae9f1c = 1;
            unsigned char next = DAT_00ae9f24 + 1;
            if (next == 0x30) {
                next = 0;
            }
            unsigned char itemId = g_itemboxSlots[next].Id;
            if ((itemId != 0) && (itemId < 0x6f)) {
                itembox_draw_slot_icon((int)g_ItemImageLookupTable[(unsigned int)itemId * 4] - 1,
                                       next & 7);
            }
        } else {
            // L1: slide back
            SUBMENU_STATE_ID = 0x0f;
            DAT_00ae9f1c = -1;
            if (DAT_00ae9f24 == 0) {
                DAT_00ae9f24 = 0x2f;
            } else {
                DAT_00ae9f24 = DAT_00ae9f24 - 1;
            }
            unsigned char itemId = g_itemboxSlots[DAT_00ae9f24].Id;
            if ((itemId != 0) && (itemId < 0x6f)) {
                itembox_draw_slot_icon((int)g_ItemImageLookupTable[(unsigned int)itemId * 4] - 1,
                                       DAT_00ae9f24 & 7);
            }
        }
        if (((unsigned short)(DAT_00ae9f27 & (g_button_pressed_id >> 8)) != 0) &&
            (DAT_00ae9f26 == 0)) {
            if (DAT_00ae9f1c < 1) {
                DAT_00ae9f1c = -3;
            } else {
                DAT_00ae9f1c = 3;
            }
        }
        // fall through
    case 3:
        if ((unsigned short)(DAT_00ae9f27 & (g_button_pressed_id >> 8)) == 0) {
            DAT_00ae9f26 = 0x0f;
            if (((unsigned short)g_button_pressed_id & 0x5000) == 0) {
                DAT_00ae9f27 = 0;
            } else if (((unsigned short)g_button_pressed_id & 0x1000) == 0) {
                DAT_00ae9f27 = 0x40;
            } else {
                DAT_00ae9f27 = 0x10;
            }
        } else if (DAT_00ae9f26 != 0) {
            DAT_00ae9f26 = DAT_00ae9f26 - 1;
        }
        SUBMENU_STATE_ID = SUBMENU_STATE_ID + DAT_00ae9f1c;
        if (SUBMENU_STATE_ID == 0x0f) {
            SUBMENU_STATE_ID = 0;
        }
        if (SUBMENU_STATE_ID == 0) {
            DAT_00ae9f20 = 2;
            if (0 < DAT_00ae9f1c) {
                if (DAT_00ae9f24 == 0x2f) {
                    DAT_00ae9f24 = 0;
                } else {
                    DAT_00ae9f24 = DAT_00ae9f24 + 1;
                }
            }
            DAT_00ae9f1c = 0;
        }
        break;
    }
    itembox_draw_cursor();
    FUN_00454fd0(g_bItemMenuSelectedItemId, 0, item_name_left() - g_ScreenOffsetX, 0xba - g_ScreenOffsetY);
    return 0;
}

// ============================================================================
// Item box screen assets and per-frame overlay (loadMenuAssets + draw_itembox_menu)
//
// loadMenuAssets (0x00494730) loads itemboxn.tim (the box frame art: panel
// edges, list border, page arrows) into texture page slot 0xC, then item_all.pix
// (the item sprite sheet) into the TIM buffer, and pre-blits the first box
// slot's icon. draw_itembox_menu (0x004947c0) draws the box overlay every
// frame: frame textures (table A), mask rects (table B), the cursor slot
// (icon + quantity), the item name list, the "box is full" line and the
// page-position triangles, then the final frame pieces (table C).
//
// The frame tables below preserve the original's memory layout: entries are
// stored in reverse draw order and load_main_menu_frame_part_tex_area walks
// backwards from the end of each table.
// ============================================================================

// 0x004d4430 - frame textures, 5 entries of {screenX, screenY, width, height,
// texU, texV} + 2 pad bytes (drawn 5th..1st in memory order):
//   preview border (42x32 at 0x10,0x30), down arrow (8x7), up arrow (8x7),
//   and the green item-list frame's top/bottom 1px lines at (41,33)/(41,80)
//   - DECIMAL (the port once read them as hex 0x41/0x33 and "corrected" them
//   to (42,35), which parked the top line inside the top shadow rect
//   y=34..48: the 50% shadow blended the bright green (48,216,16) into dark
//   green, and the 2px strip y=33..34 the line used to cover let sliding
//   names peek above the border). The lines sample the itemboxn panel's
//   green edge rows (row 0 / row 47). The frame's side borders and the blue
//   list background come from the table C panel piece below, not from here.
static const unsigned char s_itemboxFramePartsA[60] = {
    0x29,0x00,0x21,0x00,0x80,0x00,0x01,0x00,0x00,0x00,0x00,0x00, // drawn 5th: top line (41, 33) panel row 0
    0x29,0x00,0x50,0x00,0x80,0x00,0x01,0x00,0x00,0x00,0x2f,0x00, // drawn 4th: bottom line (41, 80) panel row 47
    0xaf,0x00,0x18,0x00,0x08,0x00,0x07,0x00,0x48,0x00,0x38,0x00, // drawn 3rd: up arrow
    0xaf,0x00,0x53,0x00,0x08,0x00,0x07,0x00,0x48,0x00,0x40,0x00, // drawn 2nd: down arrow
    0x5b,0x00,0x57,0x00,0x2a,0x00,0x20,0x00,0x10,0x00,0x30,0x00, // drawn 1st: preview border
};

// 0x004d4498 - mask rects, 5 entries of {x, y, w, h} (drawn 5th..1st in memory
// order): box list bottom bar, right column, left column, name area, top bar
static const unsigned char s_itemboxMaskRects[40] = {
    0x2a,0x00,0x18,0x00,0x7e,0x00,0x09,0x00, // drawn 5th
    0x2a,0x00,0x51,0x00,0x31,0x00,0x0f,0x00, // drawn 4th
    0x5b,0x00,0x51,0x00,0x2a,0x00,0x06,0x00, // drawn 3rd
    0x85,0x00,0x51,0x00,0x23,0x00,0x0f,0x00, // drawn 2nd
    0x5c,0x00,0x77,0x00,0x28,0x00,0x09,0x00, // drawn 1st
};

// 0x004d4470 - trailing frame textures, 3 entries (drawn 3rd..1st in memory
// order): the item-list box interior piece, the bottom half-arrow, the top
// arrow. Entry 1 is the heart of the list box look: the itemboxn panel's
// rows 1..46 (128x46) drawn at (41,34) - its blue interior texels fill the
// list background and its green edge columns (panel col 0 / col 127) are the
// frame's side borders, closing the rectangle against the table A lines at
// y=33/80. The two 50% shadow rects dim its first/third name rows, which is
// what makes the selected (middle) row read lighter. The port once stored
// this entry 4 bytes early (straddling the 0x004d446c padding), producing
// (0,0,65,34,u=0x80,v=0x2e) - a draw that samples past the texture page and
// renders nothing, leaving the list box black with fabricated 1px sides.
static const unsigned char s_itemboxFramePartsC[36] = {
    0x29,0x00,0x22,0x00,0x80,0x00,0x2e,0x00,0x00,0x00,0x01,0x00, // drawn 3rd: list box interior + side borders (41, 34) 128x46
    0xaf,0x00,0x20,0x00,0x08,0x00,0x20,0x00,0x00,0x00,0x30,0x00, // drawn 2nd: bottom half-arrow
    0xaf,0x00,0x40,0x00,0x08,0x00,0x12,0x00,0x08,0x00,0x30,0x00, // drawn 1st: top arrow
};

// (0x00494730) - Load item box menu textures
static void loadMenuAssets(void)
{
    LoadFile(GAME_DATA_ROOT "data\\itemboxn.tim", g_TimImageBuffer__bitmap, 0x20);
    g_TextureCurrentPage = 0x1c;
    g_TextureBankID = 0x15;
    LoadTexturePage(g_TimImageBuffer__bitmap, 0x15, 0x1c, 0xc, 0, 0, 0, 0);

    LoadFile(GAME_DATA_ROOT "data\\item_all.pix", g_TimImageBuffer__bitmap, 0x20);
    if ((g_itemboxSlots[0].Id != 0) && (g_itemboxSlots[0].Id < 0x6f)) {
        itembox_draw_slot_icon((int)g_ItemImageLookupTable[(unsigned int)g_itemboxSlots[0].Id * 4] - 1, 0);
    }
    DAT_00ae9f24 = 0;
    SUBMENU_STATE_ID = 0;
    DAT_00ae9f1c = 0;
}

// (0x004947c0) - Draw item box menu overlay
static void draw_itembox_menu(void)
{
    // 0x00494855-0x004948d3: Draw the mask rects (cut the box list slots out
    // of the dark background)
    g_rect.textureId = 0;
    g_rect.r = 0;
    g_rect.g = 0;
    g_rect.b = 0;
    const unsigned char* pRect = s_itemboxMaskRects + sizeof(s_itemboxMaskRects);
    do {
        pRect -= 8;
        g_rect.h = *(short*)(pRect + 6);
        g_rect.w = *(short*)(pRect + 4);
        g_rect.y = *(short*)(pRect + 2);
        g_rect.x = *(short*)(pRect + 0);
        draw_rect(&g_rect, 0, 0);
    } while (pRect > s_itemboxMaskRects);

    // 0x004948d5-0x004949ad: Shadow rects above the item list and cursor
    // highlight rects while the box is in cursor mode (DAT_00ae9f20 < 2).
    // These rects are drawn with blend=51 (pending depth 501) instead of the
    // original blend=0 (450): pending rects and command sprites interleave in
    // one far-to-near walk, and a depth-500 rect is drawn AFTER the
    // FlushSpriteCommandsRange(500, ...) that emits the green frame lines
    // (command depthSort = depth*16+500 = 500) - the 50% shadow landed on top
    // of the frame and blended the bright green into dark green. At 501 the
    // walk emits the shadow first and the frame lines after it, which is the
    // original layering: the shadow dims the list content (top name row,
    // table C panel piece) while the frame stays bright. The masks (450)
    // still draw over everything in the final pass, clipping the sliding
    // names to the list box.
    g_rect.r = 112;
    g_rect.g = 112;
    g_rect.y = 34;
    g_rect.b = 112;
    g_rect.textureId = 0x60000000;
    g_rect.x = 42;
    g_rect.w = 126;
    g_rect.h = 15;
    draw_rect(&g_rect, 51, 0);
    g_rect.y = 0x40;
    g_rect.h = 0x10;
    draw_rect(&g_rect, 51, 0);
    if (DAT_00ae9f20 < 2) {
        g_rect.y = 34;
        g_rect.h = 0x2e;
        draw_rect(&g_rect, 51, 0);
        g_rect.x = 0x5c;
        g_rect.y = 0x58;
        g_rect.w = 0x28;
        g_rect.h = 0x1e;
        draw_rect(&g_rect, 51, 0);
    }

    // 0x004949ad-0x00494a01: Small rects (scrollbar track on the right)
    g_rect.y = 0x10;
    g_rect.h = 0x10;
    g_rect.x = 0xd2;
    g_rect.w = 0x5e;
    draw_rect(&g_rect, 0, 0);
    g_rect.y = 0x20;
    g_rect.w = 0x2f;
    draw_rect(&g_rect, 0, 0);

    // Draw the box frame textures (preview border, arrows, item-list frame).
    // Submission order does not decide the result here - the interleaved
    // render walk orders by depth (shadows 501, frame commands 500, masks
    // 450) - so the pieces are submitted in the original's order.
    g_TextureDesc.flags = 0x01000040;
    g_TextureDesc.clutY = 0x1fc;
    g_TextureDesc.texturePage = 0x15;
    g_CurrentMenuFramesDataPtr = (unsigned short*)(s_itemboxFramePartsA + sizeof(s_itemboxFramePartsA));
    g_TextureDesc.clutX = 0;
    for (int i = 0; i < 5; i++) {
        load_main_menu_frame_part_tex_area();
        draw_texture(&g_TextureDesc, 0);
    }

    // 0x00494a04-0x00494b7e: Draw the cursor slot (icon + quantity).
    // One slot normally, two during a page slide (SUBMENU_STATE_ID != 0); the
    // slide shifts the sampled texV by 2px/frame while the icon's height grows
    // from 0, so the incoming slot reveals from its bottom edge.
    unsigned char slot = DAT_00ae9f24;
    unsigned char loopCount = 2 - (SUBMENU_STATE_ID == 0);
    unsigned char slide2 = (unsigned char)((unsigned short)SUBMENU_STATE_ID * 2);
    short qtyY = (0x2c - (unsigned short)SUBMENU_STATE_ID) * 2;
    g_TextureDesc.screenY = 0x58;
    g_TextureDesc.height = 0x1e - slide2;
    do {
        g_TextureDesc.screenX = 0x5c;
        g_TextureDesc.width = 0x28;
        unsigned char itemId = g_itemboxSlots[slot].Id;
        if (itemId == 0) {
            // Empty slot background (blue.tim at page slot 0xA)
            g_TextureDesc.clutY = 0x1e0;
            g_TextureDesc.texU = 0;
            g_TextureDesc.texV = 0;
            g_TextureDesc.texturePage = 0x1c;
            display_texture(&g_TextureDesc, 0x17, 0xa, 1);
        } else {
            // Filled slot: the item icon was composited into the shared
            // item-image VRAM page by itembox_draw_slot_icon at exactly
            // ((slot&1)*40, (slot&6)*16+0x50), and the port's LoadImage page
            // descriptors sit in 0x1d space with clut base 0x1e0 - the
            // original's 0x15/0x1fc (the itemboxn page's space) never matches
            // a page here, so the desc is adapted the same way the inventory
            // item draw is.
            g_TextureDesc.texturePage = 0x1d;
            g_TextureDesc.texU = (slot & 1) * 0x28;
            g_TextureDesc.clutY = 0x1e4;
            g_TextureDesc.texV = (slot & 6) * 0x10 + slide2 + 0x50;
            if (itemId < 0x6f) {
                display_texture(&g_TextureDesc, 0x17, 0xf, 8);
            } else {
                // Special items (>= 0x6f) come from staitem.tim (page slot
                // 0x1E), one 30px row per item id
                g_TextureDesc.clutY = 0x1e0;
                g_TextureDesc.texturePage = 0;
                g_TextureDesc.texU = 0;
                g_TextureDesc.texV = (unsigned char)(itemId * 0x1e) + slide2 - 2;
                display_texture(&g_TextureDesc, 0x17, 0x1e, 1);
            }
            g_TextureDesc.screenX = g_TextureDesc.screenX - 2;
            g_TextureDesc.texturePage = 0x1c;
            g_TextureDesc.screenY = (unsigned short)qtyY;
            display_item_qty(itemId, g_itemboxSlots[slot].qty, 0x16);
        }
        g_TextureDesc.height = 0x1e;
        g_TextureDesc.screenY = (unsigned short)(qtyY + 0x1e);
        slide2 = 0;   // the second slide slot samples its own row, no offset
        if (slot == 0x2f) {
            slot = 0;
        } else {
            slot++;
        }
        qtyY = qtyY + 0x1e;
    } while (--loopCount != 0);

    // 0x00494b84-0x00494c19: Item name list (3 names normally, 4 during a
    // slide), ending at the cursor slot
    unsigned char nameSlot = (DAT_00ae9f24 != 0) ? (DAT_00ae9f24 - 1) : 0x2f;
    short nameY = 0x23 - (unsigned short)SUBMENU_STATE_ID;
    sprintf(PRINT_TEXT_BUFFER, "_Nothing_");
    unsigned char nameCount = 4 - (SUBMENU_STATE_ID == 0);
    do {
        if (g_itemboxSlots[nameSlot].Id == 0) {
            PrintText8x14(0x2a, nameY, 0x83, 0);
        } else {
            FUN_00454fd0(g_itemboxSlots[nameSlot].Id, 0x80, 0x2a, nameY);
        }
        nameY = nameY + 0xf;
        if (nameSlot == 0x2f) {
            nameSlot = 0;
        } else {
            nameSlot++;
        }
    } while (--nameCount != 0);

    // 0x00494c19-0x00494c99: "Box is full" warning line under the last row
    // (cursor on slots 0x2E/0x2F): a red horizontal line following the grid
    if (0x2c < (unsigned char)(DAT_00ae9f24 - 1)) {
        unsigned char line[16] = { 0 };
        line[0xc] = 0xff;
        line[0xd] = 0xef;
        line[0xe] = 0;
        *(unsigned short*)(line + 4) = 0x2a;
        *(unsigned short*)(line + 8) = 0xa8;
        short lineY = 0x31 - (unsigned short)SUBMENU_STATE_ID;
        if (DAT_00ae9f24 != 0) {
            lineY = lineY + (0x30 - (unsigned short)DAT_00ae9f24) * 0xf;
        }
        *(unsigned short*)(line + 6) = (unsigned short)lineY;
        *(unsigned short*)(line + 10) = (unsigned short)lineY;
        FUN_00470c60(line, 0x19);
    }

    // 0x00494c9c-0x00494d37: Page-position triangles (3px strip following the
    // cursor slot in the 48-slot list)
    unsigned char triSlot = 0x2f;
    if (DAT_00ae9f24 != 0) {
        triSlot = DAT_00ae9f24 - 1;
    }
    if (DAT_00ae9f1c < 0) {
        triSlot++;
    }
    g_TextureDesc.flags = 0x01000040;
    g_TextureDesc.texU = 0x40;
    g_TextureDesc.texV = 0x38;
    g_TextureDesc.screenX = 0xb0;
    g_TextureDesc.width = 6;
    g_TextureDesc.height = 1;
    g_TextureDesc.clutX = 0;
    g_TextureDesc.clutY = 0x1fc;
    g_TextureDesc.texturePage = 0x15;
    unsigned char triCount = 3;
    do {
        if (0x2f < triSlot) {
            triSlot = 0;
        }
        g_TextureDesc.screenY = triSlot + 0x21;
        triSlot++;
        draw_texture(&g_TextureDesc, 0);
    } while (--triCount != 0);

    // 0x00494d39-0x00494d86: Trailing frame pieces (arrows + dead corner)
    g_CurrentMenuFramesDataPtr = (unsigned short*)(s_itemboxFramePartsC + sizeof(s_itemboxFramePartsC));
    load_main_menu_frame_part_tex_area();
    draw_texture(&g_TextureDesc, 0x1e);
    load_main_menu_frame_part_tex_area();
    draw_texture(&g_TextureDesc, 0x1e);
    load_main_menu_frame_part_tex_area();
    draw_texture(&g_TextureDesc, 0x28);
}

// (0x00464560) - Load frame part texture area from g_CurrentMenuFramesDataPtr table pointer.
// Each call reads a 12-byte entry backwards: screenX(short), screenY(short),
// width(ushort), height(ushort), texU(byte), texV(byte), then advances the pointer.
static void load_main_menu_frame_part_tex_area(void)
{
    int base = (int)g_CurrentMenuFramesDataPtr;
    g_TextureDesc.texV    = *(unsigned char*)(base - 2);
    g_TextureDesc.texU    = *(unsigned char*)(base - 4);
    g_TextureDesc.height  = *(unsigned short*)(base - 6);
    g_TextureDesc.width   = *(unsigned short*)(base - 8);
    g_TextureDesc.screenY = *(short*)(base - 10);
    g_TextureDesc.screenX = *(short*)(base - 12);
    g_CurrentMenuFramesDataPtr = (unsigned short*)(base - 12);
}

// Forward declarations for the item 3D model viewer (0x0044e1b0 family)
static void FUN_0044ea50(void);
static int  FUN_0040a990(int y, int x);
static int  FUN_0040a530(int value);
static void FUN_0040a250(MATRIX* src, MATRIX* dst);
static int  FUN_0044ed40(void);
void FUN_004846f0(int slot, int depth);   // 0x004846f0 - offset quad OT record
void FUN_00484740(int slot, int depth);   // 0x00484740 - full-screen quad OT record
static int  FUN_0044ef60(short angle, int axisMask);
static void FUN_0044eca0(void);
static void FUN_0044eb30(void);
static void FUN_0044e660(void);
static void FUN_0044e820(void);
static void FUN_0044e8c0(void);
static void FUN_0044e920(void);
// Non-static: the ending epilogue (EndingScreen.cpp) reuses the item viewer.
void FUN_004846d0(int slot);
static void FUN_004844c0(void);
extern void FUN_00483580(int* joint, MATRIX* out);   // shared with render_room_objects
static void FUN_004841f0(void);
extern void ResolveAnimPointers(unsigned char* data);
extern void InitScaMatrix(int parentPtr, ScaMatrixData* matrix);
extern unsigned int CheckTmdTransparency(int tmdData);
// room_event_take_item (0x004631c0) is declared in Globals.h
extern void LoadPSXImage(PSXTexture* tex, void* buf, int mode);

// Item 3D model viewer (0x0044e1b0) and item model loader (0x004841f0)
// ============================================================================

// Item model load state (FUN_00484420 / FUN_004841f0)
static int  g_itemModelSrc;            // DAT_00aafcdc - .ivm file buffer (TIM part)
static int  g_itemModelTmdBase;        // DAT_00aafcd8 - TMD header inside the .ivm
static int  g_itemModelBlendFlag;      // DAT_004d2c10 - 0x3f000000 when the model has transparency
static int  g_itemModelTmdCount;       // DAT_004d2c14 - processed object count
static int  g_itemRenderSlot;          // DAT_008f8d80 - render slot index (0-2)
// DAT_008fc3f8 / DAT_008f8c48 / DAT_008f8c50 / DAT_009220b8 are NOT standalone
// globals: each is a field of the object it sits at the end of.
//   0x008fc3f8 = itemPage        + 0x348  (Direct3DTIM_Create's "created" flag)
//   0x008f8c48 = g_itemStpPage   + 0x340  (PSXTexture::m_NumCLUTs)
//   0x008f8c50 = g_itemStpPage   + 0x348  ("created" flag)
//   0x009220b8 = shared TMD slot + 0x1590 (CMarniDirect3DTMD::m_initialized)
// Declaring them as separate ints left every one of them permanently 0, which
// is what deleted the examine screen's semi-transparency pass (see
// FUN_004841f0 / FUN_004844c0 below).
#define ITEM_PAGE_CREATED(page)   (*(DWORD*)((BYTE*)(page) + 0x348))
#define ITEM_PAGE_NUMCLUTS(page)  (*(DWORD*)((BYTE*)(page) + 0x340))
// The original stores EACH TMD object of a multi-object item into its own
// CMarniDirect3DTMD slot at 0x008f8d88 + objIndex*0x1594, and the viewer draws
// the slot matching the render slot. That address is a region of its own, well
// clear of the main TMD buffer at 0x00923b50 that ObjectCleanupCallback sweeps
// - so the port gives it one too, rather than carving slots 247..249 out of
// g_tmdObjectBuffer and relying on the sweep never coinciding with a viewer
// draw. See the slot-region note in TmdRenderer.h.
alignas(16) unsigned char g_itemTmdSlotBuffer[TMD_ITEM_SLOT_COUNT * TMD_SLOT_STRIDE];

static BYTE* g_itemTmdSlots[TMD_ITEM_SLOT_COUNT] = {
    &g_itemTmdSlotBuffer[0 * TMD_SLOT_STRIDE],  // DAT_008fc0b0 (item object 0)
    &g_itemTmdSlotBuffer[1 * TMD_SLOT_STRIDE],  // item object 1
    &g_itemTmdSlotBuffer[2 * TMD_SLOT_STRIDE],  // item object 2
};
// DAT_00920b28 - the shared TMD slot the semi-transparency pass draws from.
// A region of its own in the original; CMarniDirect3DTMD::Transform is called
// on it (see the examine render), so TmdQueueObject has to be able to resolve
// pointers into it or the object is silently dropped from the frame's queue.
alignas(16) unsigned char g_itemSharedTmdSlot[TMD_SLOT_STRIDE];

// DAT_008f8908 - the STP ("semi-transparent") variant of the item texture page.
// A second PSXTexture holding its own copy of the same TIM, with every texel
// whose CLUT colour carries the PS1 STP bit (0x8000) knocked out to palette
// index 0 (= fully transparent). The examine screen draws the model twice at
// the same OT depth: once from the item page at 50% alpha, once from this page
// opaque, so the glass texels end up half-transparent and everything else keeps
// its full brightness. Sized like a texture-page record: PSXTexture (0x348) +
// the created flag (0x348) + eight handles (0x34C..0x36C).
alignas(16) static unsigned char g_itemStpPage[0x36C];

// Viewer animation state
static short DAT_00ae9f4b;             // 0x00ae9f4b - item model object count

// 0x00ae9f4c - examine spin input vector (x,y,z); the 4th short is the
// SVECTOR pad RotMatrix reads past the end of.
static short g_viewerSpinInput[4];
#define DAT_00ae9f4c (g_viewerSpinInput[0])
#define DAT_00ae9f4e (g_viewerSpinInput[1])
#define DAT_00ae9f50 (g_viewerSpinInput[2])

// 0x00ae9f54 - per-object rotation vectors, 8-byte stride (0x00ae9f54 for
// object 0, 0x00ae9f5c for object 1). The viewer indexes this block by BYTE
// offset (index * 8), so the two vectors have to live in one contiguous
// array. Declaring the components as six separate shorts made
// `&DAT_00ae9f54 + 8` resolve to the base rotation vector below, so object 1
// was rotated by the base rotation as well as the root Sca - the base
// rotation got applied twice and the turntable spin came out as a compound
// tumble about a diagonal axis.
static short g_viewerObjRot[2][4];
#define DAT_00ae9f54 (g_viewerObjRot[0][0])
#define DAT_00ae9f56 (g_viewerObjRot[0][1])
#define DAT_00ae9f58 (g_viewerObjRot[0][2])
// 0x00ae9f5e - the examine spin accumulator IS object 1's yaw (0xae9f5c + 2)
#define DAT_00ae9f5e (g_viewerObjRot[1][1])

// 0x00ae9f64 - the viewer's base rotation: x = pitch, y = yaw (the turntable
// axis the intro spin and the left/right d-pad drive), z = roll.
static short g_viewerRot[4];
#define DAT_00ae9f64 (g_viewerRot[0])
#define DAT_00ae9f66 (g_viewerRot[1])
#define DAT_00ae9f68 (g_viewerRot[2])

static int   g_viewerJoints[3 * 0x50 / 4]; // 0x00ae9f6c - joint structs (parent/child at +0x48/+0x4c stay NULL)
static BYTE  g_viewerMatrices[3 * 0x50]; // 0x00ae9f94 - sub-object matrices (0x50 each)
static int   g_viewerScaMatrices[3 * 0x50 / 4]; // Sca[0] 0x9f94, Sca[1] 0x9fe4, Sca[2] 0xaea034

// 0x00aea04c/50/54 are Sca[2].localMatrix.t - the root Sca's translation, the
// "zoom" the intro animates from -0xaf00 up to the resting 0x1980. They must
// alias the Sca so the translation flows through the chain compose and the
// menu camera matrix (which turns the +X offset into view depth) exactly as
// in the original; standalone ints left the root translation at zero.
#define VIEWER_ROOT_SCA ((ScaMatrixData*)((BYTE*)g_viewerScaMatrices + 2 * 0x50))
#define DAT_00aea04c (VIEWER_ROOT_SCA->localMatrix.t[0])
#define DAT_00aea050 (VIEWER_ROOT_SCA->localMatrix.t[1])
#define DAT_00aea054 (VIEWER_ROOT_SCA->localMatrix.t[2])

static int   g_bItemViewerZoomTimer;             // 0x00aea084 - animation counter
static int   g_viewerPadWord;          // DAT_00be05b0 - pad read
static MATRIX g_viewerMatrixBe0f60;    // 0x00be0f60
static MATRIX g_viewerMatrixBe0f80;    // 0x00be0f80
static VECTOR g_viewerSvecBe0fa0;      // 0x00be0fa0 - ApplyMatrix writes 3 ints, not an SVECTOR
static SVECTOR g_viewerSvecBe0fb0;     // 0x00be0fb0
static int   g_viewerLightData[3][8];  // 0x00d22700 - viewer light data (3 lights)

// The base rotation matrix lives at 0x00aea038 (Sca[2].localMatrix); its
// third column (m[0][2], m[1][2], m[2][2]) is the model's up vector read by
// the angle recompute (DAT_00aea03c / 0x00aea042 / 0x00aea048).
#define VIEWER_BASE_MATRIX  ((short*)((BYTE*)g_viewerScaMatrices + 2 * 0x50 + 4))

// Viewer light setup (FUN_0040ac80) - the original built light records in
// DAT_00d22700..00d22730 and passed each 8-dword block to FUN_0040ac80.
// The directions are set ONCE by main_menu (0x004638e3: light0 (100,0x50,0x50),
// light1 (-100,0x50,0x50), light2 (0,-100,0x50)) and the intro/exit animation
// only rewrites the three colour bytes (+0xC/+0xD/+0xE) of each record with the
// fade value. A previous revision stored the fade value into ALL eight dwords,
// which corrupted the light directions and left every face in shadow.
static void viewer_setup_lights(int fadeValue)
{
    // NOTE: ambient comes from setBackColor via SetLightMatrix (called every
    // frame by the renderer), so nothing is written here - matching the
    // original, which only updates the light record colours.
    for (int i = 0; i < 3; i++) {
        int* p = &g_viewerLightData[i][0];
        if (p[0] == 0 && p[1] == 0 && p[2] == 0) {
            // Seed the directions once (main_menu's DAT_00d22700 setup)
            switch (i) {
            case 0: p[0] = 100;  p[1] = 0x50; p[2] = 0x50; break;
            case 1: p[0] = -100; p[1] = 0x50; p[2] = 0x50; break;
            case 2: p[0] = 0;    p[1] = -100; p[2] = 0x50; break;
            }
        }
        // The original writes the same byte to +0xC, +0xD and +0xE
        unsigned char* c = (unsigned char*)&g_viewerLightData[i][3];
        c[0] = (unsigned char)fadeValue;
        c[1] = (unsigned char)fadeValue;
        c[2] = (unsigned char)fadeValue;
        FUN_0040ac80(i, p);
    }
}

// (0x0044ef60) - Examine rotation combo check
// Advances the 12-byte combo record pointer and tests the current rotation
// angle against the record's target window. Returns 1 (match) or 0.
static int FUN_0044ef60(short angle, int axisMask)
{
    unsigned short* pRec = g_CurrentMenuFramesDataPtr;
    unsigned short target = pRec[0];
    if (target == 0) {
        g_CurrentMenuFramesDataPtr = (unsigned short*)((int)g_CurrentMenuFramesDataPtr + 4);
        return axisMask;
    }
    unsigned short tol = pRec[1];
    g_CurrentMenuFramesDataPtr = (unsigned short*)((int)g_CurrentMenuFramesDataPtr + 4);
    int diff = (int)((short)(angle + (short)target) & 0xfff) - (int)tol & 0xfff;
    if (diff <= target * 2) {
        return axisMask;
    }
    return 0;
}

// (0x0044ed40) - Item examine check. Returns 1 when the examine message has
// been queued (or the examine sequence is running), 0 when the item cannot be
// examined yet.
static int FUN_0044ed40(void)
{
    unsigned char flagIndex = g_ItemImageLookupTable[(unsigned int)g_bItemMenuSelectedItemId * 4 + 2];
    if ((flagIndex & 0x80) == 0) {
        if (ITEM_NON_INFINITE_MAX < g_bItemMenuSelectedItemId) {
            return 0;
        }
        unsigned char exType = g_ItemExamineTypes[flagIndex];
        if ((exType & 0xf0) == 0) {
            g_bItemViewerActionIndex = 2;
            Flg_on((int)g_itemExaminedFlags, (unsigned int)flagIndex);
            set_message_display(g_ItemHealTable[(unsigned int)flagIndex + 0x51], 0);
            return 1;
        }
        unsigned char tries = exType >> 4;
        int bVar3;
        g_CurrentMenuFramesDataPtr = (unsigned short*)(g_ItemExamineCombos + (unsigned int)(exType & 0xf) * 0xc);
        do {
            bVar3 = FUN_0044ef60(DAT_00ae9f64, 1);
            bVar3 |= FUN_0044ef60(DAT_00ae9f66, 2);
            bVar3 |= FUN_0044ef60(DAT_00ae9f68, 4);
            tries = tries - 1;
            if (tries == 0) break;
        } while (bVar3 != 7);
        if (bVar3 == 7) {
            if (g_bItemMenuSelectedItemId == ITEM_RED_BOOK) {
                if (DAT_00ae9f5e == 0) {
                    g_bItemViewerActionIndex = 1;
                    g_bItemViewerZoomTimer = 0;
                    return 1;
                }
            } else if ((ITEM_RED_BOOK < g_bItemMenuSelectedItemId) && (g_bItemMenuSelectedItemId < ITEM_FIRST_AID_SPRAY)) {
                g_bItemViewerActionIndex = 1;
                g_bItemViewerZoomTimer = 0;
                return 1;
            }
            g_bItemViewerActionIndex = 2;
            Flg_on((int)g_itemExaminedFlags, (unsigned int)flagIndex);
            set_message_display(g_ItemHealTable[(unsigned int)flagIndex + 0x51], 0);
            return 1;
        }
    } else {
        switch (g_bItemMenuSelectedItemId) {
        case ITEM_COLT_PYTHON_DUM:
            // In the USA build item 4 is the unobtainable DumDum-rounds Colt
            // Python, one of the ammo-holding weapons the +9 below maps onto its
            // own ammo item (4 + 9 = 0x0D, DUMDUM ROUNDS). The DC gives that id
            // to the Beretta M92FS custom - its ADVANCED starting handgun, with
            // rounds in the slot - and renamed 0x0D to LOCKPICK, so the same
            // branch printed "LOCKPICK loaded." for the Beretta instead of its
            // description ("A beretta M92FS Automatic. / Custom edition."),
            // which is what the item shows on the PS1 DC. The DC falls through
            // to the description path below like any other unexaminable item.
            // (docs/DC_PORT.md 3i)
            if (g_bDcMode) {
                break;
            }
            // fall through - the USA build's DumDum Python
        case ITEM_COLT_PYTHON_MAG:
        case ITEM_BAZOOKA_EXPLOSIVE:
        case ITEM_BAZOOKA_ACID:
        case ITEM_BAZOOKA_FLAME:
            if (*(char*)((unsigned int)g_ItemSlotsPointer + 1 + (unsigned int)((DAT_00ae9f23 >> 1) - 4) * 2) != 0) {
                g_selectedItemId = weapon_ammo_item_id(g_bItemMenuSelectedItemId);
                g_bItemViewerActionIndex = 2;
                set_message_display(0xf0, 0);
                return 1;
            }
            break;
        case 0x13:
            if ((Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_ITEM13_USE_LOCK) != 0) ||
                (Flg_ck((int)g_ScenarioFlags, SCENARIO_FLAG_STAGE_VARIANT) != 0)) {
                g_bItemViewerActionIndex = 2;
                set_message_display(0xf1, 0);
                return 1;
            }
        }
    }
    return 0;
}

// Rotation control icon positions (0x004bf320 / 0x004bf328)
static const short g_itemViewerIconPos[4] = { 0x6D, 0xB6, 0x6D, 0x22 };
static const short g_itemViewerIconPosY[4] = { 0x1A, 0x49, 0x76, 0x49 };

// (0x0044eca0) - Draw the 4 rotation-control indicator arrows
static void FUN_0044eca0(void)
{
    g_TextureDesc.width = 9;
    g_TextureDesc.flags = 0x01000040;
    g_TextureDesc.texV = 0x7c;
    g_TextureDesc.texturePage = 0x1c;
    g_TextureDesc.clutX = 0;
    g_TextureDesc.clutY = 0x1e4;
    g_TextureDesc.height = 9;
    unsigned short bitMask = 0x8000;
    unsigned char count = 4;
    do {
        count = count - 1;
        g_TextureDesc.texU = 0x80;
        g_TextureDesc.screenX = g_itemViewerIconPos[count];
        g_TextureDesc.screenY = g_itemViewerIconPosY[count];
        // 0x0044ecfb tests the raw held word at 0x00bf0a0c, i.e. the same four
        // face-button bits the rotation reads - not the edge-detected word.
        if (((unsigned short)g_button_pressed_id & bitMask) == 0) {
            g_TextureDesc.texU = 0x8c;
        }
        bitMask = bitMask >> 1;
        display_texture(&g_TextureDesc, 4, 0, 1);
        g_TextureDesc.texV = g_TextureDesc.texV - 0xc;
    } while (count != 0);
}

// (0x0044eb30) - Recompute the model view rotation angles from the spin input
static void FUN_0044eb30(void)
{
    // Zoom target
    g_viewerMatrixBe0f60.t[0] = DAT_00aea04c;
    g_viewerMatrixBe0f60.t[1] = DAT_00aea050;
    g_viewerMatrixBe0f60.t[2] = DAT_00aea054;

    RotMatrix((SVECTOR*)&DAT_00ae9f4c, (MATRIX*)&g_viewerMatrixBe0f60);
    // base = spin * base (the original wrote the product back into 0xaea038)
    MulMatrixInPlace(&g_viewerMatrixBe0f60, (MATRIX*)VIEWER_BASE_MATRIX);

    // Up vector = third column of the (spinned) base rotation matrix.
    // The original reads 0x00aea03c / 0x00aea042 / 0x00aea048, i.e. byte
    // offsets +4 / +0xa / +0x10 from the matrix base = short indices 2, 5, 8.
    short* baseM = VIEWER_BASE_MATRIX;
    int iVar1 = baseM[2];   // m[0][2]
    int iVar5 = baseM[8];   // m[2][2]
    int iVar4 = baseM[5];   // m[1][2]
    int iVar2 = iVar1 * iVar1 + iVar5 * iVar5 + iVar4 * iVar4;
    // The `U` suffix Ghidra puts on the sign-correction mask is an artifact of
    // how it renders the original's signed shift - it must NOT propagate into
    // the operands. Without the (int) cast the divisor is unsigned, so the
    // whole division goes unsigned: a negative m[0][2] (any yaw past 180 deg)
    // divided as ~4.29e9, which drove the extracted yaw to 90 deg and left the
    // model upside down for the rest of the turn. The original is an idiv.
    int denom = (int)(iVar2 + ((iVar2 >> 0x1f) & 0xfff)) >> 0xc;
    if (denom == 0) denom = 1;           // guard: never divide by zero
    iVar1 = (iVar1 << 0xc) / denom;

    DAT_00ae9f64 = FUN_0040a990(iVar4, iVar5);
    {
        int sq = (int)(iVar1 * iVar1 + ((iVar1 * iVar1 >> 0x1f) & 0xfff)) >> 0xc;
        int inv = 0x1000 - sq;
        if (inv < 0) inv = 0;
        int uVar3 = FUN_0040a530(inv);
        DAT_00ae9f66 = FUN_0040a990(iVar1, uVar3);
    }
    DAT_00ae9f68 = 0;

    g_viewerMatrixBe0f60.t[0] = 0;
    g_viewerMatrixBe0f60.t[1] = 0;
    g_viewerMatrixBe0f60.t[2] = 0;
    RotMatrix((SVECTOR*)&DAT_00ae9f64, (MATRIX*)&g_viewerMatrixBe0f60);
    FUN_0040a250(&g_viewerMatrixBe0f60, &g_viewerMatrixBe0f80);

    // First column of the base matrix: 0x00aea038 / 0x00aea03e / 0x00aea044,
    // i.e. byte offsets +0 / +6 / +0xc = short indices 0, 3, 6.
    g_viewerSvecBe0fb0.x = baseM[0];
    g_viewerSvecBe0fb0.y = -baseM[3];
    g_viewerSvecBe0fb0.z = baseM[6];
    ApplyMatrix((MATRIX*)&g_viewerMatrixBe0f80, (SVECTOR*)&g_viewerSvecBe0fb0, (VECTOR*)&g_viewerSvecBe0fa0);
    DAT_00ae9f68 = FUN_0040a990(g_viewerSvecBe0fa0.y, g_viewerSvecBe0fa0.x);
}

// (0x0044e660) - Viewer action 0: display mode (rotation + examine confirm)
static void FUN_0044e660(void)
{
    if ((g_PlayerDpadPressed >> 8) & 0x40) {
        if (FUN_0044ed40()) {
            if (g_bItemViewerActionIndex != 1) {
                return;
            }
            FUN_00454fd0(g_bItemMenuSelectedItemId, 0, item_name_left() - g_ScreenOffsetX, 0xba - g_ScreenOffsetY);
            return;
        }
        g_bItemViewerActionIndex = 2;
        // 0x0044e6ce/db/e6 - the examine text comes from the item description
        // table (0x004c6160) through its own setter, NOT from set_message_display:
        // the index here is a raw table index, so feeding it to the room/global
        // message picker showed an unrelated RDT message instead.
        if (g_bItemMenuSelectedItemId != ITEM_INGRAM) {
            if (g_bItemMenuSelectedItemId == ITEM_MINIMI) {
                set_item_description_message(0x4e, 0);
                return;
            }
            set_item_description_message(g_bItemMenuSelectedItemId - 1, 0);
            return;
        }
        set_item_description_message(0x4d, 0);
        return;
    }
    if ((g_PlayerDpadPressed >> 8) & 0x80) {
        DAT_00ae9f49 = 5;
        g_bItemViewerZoomTimer = 0x40;
        DAT_00ae9f4c = 0;
        DAT_00ae9f4e = 0;
        DAT_00ae9f50 = 0;
        goto viewer_spin_input;
    }

    // Rotation is NOT applied by bumping the Euler angles directly: those are
    // model-space (RotMatrix builds Rx * Ry * Rz), so only r->y happens to line
    // up with a screen axis and r->x comes out as an in-screen roll. The
    // original instead builds a small incremental rotation from the spin input
    // vector and PRE-multiplies it onto the base matrix (FUN_0044eb30), so the
    // spin axes are always the view's:
    //   spin x (0x00ae9f4c) = roll, y (0x00ae9f4e) = yaw, z (0x00ae9f50) = pitch
    // FUN_0044eb30 then re-extracts 0x00ae9f64/66/68 from the product so the
    // examine combo check still has usable angles.
    {
        unsigned int rawHeld = (g_button_pressed_id >> 8) & 0xff;   // 0x00bf0a0d
        unsigned int dpad = g_PlayerDpadHeld;                       // 0x00be9840

        if (g_viewerPadWord != 0) {
            // 0x0044e722 - shift held: the d-pad drives zoom and roll instead
            if ((dpad & 0x5) != 0) {
                if ((dpad & 0x1) != 0) {
                    DAT_00aea04c = DAT_00aea04c + 0x64;
                    if (DAT_00aea04c > 0x2920) DAT_00aea04c = 0x2920;
                } else {
                    DAT_00aea04c = DAT_00aea04c - 0x64;
                    if (DAT_00aea04c < 0x1980) DAT_00aea04c = 0x1980;
                }
            }
            if ((dpad & 0xa) != 0) {
                DAT_00ae9f4c = ((dpad & 0x8) != 0) ? 0x20 : -0x20;
            } else {
                DAT_00ae9f4c = 0;
            }
            // The original leaves these alone because its recompute gate only
            // fires on the face buttons; the gate below keys off the spin
            // vector, so the other mode's axes have to be released here or a
            // stale value would keep the model turning by itself.
            DAT_00ae9f4e = 0;
            DAT_00ae9f50 = 0;
        } else {
            DAT_00ae9f4c = 0;
            // 0x0044e796 - the four face buttons (the on-screen arrow diamond).
            // Port addition: the d-pad drives the same two axes, so the model
            // can be rotated with the arrow keys as well.
            if ((rawHeld & 0xa0) != 0 || (dpad & 0xa) != 0) {
                DAT_00ae9f4e = ((rawHeld & 0x80) != 0 || (dpad & 0x8) != 0) ? 0x20 : -0x20;
            } else {
                DAT_00ae9f4e = 0;
            }
            if ((rawHeld & 0x50) != 0 || (dpad & 0x5) != 0) {
                DAT_00ae9f50 = ((rawHeld & 0x10) != 0 || (dpad & 0x1) != 0) ? -0x20 : 0x20;
            } else {
                DAT_00ae9f50 = 0;
            }
            // Aim button (raw 0x08) zooms without needing shift
            if ((g_PlayerPadHeld & 0x08) != 0) {
                DAT_00aea04c = DAT_00aea04c - 0x64;
                if (DAT_00aea04c < 0x1980) DAT_00aea04c = 0x1980;
            } else if ((g_PlayerPadHeld & 0x1000) != 0) {
                DAT_00aea04c = DAT_00aea04c + 0x64;
                if (DAT_00aea04c > 0x2920) DAT_00aea04c = 0x2920;
            }
        }
    }
    // 0x0044e7f8 - the original gates the recompute on the pad bits; gating on
    // the spin vector instead covers the d-pad additions above and skips the
    // extract/rebuild round trip on idle frames.
    if (DAT_00ae9f4c != 0 || DAT_00ae9f4e != 0 || DAT_00ae9f50 != 0) {
        FUN_0044eb30();
    }
viewer_spin_input:
    FUN_0044eca0();
}

// (0x0044e820) - Viewer action 1: examine spin-in animation
static void FUN_0044e820(void)
{
    g_bItemViewerZoomTimer = g_bItemViewerZoomTimer + 1;
    unsigned char step = g_bItemViewerZoomTimer * 4;
    if (0x20 < step) {
        step = 0x20;
    }
    DAT_00ae9f5e = (DAT_00ae9f5e + step) & 0xfff;
    DAT_00ae9f56 = (DAT_00ae9f56 + (short)step * -2) & 0xfff;
    if (0x300 < DAT_00ae9f5e) {
        g_bItemViewerActionIndex = 2;
        set_message_display(g_ItemHealTable[(unsigned int)g_ItemImageLookupTable[(unsigned int)g_bItemMenuSelectedItemId * 4 + 2] + 0x51], 0);
        return;
    }
    FUN_00454fd0(0xf00 | g_bItemMenuSelectedItemId, 0, item_name_left() - g_ScreenOffsetX, 0xba - g_ScreenOffsetY);
}

// (0x0044e8c0) - Viewer action 2: examine message wait
static void FUN_0044e8c0(void)
{
    if ((g_menu_choice_id & 0x80) == 0) {
        if (g_bItemMenuSelectedItemId == ITEM_RED_BOOK) {
            if (DAT_00ae9f5e != 0) {
                Flg_on((int)g_itemExaminedFlags, 0xd);
            }
        } else if (((ITEM_RED_BOOK < g_bItemMenuSelectedItemId) && (g_bItemMenuSelectedItemId < ITEM_FIRST_AID_SPRAY)) && (DAT_00ae9f5e != 0)) {
            g_bItemViewerActionIndex = 3;
            g_bItemViewerZoomTimer = 0x40;
            return;
        }
        g_bItemViewerActionIndex = 0;
    }
}

// (0x0044e920) - Viewer action 3: the doom book -> medal transformation.
//
// FUN_0044e8c0 hands control here (g_bItemViewerActionIndex = 3, g_bItemViewerZoomTimer = 0x40) once
// the examine spin has opened one of the two doom books. The first 0x40 frames
// are the viewer's normal fade-out - the same zoom/yaw/roll steps and light
// ramp as state 5 of FUN_0044e1b0 - and the last frame does the swap:
//   0x3F (doom book 2) -> 0x24 (wolf medal), 0x40 (doom book 1) -> 0x25 (eagle)
// The new slot sprite comes from data/medal.pix, two 1200-byte 20x30 images
// with the book id minus 0x3F picking the row, blitted into the cursor slot's
// own sheet row (g_ItemSlotIndices). menu_load_item_model then reloads the
// model and DAT_00ae9f49 = 0 restarts the viewer on the medal.
//
// This was previously a fabricated stub that reloaded the CURRENT item's image
// into sheet row 0 and dropped back to display mode: the book never became a
// medal and slot 0's sprite got overwritten with whatever the model buffer held.
static void FUN_0044e920(void)
{
    DAT_00aea04c = DAT_00aea04c - 0x322;
    DAT_00ae9f66 = DAT_00ae9f66 - 0xc0;
    DAT_00ae9f68 = DAT_00ae9f68 - 0x80;
    g_bItemViewerZoomTimer = g_bItemViewerZoomTimer - 1;
    g_spriteAnimB = g_bItemViewerZoomTimer >> 1;
    // 0x0044e953: the light colour ramp is a BYTE (g_bItemViewerZoomTimer << 2) - 1
    viewer_setup_lights((g_bItemViewerZoomTimer << 2) - 1);
    if (g_bItemViewerZoomTimer != 0) {
        return;
    }

    // 0x0044e9c0 - the cursor slot, read before g_bItemMenuSelectedItemId is rewritten
    unsigned char slot = (unsigned char)((unsigned char)(DAT_00ae9f23 >> 1) - 4);
    g_bItemMenuSelectedItemId = g_bItemMenuSelectedItemId - 0x3f;     // 0 = wolf, 1 = eagle
    LoadFile((char*)g_MedalPixPath, g_TimImageBuffer__bitmap, 0x20);
    LoadItemImage((int)g_bItemMenuSelectedItemId, (int)g_ItemSlotIndices[slot], (int)g_TimImageBuffer__bitmap);
    g_bItemMenuSelectedItemId = g_bItemMenuSelectedItemId + 0x24;     // ITEM_WOLF_MEDAL / ITEM_EAGLE_MEDAL
    ITEM_SLOTS[(unsigned int)slot * 2] = g_bItemMenuSelectedItemId;
    ITEM_SLOTS[(unsigned int)slot * 2 + 1] = 1;
    menu_load_item_model();
    DAT_00ae9f49 = 0;
}

// Viewer action dispatch table (PTR_FUN_004bf310)
static void (*const g_viewerActions[4])(void) = {
    FUN_0044e660, FUN_0044e820, FUN_0044e8c0, FUN_0044e920
};

// (0x0044e1b0) - Item 3D model viewer animation
// Returns non-zero when the viewer finishes and the menu should continue.
int FUN_0044e1b0(void)
{
    int iVar1;
    unsigned int uVar7;
    unsigned char bVar4;
    unsigned char bVar5;

    // 0x0044e1b5 - FUN_00497e40 is GetAsyncKeyState(VK_SHIFT) reduced to 0/1
    // (0x00497e00: push 0x10, call GetAsyncKeyState, store 1 if nonzero).
    // Shift switches the d-pad from rotate to zoom+roll, so this must be the
    // live key state - the previous constant 0x10 pinned it to "shift held".
    g_viewerPadWord = (plat_key_state(0x10) != 0) ? 1 : 0;
    switch (DAT_00ae9f49) {
    case 0:
        // Model setup: resolve the model pointers and initialise the joint chain
        if (g_itemModelTmdBase == 0) {
            return 0;   // the model file failed to load; keep the viewer inert
        }
        DAT_00ae9f49 = 1;
        ResolveAnimPointers((unsigned char*)(g_itemModelTmdBase + 4));
        DAT_00ae9f4b = *(unsigned char*)(g_itemModelTmdBase + 8);
        // Sca chain: Sca[2] (root) <- Sca[1] <- Sca[0]
        InitScaMatrix(0, (ScaMatrixData*)&g_viewerScaMatrices[2 * 0x50 / 4]);
        InitScaMatrix((int)&g_viewerScaMatrices[2 * 0x50 / 4], (ScaMatrixData*)&g_viewerScaMatrices[0x50 / 4]);
        InitScaMatrix((int)&g_viewerScaMatrices[0x50 / 4], (ScaMatrixData*)&g_viewerScaMatrices[0]);
        // Items have 1 or 2 objects; never trust the file. The do-while below
        // walks indices [2 - count, 1], so a count outside 1..2 would run off
        // both the joint array and g_viewerObjRot.
        bVar4 = (unsigned char)DAT_00ae9f4b;
        if (bVar4 < 1) bVar4 = 1;
        if (bVar4 > 2) bVar4 = 2;
        bVar5 = 2 - bVar4;
        do {
            bVar4 = bVar4 - 1;
            {
                int uVar6 = bVar5;
                bVar5 = bVar5 + 1;
                *(int*)((BYTE*)g_viewerJoints + uVar6 * 0x14) = 0;
                *(int**)((BYTE*)g_viewerJoints + 4 + uVar6 * 0x14) = (int*)((BYTE*)g_viewerScaMatrices + bVar4 * 0x50);
                *(short*)((BYTE*)&DAT_00ae9f54 + uVar6 * 8) = 0;
                *(short*)((BYTE*)&DAT_00ae9f56 + uVar6 * 8) = 0;
                *(short*)((BYTE*)&DAT_00ae9f58 + uVar6 * 8) = 0;
                *(int*)((BYTE*)g_viewerScaMatrices + 0x18 + uVar6 * 0x50) = 0;
                *(int*)((BYTE*)g_viewerScaMatrices + 0x1c + uVar6 * 0x50) = 0;
                *(int*)((BYTE*)g_viewerScaMatrices + 0x20 + uVar6 * 0x50) = 0;
            }
        } while (bVar4 != 0);
        DAT_00aea04c = -0xaf00; // 0x0044e2ac - start far away; the intro closes in
        DAT_00ae9f64 = 0;       // 0x0044e2b8
        DAT_00aea050 = 0;
        DAT_00ae9f66 = 0;       // 0x0044e2c3
        DAT_00aea054 = 0;
        // 0x0044e2ce - the resting pose is the identity rotation. The item is
        // framed by the menu camera main_menu installs (from (15000,0,0) toward
        // the origin), not by a baked rotation here.
        DAT_00ae9f68 = 0;
        DAT_00ae9f4c = 0;
        DAT_00ae9f4e = 0;
        DAT_00ae9f50 = 0;
        RotMatrix((SVECTOR*)&DAT_00ae9f64, (MATRIX*)VIEWER_BASE_MATRIX);
        g_bItemViewerZoomTimer = 0x40;
        // fall through
    case 1:
        // Intro animation (0x0044e2ff-0x0044e31b): close in while spinning.
        // 0x40 frames x 0xc0 of yaw = 0x3000 = three full turns, and 0x40 x
        // 0x80 of roll = 0x2000 = two full turns, so both angles land back on
        // 0 and the model comes to rest in its identity pose.
        DAT_00aea04c = DAT_00aea04c + 0x322;
        DAT_00ae9f66 = DAT_00ae9f66 + 0xc0;
        DAT_00ae9f68 = DAT_00ae9f68 + 0x80;
        g_bItemViewerZoomTimer = g_bItemViewerZoomTimer - 1;
        if (g_bItemViewerZoomTimer != 0) goto viewer_draw;
        switch (DAT_00ae9f10) {
        case 0:
            DAT_00ae9f49 = 2;
            g_bItemViewerActionIndex = 0;
            goto viewer_draw;   // no message for the plain status-screen viewer
        case 3:
            g_selectedItemId = g_bItemMenuSelectedItemId;
            DAT_00ae9f49 = 3;
            if (g_TotalHeldItems < g_totalInventorySlots) {
                uVar7 = 0xc0;
            } else {
                if (((ITEM_EXPLOSIVE_ROUNDS < g_bItemMenuSelectedItemId) && (g_bItemMenuSelectedItemId < ITEM_EMPTY_BOTTLE)) || (g_bItemMenuSelectedItemId == ITEM_INK_RIBBONS)) {
                    // Same doubling the pickup itself will apply, so the
                    // "you got it" / "no room" choice matches what lands in the
                    // inventory (PS1 FUN_8002e1a4 repeats IncludeCurrentItem's
                    // shift for exactly this decision).
                    unsigned char pickupQty = dc_item_pickup_quantity(
                        g_bItemMenuSelectedItemId,
                        *(unsigned char*)(*(int*)((int)g_pRoomActionEntry + 8) + 9));
                    bVar5 = 0;
                    bVar4 = g_totalInventorySlots;
                    do {
                        unsigned char* pbVar2 = (unsigned char*)((unsigned int)bVar5 * 2 + (unsigned int)g_ItemSlotsPointer);
                        if ((*pbVar2 == g_bItemMenuSelectedItemId) &&
                            ((unsigned short)((unsigned short)pbVar2[1] +
                             (unsigned short)pickupQty) < 251)) {
                            set_message_display(0xc0, 0);
                            break;
                        }
                        bVar4 = bVar4 - 1;
                        bVar5 = bVar5 + 1;
                    } while (bVar4 != 0);
                }
                uVar7 = 0xc2;
            }
            break;
        case 4:
            uVar7 = 0xc1;
            g_selectedItemId = g_bItemMenuSelectedItemId;
            DAT_00ae9f49 = 3;
            break;
        case 6:
            DAT_00ae9f49 = 4;
            uVar7 = 199;
            break;
        default:
            goto viewer_draw;
        }
        set_message_display(uVar7, 0);
viewer_draw:
        g_spriteAnimB = 0x20 - (g_bItemViewerZoomTimer >> 1);
        // 0x0044e538: intro colour fades in as 0xFF - (g_bItemViewerZoomTimer << 2)
        viewer_setup_lights(0xff - (g_bItemViewerZoomTimer << 2));
        break;

    case 2:
        // Display mode: rotation input + examine confirm (action table)
        g_viewerActions[g_bItemViewerActionIndex]();
        break;

    case 3:
        goto viewer_state3;
    case 4:
        DAT_00ae9f66 = DAT_00ae9f66 - 0x10;
viewer_state3:
        if ((g_menu_choice_id & 0x80) == 0) {
            DAT_00ae9f49 = 5;
            g_bItemViewerZoomTimer = 0x40;
            if (DAT_00ae9f10 != 4) {
                if ((g_menu_choice_id & 1) == 0) {
                    uVar7 = 6;
                } else {
                    uVar7 = 5;
                }
            } else {
                // 0x0044e5b7 - menu mode 4 is the script-driven award
                // (SCD `got_item`): the viewer itself hands over the item,
                // there is no message post-action to do it. Global message
                // 0xc1 ("You got the X.") carries no trailing action bytes.
                room_event_take_item();
                break;
            }
        } else {
            if ((DAT_00ae9f10 != 4) && (*g_MessageCurrentPtr != 8)) break;
            if (((g_PlayerPadHeld >> 8) & 0xa0) == 0) break;
            uVar7 = 4;
        }
        play_sfx(3, uVar7, 0);
        break;

    case 5:
        // Exit animation (0x0044e397-0x0044e3ba): mirrors the intro
        g_bItemViewerZoomTimer = g_bItemViewerZoomTimer - 1;
        if (g_bItemViewerZoomTimer == 0) {
            return 1;
        }
        DAT_00aea04c = DAT_00aea04c - 0x322;
        DAT_00ae9f66 = DAT_00ae9f66 - 0xc0;
        DAT_00ae9f68 = DAT_00ae9f68 - 0x80;
        g_spriteAnimB = g_bItemViewerZoomTimer >> 1;
        // 0x0044e3d0: exit colour fades out as (g_bItemViewerZoomTimer << 2) - 1
        viewer_setup_lights((g_bItemViewerZoomTimer << 2) - 1);
        break;
    }

    if (DAT_00ae9f49 != 0) {
        FUN_0044ea50();
    }
    if ((DAT_00ae9f10 == 0) && (g_bItemViewerActionIndex == 0)) {
        FUN_00454fd0(g_bItemMenuSelectedItemId, 0, item_name_left() - g_ScreenOffsetX, 0xba - g_ScreenOffsetY);
    }
    return 0;
}

// (0x00484420) - Process texture for menu item model
// src: the loaded .ivm file buffer; dst: unused in the port (the original
// fixed buffers happened to sit exactly a TIM image apart). The TMD header
// inside the .ivm starts right after the TIM image, so the async processor
// receives the TMD base computed from the TIM header.
static void FUN_004841f0(void);
void FUN_00484420(void* src, void* dst)
{
    g_itemModelSrc = (int)src;
    unsigned char* p = (unsigned char*)src;
    g_itemModelTmdBase = 0;
    // Validate the .ivm TIM header before trusting its lengths
    if (p != 0 && *(int*)p == 0x10 && (*(int*)(p + 4) & 7) <= 2) {
        int clutLen = *(int*)(p + 8);
        int imgLen = *(int*)(p + 8 + clutLen);
        g_itemModelTmdBase = (int)(p + 8 + clutLen + imgLen);
    }
    ExecAsync((void*)FUN_004841f0);
}

// (0x004844b0) - menu cleanup sub: queues the async teardown of the item
// viewer's D3D state. Async body 0x00484450:
//   VideoDriver_ClearState348(DAT_008fc0b0 = item page, pD3D)
//   VideoDriver_ClearState348(DAT_008f8908 = STP page,  pD3D)
//   CMarniDirect3DTMD::CleanupObjects on the item TMD slots
//     0x008f8d88 and 0x008fa31c (stride 0x1594, loop bound 0x008fb8b0 = 2)
//   CMarniDirect3DTMD::CleanupObjects on the shared slot DAT_00920b28.
static void menu_viewer_teardown_async(void)
{
    VideoDriver_ClearState348(&g_psxTextureArray[0x15 * 0x1b60], g_pMarniDirect3D); // item page (DAT_008fc0b0)
    VideoDriver_ClearState348(g_itemStpPage, g_pMarniDirect3D);                     // STP page (DAT_008f8908)
    for (int i = 0; i < 2; i++) {
        ((CMarniDirect3DTMD*)g_itemTmdSlots[i])->CleanupObjects(g_pMarniDirect3D);
    }
    ((CMarniDirect3DTMD*)g_itemSharedTmdSlot)->CleanupObjects(g_pMarniDirect3D);
}

void FUN_004844b0(void)
{
    ExecAsync((void*)menu_viewer_teardown_async);
}

// ============================================================================
// Item model processor (0x004841f0) and viewer renderer
// ============================================================================

// (0x0040a990) - GTE-style atan2, returning the angle in the game's
// 0x1000 = 360 deg units (the same scale RotMatrix consumes).
// Original: fpatan * (180/pi) * (2048/180); the two literals are the doubles
// at 0x004af050 (57.29577791868204) and 0x004af058 (11.377777777777778).
// A previous revision used 24.7555... there, which came out 2.18x too large -
// the angles the examine recompute extracts from the base matrix are fed
// straight back into RotMatrix, so any scale error compounds every frame.
static int FUN_0040a990(int y, int x)
{
    double rad = atan2((double)y, (double)x);
    return (int)(rad * 57.29577791868204 * 11.377777777777778);
}

// (0x0040a530) - GTE-style fixed-point square root: fmul by 1/4096 (0x004af028),
// fsqrt, fmul by 4096.0 (0x004af030). Input and output are both 20.12 fixed
// point, i.e. sqrt(x * 4096) - not the plain integer sqrt.
static int FUN_0040a530(int value)
{
    if (value < 0) return 0;
    return (int)(sqrt((double)value * 0.000244140625) * 4096.0);
}

// (0x0040a250) - Transpose a MATRIX (3x3 shorts) into a 9-short buffer
// The original reads the source in column order (6-byte strides) and writes
// the rows consecutively, i.e. dst = src^T.
static void FUN_0040a250(MATRIX* src, MATRIX* dst)
{
    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 3; i++) {
            ((short*)dst)[j * 3 + i] = src->m[i][j];
        }
    }
}

// (0x00483580) - Compose the viewer joint chain matrices into the output
// The "joint" argument is the ScaMatrixData chain root; the chain is linked
// through the OWNER back-pointers (ScaMatrixData.owner at +0x48, set by
// InitScaMatrix), with each parent's child back-ref at +0x4C. The walk
// follows the owners to the root and composes local -> world from the root
// down, exactly like the original. Shared with render_room_objects
// (FUN_004745f0) which composes each room item/model object the same way.
void FUN_00483580(int* joint, MATRIX* out)
{
    int* chain[0x14];
    int chainCount = 0;
    int* p = joint;
    while (p != 0 && chainCount < 0x14) {
        chain[chainCount++] = p;
        p = (int*)p[0x12];   // owner (parent)
    }

    // Compose from the deepest (root) down to the given joint
    for (int i = chainCount - 1; i >= 0; i--) {
        int* j = chain[i];
        if (j[0x12] == 0) {
            for (int k = 0; k < 8; k++) {
                j[9 + k] = j[1 + k];
            }
        } else {
            CompMatrix((MATRIX*)(j[0x12] + 0x24), (MATRIX*)(j + 1), (MATRIX*)(j + 9));
        }
    }
    // 0x004835ee - compose against the camera. main_menu (0x004638e3) installs
    // a menu camera at (15000,0,0) looking at the origin, so g_RoomCameraData
    // here is a 90 deg rotation about Y with t = (0,0,15000): it maps the root
    // Sca's +X translation ("zoom") onto view depth (15000 - zoom) and leaves
    // world Y on the screen vertical, which is what makes the base rotation's
    // y component read as a turntable.
    CompMatrix(&g_RoomCameraData, (MATRIX*)(joint + 9), out);
}

// (0x004846d0) - Queue the item model draw for the given render slot
void FUN_004846d0(int slot)
{
    g_itemRenderSlot = slot;
    ExecAsync((void*)FUN_004844c0);
}

// ----------------------------------------------------------------------------
// Ordering-table quad records used by the async item draw (original buffers:
// 0x00ac3508 and 0x00922100, three 0x58-byte records, one per render slot).
// ----------------------------------------------------------------------------
static DWORD g_itemViewerOffsetQuadRecs[3][0x58 / 4];     // 0x00ac3508
static DWORD g_itemViewerFullQuadRecs[3][0x58 / 4];       // 0x00922100

// (0x004846f0) - Insert the screen-offset background quad record for render
// `slot` (< 3) at `depth`. Fields: +0x00 prim type 1, +0x4C code 4,
// +0x50/+0x54 = g_SubpixelOffsetX/Y. Called from item_viewer_async_draw
// (0x00484630) right before Direct3DTMD_Transform.
// NOTE: this port's ordering table only consumes depth 0xFFF
// (see OT_InsertPrimitive), so like every other sub-0xFFF OT record in the
// original this one stays queued-but-unwalked; kept for pipeline parity.
void FUN_004846f0(int slot, int depth)
{
    if (slot >= 3) return;
    DWORD* rec = g_itemViewerOffsetQuadRecs[slot];
    rec[0]        = 1;                          // +0x00: primitive type
    rec[0x4C / 4] = 4;                          // +0x4C: code
    rec[0x50 / 4] = (DWORD)g_SubpixelOffsetX;   // +0x50
    rec[0x54 / 4] = (DWORD)g_SubpixelOffsetY;   // +0x54
    OT_InsertPrimitive(rec, depth);
}

// (0x00484740) - Insert the fixed full-screen quad record (160x120) for
// render `slot` (< 3) at `depth`. Same layout as 0x004846f0 but with hard
// coded extents 0xA0 x 0x78 instead of the live subpixel offsets. Called at
// the very end of item_viewer_async_draw, after the STP blend pass.
void FUN_00484740(int slot, int depth)
{
    if (slot >= 3) return;
    DWORD* rec = g_itemViewerFullQuadRecs[slot];
    rec[0]        = 1;      // +0x00: primitive type
    rec[0x4C / 4] = 4;      // +0x4C: code
    rec[0x50 / 4] = 0xA0;   // +0x50: width  (160)
    rec[0x54 / 4] = 0x78;   // +0x54: height (120)
    OT_InsertPrimitive(rec, depth);
}

// (0x004844c0) - Async item model draw: build the transform matrix from the
// GTE buffer and call CMarniDirect3DTMD::Transform on the item model slot.
static void FUN_004844c0(void)
{
    int slot = g_itemRenderSlot;

    // Build the 4x4 float matrix from the GTE rotation/translation buffer
    // (same layout as the entity render path).
    float transformMatrix[16];
    float scale = 0.00024414063f; // 1/4096
    transformMatrix[0]  = (float)g_gteRotTransMatrix.m[0][0] * scale;
    transformMatrix[4]  = (float)g_gteRotTransMatrix.m[0][1] * scale;
    transformMatrix[8]  = (float)g_gteRotTransMatrix.m[0][2] * scale;
    transformMatrix[1]  = (float)g_gteRotTransMatrix.m[1][0] * scale;
    transformMatrix[5]  = (float)g_gteRotTransMatrix.m[1][1] * scale;
    transformMatrix[9]  = (float)g_gteRotTransMatrix.m[1][2] * scale;
    transformMatrix[2]  = (float)g_gteRotTransMatrix.m[2][0] * scale;
    transformMatrix[6]  = (float)g_gteRotTransMatrix.m[2][1] * scale;
    transformMatrix[10] = (float)g_gteRotTransMatrix.m[2][2] * scale;
    transformMatrix[12] = (float)g_gteRotTransMatrix.t[0];
    transformMatrix[13] = (float)g_gteRotTransMatrix.t[1];
    transformMatrix[14] = (float)g_gteRotTransMatrix.t[2];
    transformMatrix[3]  = 0.0f;
    transformMatrix[7]  = 0.0f;
    transformMatrix[11] = 0.0f;
    transformMatrix[15] = 1.0f;

    // The view distance already comes through t[2]: the menu camera contributes
    // t = (0,0,15000) and rotates the root Sca's +X "zoom" onto -Z, so the depth
    // is 15000 - zoom - 59800 at the start of the intro, 8472 at rest, and it
    // never crosses the camera plane.

    int depth = slot + 10;
    if (slot >= 3) slot = 0;
    // 0x004846f0: queue the screen-offset background quad record at `depth`.
    FUN_004846f0(slot, depth);
    ((CMarniDirect3DTMD*)g_itemTmdSlots[slot])->Transform(g_pMarniDirect3D, (void*)(size_t)depth, transformMatrix, 0);

    // 0x00484670: the semi-transparency pass, and the only place the item
    // viewer's blend alpha is ever applied. The gate is the SHARED slot's
    // m_initialized (DAT_009220b8 = 0x00920b28 + 0x1590), raised by its Create
    // in FUN_004841f0 and cleared there for every non-transparent item.
    CMarniDirect3DTMD* shared = (CMarniDirect3DTMD*)g_itemSharedTmdSlot;
    if ((slot == 0) && (shared->m_initialized == 1)) {
        // 0x0048467f: stamp DAT_004d2c10 into +0x68 and +0x78 of all 32 object
        // records of item slot 0 - both m_objectData (0x4D0) and its copy
        // (0xD10), 0x84 apart, whichever side Transform queued. This runs AFTER
        // the Transform above on purpose: the draw queue holds POINTERS to the
        // records (see TmdQueueObject), so the alpha still lands on this
        // frame's opaque pass and turns it into the 50% pass.
        BYTE* rec = g_itemTmdSlots[0] + 0x4D0 + 0x68;
        for (int i = 0; i < 32; i++) {
            *(DWORD*)(rec)        = (DWORD)g_itemModelBlendFlag;
            *(DWORD*)(rec + 0x10) = (DWORD)g_itemModelBlendFlag;
            rec += 0x84;
        }
        // Second pass at the SAME depth, from the STP page (opaque texels
        // only). Coplanar with the first: DrawTriangles3D depth-tests
        // LESS_EQUAL, so both survive, and where the two agree the texel
        // colour is identical - so the pass order does not matter.
        shared->Transform(g_pMarniDirect3D, (void*)(size_t)10, transformMatrix, 0);
    }
    // 0x00484740: queue the fixed 160x120 full-screen quad record at `depth`
    // (tail of item_viewer_async_draw).
    FUN_00484740(slot, depth);
}

// (0x0044ea50) - Viewer render: compute the model matrices and queue the draw
static void FUN_0044ea50(void)
{
    DAT_00ae9f64 = DAT_00ae9f64 & 0xfff;
    DAT_00ae9f66 = DAT_00ae9f66 & 0xfff;
    DAT_00ae9f68 = DAT_00ae9f68 & 0xfff;
    // Base (spin) rotation into Sca[2].localMatrix (0xaea038)
    RotMatrix((SVECTOR*)&DAT_00ae9f64, (MATRIX*)((BYTE*)g_viewerScaMatrices + 2 * 0x50 + 4));
    *(int*)((BYTE*)g_viewerScaMatrices + 2 * 0x50) = 0;

    unsigned char local_1 = (unsigned char)DAT_00ae9f4b;
    if (local_1 < 1) local_1 = 1;   // never trust the model object count
    if (local_1 > 2) local_1 = 2;
    unsigned char local_2 = 2;
    do {
        local_1 = local_1 - 1;
        local_2 = local_2 - 1;
        {
            int uVar1 = local_2;
            // Per-object rotation into Sca[uVar1].localMatrix
            RotMatrix((SVECTOR*)((BYTE*)&DAT_00ae9f54 + uVar1 * 8),
                      (MATRIX*)((BYTE*)g_viewerScaMatrices + 4 + uVar1 * 0x50));
            *(int*)((BYTE*)g_viewerScaMatrices + uVar1 * 0x50) = 0;
            // Compose the Sca chain from the joint's matrix
            int* joint = *(int**)((BYTE*)g_viewerJoints + 4 + uVar1 * 0x14);
            // The compose already carries the zoom into t[2] via the camera.
            FUN_00483580(joint, &g_viewerMatrixBe0f60);
            // 0x0044eaef - multAndSetLightMatrix(&g_RoomCameraData)
            MATRIX lightM;
            MulMatrix0(&g_lightMatrix, &g_RoomCameraData, &lightM);
            SetLightMatrix(&lightM);
            SetRotAndTransMatrix(&g_viewerMatrixBe0f60);
            FUN_004846d0(1 - uVar1);
        }
    } while (local_1 != 0);
}


// (0x004842c0) - Knock the semi-transparent texels out of the STP item page
// The original locks each of the page's CLUT sub-surfaces (vtable[4]/[5]),
// walks all 256 palette entries and, for every colour with the PS1 STP bit
// (0x8000) set, rewrites every pixel using that index to 0 - palette index 0
// being the PS1's fully transparent colour, and the index this port's
// VTable_CreateTextureHandle also maps to alpha 0.
//
// The result is a copy of the item texture holding ONLY the opaque texels.
// Drawn over a 50% pass of the full texture (see FUN_004844c0), it restores
// the per-texel semi-transparency the PS1 GPU did in hardware: glass stays at
// half alpha, labels and caps come back to full brightness.
//
// The original compares every pixel against every STP colour (256 * w * h);
// a 256-entry lookup gives the identical result in one pass.
static void ItemStpPage_KnockOutStpTexels(void)
{
    PSXTexture* tex = (PSXTexture*)g_itemStpPage;

    BYTE* pixels = (BYTE*)tex->m_pPixelData;
    WORD* clut   = (WORD*)tex->m_pCLUTData;
    if (pixels == NULL || clut == NULL) return;
    if (tex->m_BitDepth != 4 && tex->m_BitDepth != 8) return;   // no CLUT, nothing to key

    const int entries = 1 << tex->m_BitDepth;          // 16 (4bpp) or 256 (8bpp)
    const int w       = (int)tex->m_WidthPixels;
    const int h       = (int)tex->m_Height;
    const int pitch   = (int)tex->m_RowStride;
    if (w <= 0 || h <= 0 || pitch <= 0) return;

    // Union of every CLUT's STP mask: the sub-surfaces share one pixel buffer,
    // so the original's per-CLUT passes accumulate exactly like this.
    bool stp[256] = { false };
    DWORD clutCount = tex->m_NumCLUTs ? tex->m_NumCLUTs : 1;
    if (clutCount > 8) clutCount = 8;
    for (DWORD n = 0; n < clutCount; n++) {
        const WORD* pal = clut + n * entries;
        for (int i = 0; i < entries; i++) {
            if (pal[i] & 0x8000) stp[i] = true;
        }
    }

    int knocked = 0;
    for (int y = 0; y < h; y++) {
        BYTE* row = pixels + y * pitch;
        if (tex->m_BitDepth == 8) {
            for (int x = 0; x < w; x++) {
                if (stp[row[x]]) { row[x] = 0; knocked++; }
            }
        } else {
            for (int x = 0; x < w; x += 2) {
                BYTE two = row[x >> 1];
                BYTE lo = (BYTE)(two & 0x0F), hi = (BYTE)(two >> 4);
                if (stp[lo]) { lo = 0; knocked++; }
                if (stp[hi]) { hi = 0; knocked++; }
                row[x >> 1] = (BYTE)((hi << 4) | lo);
            }
        }
    }

    dbg_printf("[ITEM] STP page: %dbpp %dx%d, %d texels knocked out\n",
               (int)tex->m_BitDepth, w, h, knocked);
}

// (0x004841f0) - Item model async processor
// Parses the .ivm file loaded by menu_load_item_model: creates the item
// texture page (bank 0x15) and the Direct3DTMD slot(s) from the TMD header.
static void FUN_004841f0(void)
{
    int local_10 = *(int*)(g_itemModelTmdBase + 8);   // TMD object count
    g_itemModelTmdCount = 0;
    if (g_itemModelTmdBase == 0) return;
    if (local_10 < 1 || local_10 >= 3) return;

    ResolveAnimPointers((unsigned char*)(g_itemModelTmdBase + 4));

    BYTE* itemPage = &g_psxTextureArray[0x15 * 0x1b60];

    // 0x0048422c / 0x00484244: release the previous item's D3D textures on BOTH
    // pages. ECX is the page in the original, not the video driver - passing
    // g_pMarniDirect3D here released nothing and leaked a texture slot per
    // examine, and left the "created" flags reading as another object's fields.
    VideoDriver_ClearState348(itemPage, g_pMarniDirect3D);
    VideoDriver_ClearState348(g_itemStpPage, g_pMarniDirect3D);

    if (ITEM_PAGE_CREATED(itemPage) == 0) {
        LoadPSXImage((PSXTexture*)itemPage, (void*)g_itemModelSrc, 1);
        Direct3DTIM_Create(itemPage, g_pMarniDirect3D);
    }

    // 0x0048427d: cleared FIRST, so an opaque item viewed after a glass one
    // stamps 0 (= opaque) instead of inheriting the 50%.
    g_itemModelBlendFlag = 0;

    if (CheckTmdTransparency(g_itemModelTmdBase + 0xc) != 0) {
        g_itemModelBlendFlag = 0x3f000000;   // 0.5f
        if (ITEM_PAGE_CREATED(g_itemStpPage) != 1) {
            // Own copy of the same TIM (copyData = 1 allocates its own pixel
            // and CLUT buffers, so knocking texels out below cannot touch the
            // item page).
            LoadPSXImage((PSXTexture*)g_itemStpPage, (void*)g_itemModelSrc, 1);
            ItemStpPage_KnockOutStpTexels();
            // 0x0048433c-0x0048435f: the original creates the per-CLUT handles
            // by hand and then sets 0x008f8c50 (= page + 0x348). That is what
            // Direct3DTIM_Create does, and it leaves the flag clear when a
            // handle fails - which is the safer half of the difference.
            Direct3DTIM_Create(g_itemStpPage, g_pMarniDirect3D);
        }
    }

    for (int i = 0; i < local_10; i++) {
        // Each TMD object is stored into its own slot (0x008f8d88 + i*0x1594
        // in the original); a previous revision reused one slot for all
        // objects, so multi-object items only ever rendered their last object.
        CMarniDirect3DTMD* itemSlot = (CMarniDirect3DTMD*)g_itemTmdSlots[i];
        itemSlot->CleanupObjects(g_pMarniDirect3D);
        int st = PSXObject_Store(itemSlot, (int*)g_itemModelTmdBase, i, 0xffffffff, 0x100);
        int cr = itemSlot->Create(g_pMarniDirect3D, itemPage, (void*)1);
        char dbg[160];
        sprintf_s(dbg, sizeof(dbg), "[ITEM] store=%d create=%d tmd=%p count=%d obj=%d\n", st, cr, (void*)g_itemModelTmdBase, local_10, i);
        OutputDebugStringA(dbg);
    }
    g_itemModelTmdCount = local_10;

    // 0x004843cf: the shared slot is torn down for EVERY item and only rebuilt
    // when this one is transparent. CleanupObjects clears m_initialized, which
    // is the exact gate FUN_004844c0 reads - without it, examining an opaque
    // item after a glass one would leave the previous model's geometry queued
    // for a second pass. (This is the shared slot, not the item slots above;
    // those are cleaned inside the loop and re-Created right after.)
    ((CMarniDirect3DTMD*)g_itemSharedTmdSlot)->CleanupObjects(g_pMarniDirect3D);
    if (ITEM_PAGE_CREATED(g_itemStpPage) == 1) {
        PSXObject_Store((CMarniDirect3DTMD*)g_itemSharedTmdSlot, (int*)g_itemModelTmdBase, 0, 0xffffffff, 0x100);
        ((CMarniDirect3DTMD*)g_itemSharedTmdSlot)->Create(g_pMarniDirect3D, g_itemStpPage, (void*)1);
    }
}


// (0x00438800) - EKG secondary line gradient update
// Steps the secondary line's gradient color toward the primary color and
// queues the line. cVar1 = (X0 - X1) from the EKG secondary line struct;
// each gradient channel is extrapolated by (X0 - X1) * param.
void FUN_00438800(int a, int b, int c)
{
    unsigned char* line = g_EkgSecondaryLine;   // 0x00be1184
    char x0 = *(char*)(line + 4);               // DAT_00be1188
    char x1 = *(char*)(line + 8);               // DAT_00be118c
    char delta = x0 - x1;
    line[0xF] = (unsigned char)(line[0xC] - delta * (char)a);   // DAT_00be1193
    line[0x10] = (unsigned char)(line[0xD] - delta * (char)b);  // DAT_00be1194
    line[0x11] = (unsigned char)(line[0xE] - delta * (char)c);  // DAT_00be1195
    FUN_00470e60(line, 10);
    line[0xC] = line[0xF];
    line[0xD] = line[0x10];
    line[0xE] = line[0x11];
}

