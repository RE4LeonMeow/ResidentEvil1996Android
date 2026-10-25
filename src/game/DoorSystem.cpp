// ============================================================================
// DoorSystem.cpp - the 3D door-opening transition (0x004443c0..0x00444763)
//
// When the player walks through a door, room_transition_load (DoorSystem.cpp)
// loads the door's .dor data file and spawns task 1 = FUN_00444770, which runs
// the classic full-screen door animation while the destination room loads
// underneath:
//
//   FUN_004443c0  init      - relocate the .dor's pointer tables, resolve the
//                             TMD per-object pointers, set g_main_state_flags
//                             bit 0x4000000 (the bit room_transition_load
//                             polls), Task_sleep(1)
//   FUN_00444540  loop      - black rect for the first 3 frames, then dispatch
//                             the command-entry scripts (38-opcode bytecode
//                             interpreter) and render the 12 order entries
//                             (door panels) through the TMD queue
//   FUN_00444500  teardown  - clear bit 0x4000000, restore image buffers,
//                             g_bGameActive = 2
//
// The .dor file layout (offsets relative to the buffer):
//   0x00  header: [script-table offset (0x0C)] [TMD header offset] [TIM offset]
//   0x0C  script table: 35 offsets (relative to the table) + NULL; entry 0 is
//         the main script, 1..34 the phase scripts. Runtime-relocated by adding
//         the relocated dword0 (i.e. table base).
//   0x150 scripts (bytecode for the 38-opcode interpreter)
//   0x1408 TMD header (magic 0x41, nobj = 12): per-object entries of 7 dwords
//         (vertPtr, vertLen, normPtr, normLen, primPtr, primLen, pad) whose
//         pointers are header-relative offsets; ResolveAnimPointers converts
//         them to absolute pointers in place. opcode 0x10 stores the resolved
//         vert pointer into the order entry for the per-vertex animation.
//   0x900C+ door vertex pool, 0xAFD0+ PSX TIM texture
//
// The command table is 8 entries x 0x38 at 0x00be0c10 (status word, counter,
// data pointer, workspace). The order table is 12 entries x 0x80 at 0x00be05c0
// (flags, model index, hierarchy ScaMatrixData, rotation/position/velocity,
// model vertex pointer). The scripts branch on the byte variables latched by
// room_transition_load from the door record (door direction = record+0x08,
// door type = record+0x0A, etc.).
// ============================================================================
#include "../Globals.h"
#include "../DebugPrint.h"
#include "../system/AssetPath.h"
#include "../marni/Marni3DObject.h"
#include "../marni/MarniSystem.h"
#include "../marni/PSXTexture.h"
#include "TmdRenderer.h"
#include "FileLoader.h"

// --- forward declarations of port-side helpers ---------------------------------
extern int  PSXObject_Store(CMarniDirect3DTMD* self, int* tmdHdr, int objIndex,
                            int bankOrTpage, int texRef);   // Marni3DObject.cpp
extern void LoadPSXImage(PSXTexture* tex, void* buf, int mode);  // TextureLoader.cpp
extern void InitScaMatrix(int parentPtr, ScaMatrixData* matrix); // GteMatrix.cpp
extern void ResolveAnimPointers(unsigned char* data);            // EntityModelLoader.cpp

// ============================================================================
// Door data file names (0x004b39a0 - 34 entries of 0x28, indexed by the door
// record byte +0x0A). The original's "./usa/item_m1/doorNN.dor" maps to
// GAME_DATA_ROOT "item_m1\\doorNN.dor".
// ============================================================================
static const char* const g_doorFileNameTable[0x22] = {
    GAME_DATA_ROOT "item_m1\\door00.dor",
    GAME_DATA_ROOT "item_m1\\door01.dor",
    GAME_DATA_ROOT "item_m1\\door02.dor",
    GAME_DATA_ROOT "item_m1\\door03.dor",
    GAME_DATA_ROOT "item_m1\\door04.dor",
    GAME_DATA_ROOT "item_m1\\door05.dor",
    GAME_DATA_ROOT "item_m1\\door06.dor",
    GAME_DATA_ROOT "item_m1\\door07.dor",
    GAME_DATA_ROOT "item_m1\\door08.dor",
    GAME_DATA_ROOT "item_m1\\door09.dor",
    GAME_DATA_ROOT "item_m1\\door10.dor",
    GAME_DATA_ROOT "item_m1\\door11.dor",
    GAME_DATA_ROOT "item_m1\\door12.dor",
    GAME_DATA_ROOT "item_m1\\door13.dor",
    GAME_DATA_ROOT "item_m1\\door14.dor",
    GAME_DATA_ROOT "item_m1\\mon.dor",
    GAME_DATA_ROOT "item_m1\\ele03.dor",
    GAME_DATA_ROOT "item_m1\\ele01.dor",
    GAME_DATA_ROOT "item_m1\\ele01a.dor",
    GAME_DATA_ROOT "item_m1\\ele01b.dor",
    GAME_DATA_ROOT "item_m1\\ele02.dor",
    GAME_DATA_ROOT "item_m1\\ele04.dor",
    GAME_DATA_ROOT "item_m1\\kai01.dor",
    GAME_DATA_ROOT "item_m1\\kai03.dor",
    GAME_DATA_ROOT "item_m1\\kai02.dor",
    GAME_DATA_ROOT "item_m1\\kai04.dor",
    GAME_DATA_ROOT "item_m1\\lad00.dor",
    GAME_DATA_ROOT "item_m1\\lad01.dor",
    GAME_DATA_ROOT "item_m1\\door00k.dor",
    GAME_DATA_ROOT "item_m1\\door01k.dor",
    GAME_DATA_ROOT "item_m1\\door03k.dor",
    GAME_DATA_ROOT "item_m1\\door05k.dor",
    GAME_DATA_ROOT "item_m1\\door06k.dor",
    GAME_DATA_ROOT "item_m1\\door15.dor",
};

// ============================================================================
// Door-type -> animation-sequence map (0x004d2ce0, 256 bytes). The door record
// byte +0x0A indexes this; the value selects the 12-dword phase sequence used
// by DoorPhaseCheck. Most bytes map to 0.
// ============================================================================
static const unsigned char g_doorTypeToSeq[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 0x00-0x0F
    1, 3, 4, 4, 4, 5, 6, 8, 8, 8, 8, 9, 9, 0, 0, 0,   // 0x10-0x1F
};

// Phase sequences (0x004d2d08, 11 types x 12 ints). A negative entry terminates
// the scan; when the current animation phase matches an entry,
// g_SpriteAsyncFlag is raised (in the original this tells the async file
// loader it may finish; the port's LoadFile is synchronous, so the flag is
// only cosmetic here).
static const int g_doorPhaseSeq[11][12] = {
    { 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, -1 },                            // 0
    { 0x47, 0x48, 0x49, 0x4A, 0x4B, -1 },                                        // 1
    { -1 },                                                                      // 2
    { 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, -1 },                            // 3
    { 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, -1 },                            // 4
    { 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, -1 },                            // 5
    { -1 },                                                                      // 6
    { -1 },                                                                      // 7
    { 0x35, 0x36, 0x37, 0x38, 0x39, -1 },                                        // 8
    { 0x59, 0x92, 0xB4, 0xD6, 0xFA, -1 },                                        // 9
    { 0x25, 0x54, 0x7D, 0xAB, 0xD8, -1 },                                        // 10
};

// Byte-variable table (0x004bda08) - script-visible variables, all latched by
// room_transition_load from the door record or owned by other subsystems.
//   var 0: door direction (record+0x08)  = g_nextRoomDoorType   (0x00be0bc8)
//   var 1: door sfx id (record+0x09)     = g_nextRoomSfxId      (0x00be0bc1)
//   var 2: door type (record+0x0A)       = g_nextRoom_be05b7    (0x00be05b7)
//   var 3: entry camera (record+0x0B)    = g_nextRoomCameraId   (0x00be0dd4)
//   var 4: destination room (record+0x0D)= g_nextRoomDest       (0x00be0bc0)
//   var 5: zeroed by the animation init  (0x00be05b6)
//   var 6: message id                    = g_menu_choice_id     (0x00be9825)
// Short-variable table (0x004bda24) has a single entry: 0x00be0bc8 (var 0 as
// a short).
//
// Vars 1/3/4 are NOT door-local: the table at 0x004bda08 points them straight
// at the globals room_transition_load latches from the door record. Declaring
// private statics here left them permanently 0, and var 3 is the one every
// door script uses to pick the handle model (a scan of all 34 shipped .dor
// files finds IF_BYTE reading only var 0 and var 3, var 3 in 21 of them).
// With var 3 stuck at 0 no handle sub-script ever matched.
#define DOOR_BYTEVAR0   (g_nextRoomDoorType)    // 0x00be0bc8  record+0x08
#define DOOR_BYTEVAR1   (g_nextRoomSfxId)       // 0x00be0bc1  record+0x09
#define DOOR_BYTEVAR2   (g_nextRoom_be05b7)     // 0x00be05b7  record+0x0A
#define DOOR_BYTEVAR3   (g_nextRoomCameraId)    // 0x00be0dd4  record+0x0B & 0x3F
#define DOOR_BYTEVAR4   (g_nextRoomDest)        // 0x00be0bc0  record+0x0D
#define DOOR_BYTEVAR6   (g_menu_choice_id)      // 0x00be9825
static unsigned char g_doorByteVar5;    // 0x00be05b6 - cleared by the init

// ============================================================================
// .dor data buffer (0x00aafce0). The largest .dor is ~0x131F0 bytes.
// The TMD header and TIM offsets are NOT constant across door files (door00
// uses 0x1408/0xAFD0, door03 0x140C/0xAFD4, ...), so both pointers are read
// from the file header (dword1/dword2) after loading, exactly like the
// original's relocated header fields.
// ============================================================================
#define DOOR_DATA_SIZE      0x14000
static unsigned char g_doorFileData[DOOR_DATA_SIZE];
static unsigned char* g_doorTmdHeader = NULL;   // relocated dword1 - TMD header
static unsigned char* g_doorTimPtr = NULL;      // relocated dword2 - PSX TIM

// The door's own PSX texture page (the original's 0x009207b8 region) - the
// door TIM is loaded here, separate from the room texture banks.
static unsigned char g_doorTexPage[0x1b60];
static int  g_doorTexCreated = 0;       // DAT_00920b00
static void* g_doorTexSrc = NULL;       // DAT_00920b24 - door TIM pointer

// ============================================================================
// Command entries (0x00be0c10) - 8 x 0x38. Each runs a script; the dispatch in
// the animation loop executes the opcode at *data until the handler returns 0.
// ============================================================================
#pragma pack(push, 1)
struct DoorCommandEntry {
    unsigned short status;              // +0x00 (1 = active)
    unsigned short counter;             // +0x02 (loop nesting depth)
    unsigned char* data;                // +0x04 (script pointer)
    unsigned char  ws[0x30];            // +0x08..+0x37 (retaddr stack + loop counters)
};
#pragma pack(pop)
static_assert(sizeof(DoorCommandEntry) == 0x38, "DoorCommandEntry size mismatch");

// ============================================================================
// Order entries (0x00be05c0) - 12 x 0x80. One per door panel; the animation
// loop composes each entry's ScaMatrixData hierarchy and renders the model.
// ============================================================================
struct DoorOrderEntry {
    unsigned short flags;               // +0x00 (0x2000 light, 0x4000 rotate, 0x8000 draw; low nibble = draw type)
    unsigned short modelIdx;            // +0x02
    unsigned int   drawState;           // +0x04
    void*          hierRoot;            // +0x08 -> &sca (set when flags byte 5 bit 7)
    unsigned int   unk0C[3];            // +0x0C..+0x17
    ScaMatrixData  sca;                 // +0x18 (0x50) - local/world matrices, owner links
    short          rotation[3];         // +0x68 (SVECTOR - RotMatrix input)
    short          pad68;               // +0x6E
    short          vel[3];              // +0x70 (position velocity)
    short          rotVel[3];           // +0x76 (rotation velocity, sign-extended)
    void*          modelPtr;            // +0x7C (resolved TMD vertex pointer from the anim table)
};
static_assert(sizeof(DoorOrderEntry) == 0x80, "DoorOrderEntry size mismatch");

// ============================================================================
// Door animation state block
// ============================================================================
static DoorOrderEntry   g_doorOrders[12];       // 0x00be05c0
static short            g_doorPhase;            // 0x00be0bc4 - phase counter, +1 per loop pass
static int              g_doorFrameCount;       // 0x007e0df8 - frame counter
static MATRIX           g_doorCameraMatrix;     // 0x00be0bd0 - camera from/to positions
static short            g_doorMatrixDelta[6];   // 0x00be0bf0 - camera delta buffer
static DoorCommandEntry g_doorCommands[8];      // 0x00be0c10
static unsigned int     g_doorState;            // 0x00be0dd0 - bit 0 = running, bit 1 = dispatch enabled
static unsigned char*   g_doorAnimData;         // 0x00be0c04 - &file[DOOR_TMD_OFF] + 0x0C (resolve base)
static int*             g_doorScriptTable;      // relocated script table (dword0) - read by opcode 0x08

static DoorCommandEntry* g_doorCmdCur;          // 0x00be0c00 - current command entry (dispatch walk)

// TMD slots: one per order entry, in their own region rather than carved out of
// g_tmdObjectBuffer. ObjectCleanupCallback sweeps the main buffer, and a
// stage-changing transition runs that cleanup while this animation is still
// drawing - so a door slot inside it gets destroyed mid-animation. The original
// keeps these at 0x009104c8, well clear of the buffer it sweeps at 0x00923b50.
// See the block comment in TmdRenderer.h.
//
// 16-byte aligned for the region base; individual slots inherit the stride's
// 4-byte alignment exactly as they do in g_tmdObjectBuffer and in the original.
alignas(16) unsigned char g_doorTmdSlotBuffer[TMD_DOOR_SLOT_COUNT * TMD_SLOT_STRIDE];

static_assert(TMD_DOOR_SLOT_COUNT == 12, "one door TMD slot per order entry");
static_assert(TMD_SLOT_STRIDE % 4 == 0, "TMD slot stride must keep slots 4-aligned");

static CMarniDirect3DTMD* g_doorTmdSlots[TMD_DOOR_SLOT_COUNT] = {
    (CMarniDirect3DTMD*)&g_doorTmdSlotBuffer[0  * TMD_SLOT_STRIDE],
    (CMarniDirect3DTMD*)&g_doorTmdSlotBuffer[1  * TMD_SLOT_STRIDE],
    (CMarniDirect3DTMD*)&g_doorTmdSlotBuffer[2  * TMD_SLOT_STRIDE],
    (CMarniDirect3DTMD*)&g_doorTmdSlotBuffer[3  * TMD_SLOT_STRIDE],
    (CMarniDirect3DTMD*)&g_doorTmdSlotBuffer[4  * TMD_SLOT_STRIDE],
    (CMarniDirect3DTMD*)&g_doorTmdSlotBuffer[5  * TMD_SLOT_STRIDE],
    (CMarniDirect3DTMD*)&g_doorTmdSlotBuffer[6  * TMD_SLOT_STRIDE],
    (CMarniDirect3DTMD*)&g_doorTmdSlotBuffer[7  * TMD_SLOT_STRIDE],
    (CMarniDirect3DTMD*)&g_doorTmdSlotBuffer[8  * TMD_SLOT_STRIDE],
    (CMarniDirect3DTMD*)&g_doorTmdSlotBuffer[9  * TMD_SLOT_STRIDE],
    (CMarniDirect3DTMD*)&g_doorTmdSlotBuffer[10 * TMD_SLOT_STRIDE],
    (CMarniDirect3DTMD*)&g_doorTmdSlotBuffer[11 * TMD_SLOT_STRIDE],
};

// Async-call parameter staging (DAT_008fc428 / 0x00922f00 / 0x00ac3500 /
// 0x009104c4).
static int g_doorDrawIndex;             // 0x008fc428
static void* g_doorCreateTmdBase;       // 0x00922f00[0]
static int g_doorCreateModelIdx;        // 0x00ac3500
static int g_doorCreateOrderIdx;        // 0x009104c4

// ============================================================================
// DoorPhaseCheck (0x00486890)
// Scans the door type's phase sequence for the current phase; raises
// g_SpriteAsyncFlag when found. Type 9 uses sequence (door direction + 9).
// ============================================================================
static void DoorPhaseCheck(void)
{
    int type = g_doorTypeToSeq[DOOR_BYTEVAR2];
    for (int i = 0; i < 12; i++) {
        // The terminator is read from row `type`, the value compared from row
        // `seq` - that asymmetry is the original's (0x00486890), not a slip.
        if (g_doorPhaseSeq[type][i] < 0) {
            g_SpriteAsyncFlag = 0;
            return;
        }
        int seq = type;
        if (type == 9) {
            seq = DOOR_BYTEVAR0 + 9;
        }
        // The original indexes the 11-row table at 0x004d2d08 with seq and
        // walks off the end for door direction >= 2; the port's table is a
        // real array, so clamp instead of reading whatever follows it.
        if (seq < 0 || seq > 10) {
            continue;
        }
        if (g_doorPhaseSeq[seq][i] == g_doorPhase) {
            g_SpriteAsyncFlag = 1;
            return;
        }
    }
}

// ============================================================================
// DoorDrawOrder (0x00484a30) - render the TMD slot for the order entry whose
// index was staged by DoorRequestDraw. Builds the 4x4 float matrix from the
// GTE rotation/translation buffer (same layout as the entity/item paths) and
// calls CMarniDirect3DTMD::Transform, which queues the object for the frame.
// ============================================================================
static void DoorDrawOrder(void)
{
    int idx = g_doorDrawIndex;
    if (idx < 0 || idx >= 12) return;

    // Depth = clamp(t[2] >> 6, 0, 0xffa) - the view distance already comes
    // through t[2] from the camera matrix (0x008f8904 in Ghidra).
    int depth = g_gteRotTransMatrix.t[2] >> 6;
    if (depth < 0) depth = 0;
    if (depth > 0xffa) depth = 0xffa;

    float m[16];
    float scale = 0.00024414063f; // 1/4096
    m[0]  = (float)g_gteRotTransMatrix.m[0][0] * scale;
    m[4]  = (float)g_gteRotTransMatrix.m[0][1] * scale;
    m[8]  = (float)g_gteRotTransMatrix.m[0][2] * scale;
    m[1]  = (float)g_gteRotTransMatrix.m[1][0] * scale;
    m[5]  = (float)g_gteRotTransMatrix.m[1][1] * scale;
    m[9]  = (float)g_gteRotTransMatrix.m[1][2] * scale;
    m[2]  = (float)g_gteRotTransMatrix.m[2][0] * scale;
    m[6]  = (float)g_gteRotTransMatrix.m[2][1] * scale;
    m[10] = (float)g_gteRotTransMatrix.m[2][2] * scale;
    m[12] = (float)g_gteRotTransMatrix.t[0];
    m[13] = (float)g_gteRotTransMatrix.t[1];
    m[14] = (float)g_gteRotTransMatrix.t[2];
    m[3]  = 0.0f;
    m[7]  = 0.0f;
    m[11] = 0.0f;
    m[15] = 1.0f;

    g_doorTmdSlots[idx]->Transform(g_pMarniDirect3D, (void*)(size_t)depth, m, 0);
}

// DoorRequestDraw (0x00484ba0)
static void DoorRequestDraw(int idx)
{
    g_doorDrawIndex = idx;
    ExecAsync((void*)DoorDrawOrder);
}

// ============================================================================
// DoorCreateTexturePage (0x00484830) - async: load the door TIM into the door
// texture page and create its D3D texture handles.
// ============================================================================
static void DoorCreateTexturePage(void)
{
    // obj must be the texture PAGE being repopulated, context the video driver.
    // Passing g_pMarniDirect3D as obj made VideoDriver_ClearState348 treat the
    // CMarniDirect3D object as a PSXTexture: ClearCLUTEntries then walked
    // bogus offsets and Release() freed garbage heap pointers (0xC0000005 in
    // RtlFreeHeap, release-only). Matches DoorAsyncTeardown below and every
    // other call site (GameState/MainMenu/ObjectManager).
    VideoDriver_ClearState348(g_doorTexPage, g_pMarniDirect3D);
    LoadPSXImage((PSXTexture*)g_doorTexPage, g_doorTexSrc, 1);

    // 0x00484856: overwrite every CLUT descriptor's material key with a fixed
    // constant set (0, 0x1FF, 0x140, 0x100). Every shipped .dor primitive
    // packet encodes clut position 0x7FC0 (= VRAM 0,511) and page low bits
    // 0x15, so PSXObject_Store always builds the slot key
    // (0, 0x1FF, 0x140, 0x100) - stamping the descriptors with that same key
    // makes CMarniDirect3DTMD::Create's texture match succeed no matter where
    // the TIM itself placed its CLUT. Without this the descriptors keep the
    // TIM's real coordinates and any .dor whose CLUT is NOT at VRAM (0,511)
    // fails the match: ele01a.dor / ele01b.dor place theirs at (0,480), so
    // room50c0's elevator transition created no TMD objects and drew nothing,
    // while every doorNN.dor (CLUT really at 0,511) matched by coincidence.
    {
        DWORD count = *(DWORD*)(g_doorTexPage + 0x340);
        BYTE* desc = g_doorTexPage;
        for (DWORD j = 0; j < count; j++, desc += 0x68) {
            *(DWORD*)(desc + 0x54) = 0;
            *(DWORD*)(desc + 0x58) = 0x1FF;
            *(DWORD*)(desc + 0x5C) = 0x140;
            *(DWORD*)(desc + 0x60) = 0x100;
        }
    }

    Direct3DTIM_Create(g_doorTexPage, g_pMarniDirect3D);
    g_doorTexCreated = 1;
}

// DoorRequestTexture (0x004848e0)
static void DoorRequestTexture(void* timPtr)
{
    g_doorTexSrc = timPtr;
    ExecAsync((void*)DoorCreateTexturePage);
}

// ============================================================================
// DoorLoadData (0x00412300) - load the .dor file for the door type and start
// the texture page creation. Called by room_transition_load BEFORE the
// animation task is spawned.
// ============================================================================
static void DoorLoadData(void)
{
    unsigned int type = DOOR_BYTEVAR2;
    if (type >= 0x22) type = 0;         // guard: the original indexes without a bound
    LoadFile(g_doorFileNameTable[type], g_doorFileData, 0x20);

    // TMD header / TIM pointers come from the file header (dword1/dword2) -
    // they differ per door file.
    unsigned int* hdr = (unsigned int*)g_doorFileData;
    g_doorTmdHeader = g_doorFileData + hdr[1];
    g_doorTimPtr    = g_doorFileData + hdr[2];

    g_TextureBankID = 0x15;             // original writes word 0x1f15; bank 0x15 is the meaningful byte
    DoorRequestTexture((void*)g_doorTimPtr);
}

// ============================================================================
// DoorAsyncCreateTmd (0x00484900) - create the TMD object for one order entry
// from the door's TMD header. PSXObject_Store parses object `modelIdx` of the
// header (12 door parts); Direct3DTMD_Create builds the D3D object from the
// parsed viewport elements using the door texture page.
// ============================================================================
static void DoorAsyncCreateTmd(void)
{
    int modelIdx = g_doorCreateModelIdx;
    int orderIdx = g_doorCreateOrderIdx;
    if (orderIdx < 0 || orderIdx >= 12) return;
    if (modelIdx > 0xc) return;

    CMarniDirect3DTMD* slot = g_doorTmdSlots[orderIdx];
    slot->CleanupObjects(g_pMarniDirect3D);

    // FUN_004450e0: tmdHdr = the door TMD header (staged by the order-setup
    // opcode), objIndex = modelIdx, page 0x15 (the tpage value carried by the
    // door primitives), UV divisor 0x80 = 128.
    int st = PSXObject_Store(slot, (int*)g_doorCreateTmdBase, modelIdx, 0x15, 0x80);

    // Object-creation flag: 1 when the door type maps to sequence 0, else 2
    // (FUN_00486910 is exactly `g_doorTypeToSeq[byteVar2] == 0`).
    int db = (g_doorTypeToSeq[DOOR_BYTEVAR2] == 0) ? 1 : 2;
    int cr = slot->Create(g_pMarniDirect3D, g_doorTexPage, (void*)(size_t)db);

    // 0x0048497c: mark every per-object entry UNLIT. Bit 2 of the entry's
    // +0x80 word is what the original's renderer tests at 0x00446e99 and
    // 0x00447043: when set it skips both the light-direction transform and the
    // per-vertex light accumulation and writes the vertex colour straight into
    // the primitive. The door animation is deliberately full-bright - without
    // this it gets shaded by whatever g_d3dLightData the room you just left
    // happened to leave behind, so the same door renders darker on the way
    // back than on the way in.
    //
    // Create() zeroes m_objectData, so this has to run after it. The original
    // walks two entries per iteration (0x108 stride, +0 and +0x84), i.e. it
    // flags 2*objectCount entries; with the 16+16 entry array that stays in
    // bounds for any count up to 16, but clamp anyway.
    {
        DWORD count = slot->m_objectCount;
        DWORD entries = count * 2;
        if (entries > 32) entries = 32;
        for (DWORD k = 0; k < entries; k++) {
            *(DWORD*)((BYTE*)slot + 0x550 + k * 0x84) |= 2;
        }
    }

    // A failure here renders nothing at all, so keep it audible rather than
    // silent: Store rejects the model, or Create fails to match the element's
    // material against the door texture page (slot+0x38..0x44 vs page+0x54..0x60).
    if (st == 0 || cr == 0 || slot->m_initialized == 0) {
        DWORD* elem = (DWORD*)((BYTE*)slot + 0x38);
        DWORD* te   = (DWORD*)((BYTE*)g_doorTexPage + 0x54);
        dbg_printf("[door] TMD create FAILED: order=%d model=%d store=%d create=%d "
                   "init=%d count=%d texCount=%d elem(%08X %08X %08X %08X) "
                   "page(%08X %08X %08X %08X)\n",
                   orderIdx, modelIdx, st, cr, (int)slot->m_initialized,
                   (int)slot->m_objectCount,
                   (int)*(DWORD*)((BYTE*)g_doorTexPage + 0x340),
                   elem[0], elem[1], elem[2], elem[3], te[0], te[1], te[2], te[3]);
    }
}

// DoorRequestTmdCreate (0x004849b0)
static void DoorRequestTmdCreate(void* tmdBase, int modelIdx, int orderIdx)
{
    g_doorCreateTmdBase = tmdBase;
    g_doorCreateModelIdx = modelIdx;
    g_doorCreateOrderIdx = orderIdx;
    ExecAsync((void*)DoorAsyncCreateTmd);
}

// ============================================================================
// DoorAnimInit (0x004443c0)
// ============================================================================
static void DoorAnimInit(void)
{
    g_imageBufferPtr = (void*)0;            // original swaps to dedicated buffers (0x00ac592c)
    g_imageBufferPtr2 = (void*)0;           // (0x00acd710); no port code reads them, see note below
    g_bGameActive = 0;
    g_doorByteVar5 = 0;
    g_main_state_flags |= MSF_ROOM_TRANSITION;        // "door animation running" - polled by room_transition_load

    g_doorState = 3;                        // bit 0 running, bit 1 dispatch enabled

    // Zero the order table, camera matrix and command table blocks.
    memset(g_doorOrders, 0, sizeof(g_doorOrders));
    memset(&g_doorCameraMatrix, 0, sizeof(g_doorCameraMatrix));
    memset(g_doorMatrixDelta, 0, sizeof(g_doorMatrixDelta));
    memset(g_doorCommands, 0, sizeof(g_doorCommands));

    // Relocate the .dor header pointers: dword0 (script table), dword1 (TMD
    // header), dword2 (TIM) all gain the buffer base.
    unsigned int* hdr = (unsigned int*)g_doorFileData;
    hdr[0] += (unsigned int)g_doorFileData;
    hdr[1] += (unsigned int)g_doorFileData;
    hdr[2] += (unsigned int)g_doorFileData;

    // ResolveAnimPointers on the TMD header: flag byte at +0, count at +4,
    // 7-dword per-object entries at +8 (vert/norm/prim pointers are
    // header-relative offsets; resolution makes them absolute in place).
    g_doorAnimData = (unsigned char*)hdr[1] + 4;
    ResolveAnimPointers(g_doorAnimData);
    g_doorAnimData += 8;

    // Relocate the script table (NULL-terminated, offsets from the table base
    // = relocated dword0).
    g_doorScriptTable = (int*)hdr[0];
    int* table = g_doorScriptTable;
    while (*table != 0) {
        *table += (int)hdr[0];
        table++;
    }

    // Command entry 0 is the door's main script: status = 1, data = the first
    // script table entry (the main script, which branches on the door
    // direction and activates entries 1/2 with the phase scripts).
    g_doorCommands[0].status = 1;
    g_doorCommands[0].counter = 0;
    g_doorCommands[0].data = (unsigned char*)g_doorScriptTable[0];

    Task_sleep(1);
}

// ============================================================================
// The 38 script opcodes (0x00443950..0x004443a0). Each returns nonzero to keep
// dispatching the current command entry, 0 to yield until the next frame.
// ============================================================================
#define DATA g_doorCmdCur->data

// 0x00: terminate the whole animation.
//
// Script 0 of every .dor is a chain of "IF_BYTE(var0 == N) -> skip; ACTIVATE...;
// CLEAR_SELF" arms with a bare END after the last one, so reaching END on the
// first frame means the door direction has no arm in this file and nothing will
// ever be drawn. The door files cover var0 0..12; the stairs, ladder, elevator
// and mon files only cover 0 and 1.
static int door_op_end(void)        { g_doorState = 0; return 0; }

// 0x01: clear this command entry and stop
static int door_op_clear(void)
{
    DoorCommandEntry* e = g_doorCmdCur;
    e->status = 0;
    e->counter = 0;
    e->data = NULL;
    *(unsigned int*)((unsigned char*)e + 8) = 0;
    *(unsigned short*)((unsigned char*)e + 0x28) = 0;
    return 0;
}

// 0x02: yield until the given command entry is inactive
static int door_op_wait_free(void)
{
    if (g_doorCommands[DATA[1]].status != 0) return 0;
    DATA += 2;
    return 1;
}

// 0x03: advance 2 bytes, yield
static int door_op_yield2(void)     { DATA += 2; return 0; }

// 0x04: byte-var compare. The BRANCH IS TAKEN WHEN THE COMPARISON FAILS
// (0x004439e0: every satisfied case jumps straight to the `data += 6` tail,
// and only the fall-out path does `data = data + operand` first). The scripts
// are written around that: script 0 of door00.dor is a chain of
//     IF_BYTE(var0 == 6) -> skip;  ACTIVATE double-door scripts;  CLEAR_SELF
// so with the sense reversed EVERY door (var0 != 6) activated the double-door
// branch, which is why single doors rendered as a pair of leaves.
// The operands are signed chars in the original.
static int door_op_jmp_byte(void)
{
    unsigned char* p = DATA;
    signed char* var;
    switch (p[2]) {
    case 1: var = (signed char*)&DOOR_BYTEVAR1; break;
    case 2: var = (signed char*)&DOOR_BYTEVAR2; break;
    case 3: var = (signed char*)&DOOR_BYTEVAR3; break;
    case 4: var = (signed char*)&DOOR_BYTEVAR4; break;
    case 5: var = (signed char*)&g_doorByteVar5; break;
    case 6: var = (signed char*)&DOOR_BYTEVAR6; break;
    case 0:
    default: var = (signed char*)&DOOR_BYTEVAR0; break;
    }
    signed char v = (signed char)p[4];
    unsigned char relop = p[3];
    bool taken;                             // true = branch, false = fall through
    switch (relop) {
    case 0: taken = !(*var == v); break;
    case 1: taken = !(*var >  v); break;
    case 2: taken = !(*var >= v); break;
    case 3: taken = !(*var <  v); break;
    case 4: taken = !(*var <= v); break;
    case 5: taken = !(*var != v); break;
    default: taken = true; break;           // relop > 5 = unconditional jump
    }
    if (taken) DATA = p + 6 + p[1];
    else       DATA = p + 6;
    return 1;
}

// 0x05: short-var compare (single short var). The original's shortVar[0] is
// the WORD at 0x00be0bc8 (the door direction byte plus its neighbour); the
// shipped door scripts only use the short ops as unconditional jumps (relop
// > 5), so the port keeps the short var as its own word - aliasing the byte
// global would read past it. Branch sense as in 0x04: taken when the
// comparison FAILS (0x00443a60). No shipped .dor uses this opcode at all, so
// only the unconditional (relop > 5) form ever mattered.
static short g_doorShortVar0;
static int door_op_jmp_short(void)
{
    unsigned char* p = DATA;
    short v = *(short*)(p + 4);
    unsigned char relop = p[3];
    short var = g_doorShortVar0;
    bool taken;
    switch (relop) {
    case 0: taken = !(var == v); break;
    case 1: taken = !(var >  v); break;
    case 2: taken = !(var >= v); break;
    case 3: taken = !(var <  v); break;
    case 4: taken = !(var <= v); break;
    case 5: taken = !(var != v); break;
    default: taken = true; break;
    }
    if (taken) DATA = p + 6 + p[1];
    else       DATA = p + 6;
    return 1;
}

// 0x06: loop push - store return address and loop count into the workspace
static int door_op_loop_push(void)
{
    DoorCommandEntry* e = g_doorCmdCur;
    unsigned char* p = DATA;
    int n = e->counter;
    *(int*)((unsigned char*)e + 8 + n * 4) = (int)(p + 4);
    *(unsigned short*)((unsigned char*)e + 0x28 + n * 2) = *(unsigned short*)(p + 2);
    e->counter = (unsigned short)(n + 1);
    DATA = p + 4;
    return 1;
}

// 0x07: loop - decrement the top loop counter; when exhausted pop back to the
// instruction after the loop push, else jump to the stored return address and
// yield (the body re-executes next frame).
static int door_op_loop(void)
{
    DoorCommandEntry* e = g_doorCmdCur;
    unsigned char* p = DATA;
    int n = e->counter;
    unsigned short* ctr = (unsigned short*)((unsigned char*)e + 0x26 + n * 2);
    *ctr = *ctr - 1;
    if (*ctr < 1) {
        e->counter = (unsigned short)(n - 1);
        DATA = p + 2;
        return 1;
    }
    DATA = (unsigned char*)*(int*)((unsigned char*)e + 4 + n * 4);
    return 0;
}

// 0x08: activate another command entry with a script from the script table.
// Operand: word at data+2 - low byte = target entry, bits 6+ (dword-aligned) =
// byte offset into the relocated script table. Sets status = 1.
static int door_op_activate(void)
{
    unsigned short w = *(unsigned short*)(DATA + 2);
    int idx = w & 0xFF;
    int tableOff = (w >> 6) & ~3;
    DoorCommandEntry* e = &g_doorCommands[idx];
    e->status = 1;
    e->counter = 0;
    e->data = (unsigned char*)*(int*)((unsigned char*)g_doorScriptTable + tableOff);
    *(unsigned short*)((unsigned char*)e + 0x28) = 0;
    DATA += 4;
    return 1;
}

// 0x09: clear another command entry (target = the byte after the opcode)
static int door_op_clear_entry(void)
{
    int idx = DATA[1];
    DoorCommandEntry* e = &g_doorCommands[idx];
    e->status = 0;
    e->counter = 0;
    e->data = NULL;
    *(unsigned int*)((unsigned char*)e + 8) = 0;
    *(unsigned short*)((unsigned char*)e + 0x28) = 0;
    DATA += 2;
    return 1;
}

// 0x0A / 0x0B: byte var set / add
static int door_op_bytevar_set(void)
{
    unsigned char* p = DATA;
    switch (p[1]) {
    case 0: DOOR_BYTEVAR0 = p[2]; break;
    case 1: DOOR_BYTEVAR1 = p[2]; break;
    case 2: DOOR_BYTEVAR2 = p[2]; break;
    case 3: DOOR_BYTEVAR3 = p[2]; break;
    case 4: DOOR_BYTEVAR4 = p[2]; break;
    case 5: g_doorByteVar5 = p[2]; break;
    case 6: DOOR_BYTEVAR6 = p[2]; break;
    }
    DATA += 4;
    return 1;
}
static int door_op_bytevar_add(void)
{
    unsigned char* p = DATA;
    signed char d = (signed char)p[2];
    switch (p[1]) {
    case 0: DOOR_BYTEVAR0 = (unsigned char)(DOOR_BYTEVAR0 + d); break;
    case 1: DOOR_BYTEVAR1 = (unsigned char)(DOOR_BYTEVAR1 + d); break;
    case 2: DOOR_BYTEVAR2 = (unsigned char)(DOOR_BYTEVAR2 + d); break;
    case 3: DOOR_BYTEVAR3 = (unsigned char)(DOOR_BYTEVAR3 + d); break;
    case 4: DOOR_BYTEVAR4 = (unsigned char)(DOOR_BYTEVAR4 + d); break;
    case 5: g_doorByteVar5 = (unsigned char)(g_doorByteVar5 + d); break;
    case 6: DOOR_BYTEVAR6 = (unsigned char)(DOOR_BYTEVAR6 + d); break;
    }
    DATA += 4;
    return 1;
}

// 0x0C / 0x0D: short var set / add (var 0 only)
static int door_op_shortvar_set(void)
{
    g_doorShortVar0 = *(short*)(DATA + 2);
    DATA += 4;
    return 1;
}
static int door_op_shortvar_add(void)
{
    g_doorShortVar0 = (short)(g_doorShortVar0 + *(short*)(DATA + 2));
    DATA += 4;
    return 1;
}

// 0x0E / 0x0F: fade in / out (g_fading_counter = +/- w2*0x80)
static int door_op_fade_in(void)
{
    unsigned char* p = DATA;
    g_fade_type_id = p[1];
    g_fading_counter = *(short*)(p + 2) * -0x80;
    g_fading_state = *(unsigned short*)(p + 4);
    DATA += 6;
    return 1;
}
static int door_op_fade_out(void)
{
    unsigned char* p = DATA;
    g_fade_type_id = p[1];
    g_fading_counter = *(short*)(p + 2) << 7;
    g_fading_state = *(unsigned short*)(p + 4);
    DATA += 6;
    return 1;
}

// 0x10: order entry setup - hierarchy + model reference
static int door_op_order_setup(void)
{
    unsigned char* p = DATA;
    int idx = p[1];
    DoorOrderEntry* e = &g_doorOrders[idx];

    ScaMatrixData* owner = NULL;
    if (p[3] != 0xFF) {
        owner = &g_doorOrders[p[3]].sca;
    }
    InitScaMatrix((int)(size_t)owner, &e->sca);

    if ((p[5] & 0x80) != 0) {
        DoorRequestTmdCreate(g_doorAnimData - 0xC, p[2], p[1]);
        e->hierRoot = &e->sca;
        e->drawState = 0;
        // The original tests flag bit 0x2000 (byte+1 & 0x20) here - before the
        // flags word is written below, so it reads the previous (zeroed) value
        // and 0x40 is what lands in practice.
        if ((((unsigned char*)&e->flags)[1] & 0x20) == 0) {
            e->drawState = 0x40;
        }
    }

    // The resolved per-object vertex pointer from the TMD header (entry p[2]).
    unsigned int* tmdHdr = (unsigned int*)g_doorTmdHeader;
    e->modelPtr = (void*)tmdHdr[p[2] * 7 + 3];
    e->flags = *(unsigned short*)(p + 4);
    e->modelIdx = p[2];
    DATA += 6;
    return 1;
}

// 0x11: camera matrix from 6 shorts << (p[1] & 0x1f)
static int door_op_cam_matrix(void)
{
    unsigned char* p = DATA;
    set_scene_render_param(0x101);      // 0x00443e43 - title/render mode for the door scene
    int shift = p[1] & 0x1f;
    int* dst = (int*)&g_doorCameraMatrix;
    for (int i = 0; i < 6; i++) {
        dst[i] = (int)*(short*)(p + 2 + i * 2) << shift;
    }
    // t[1], t[2] zeroed (the block is 8 dwords)
    dst[6] = 0;
    dst[7] = 0;
    DATA += 0xE;
    return 1;
}

// 0x12: delta buffer from 6 shorts
static int door_op_delta_load(void)
{
    unsigned char* p = DATA;
    for (int i = 0; i < 6; i++) {
        g_doorMatrixDelta[i] = *(short*)(p + 2 + i * 2);
    }
    DATA += 0xE;
    return 1;
}

// 0x13: camera matrix += delta buffer (6 dwords, delta as signed shorts)
static int door_op_matrix_add_delta(void)
{
    int* dst = (int*)&g_doorCameraMatrix;
    for (int i = 0; i < 6; i++) {
        dst[i] = dst[i] + (int)g_doorMatrixDelta[i];
    }
    DATA += 2;
    return 1;
}

// 0x14: delta[i] += (signed char)j
static int door_op_delta_add(void)
{
    unsigned char* p = DATA;
    g_doorMatrixDelta[p[2]] = (short)(g_doorMatrixDelta[p[2]] + (signed char)p[3]);
    DATA += 4;
    return 1;
}

// 0x15: order position = 3 shorts (into sca.localMatrix.t)
static int door_op_order_pos(void)
{
    unsigned char* p = DATA;
    DoorOrderEntry* e = &g_doorOrders[p[1]];
    e->sca.localMatrix.t[0] = (int)*(short*)(p + 2);
    e->sca.localMatrix.t[1] = (int)*(short*)(p + 4);
    e->sca.localMatrix.t[2] = (int)*(short*)(p + 6);
    DATA += 8;
    return 1;
}

// 0x16: order velocity = 3 shorts
static int door_op_order_vel(void)
{
    unsigned char* p = DATA;
    DoorOrderEntry* e = &g_doorOrders[p[1]];
    e->vel[0] = *(short*)(p + 2);
    e->vel[1] = *(short*)(p + 4);
    e->vel[2] = *(short*)(p + 6);
    DATA += 8;
    return 1;
}

// 0x17: order position += velocity
static int door_op_order_pos_add_vel(void)
{
    DoorOrderEntry* e = &g_doorOrders[DATA[1]];
    e->sca.localMatrix.t[0] += (int)e->vel[0];
    e->sca.localMatrix.t[1] += (int)e->vel[1];
    e->sca.localMatrix.t[2] += (int)e->vel[2];
    DATA += 2;
    return 1;
}

// 0x18: order velocity[sub] += (signed char)j
static int door_op_order_vel_add(void)
{
    unsigned char* p = DATA;
    DoorOrderEntry* e = &g_doorOrders[p[1]];
    e->vel[p[2]] = (short)(e->vel[p[2]] + (signed char)p[3]);
    DATA += 4;
    return 1;
}

// 0x19: order rotation = 3 shorts
static int door_op_order_rot(void)
{
    unsigned char* p = DATA;
    DoorOrderEntry* e = &g_doorOrders[p[1]];
    e->rotation[0] = *(short*)(p + 2);
    e->rotation[1] = *(short*)(p + 4);
    e->rotation[2] = *(short*)(p + 6);
    DATA += 8;
    return 1;
}

// 0x1A: order rotation velocity = 3 signed chars (sign-extended to words)
static int door_op_order_rotvel(void)
{
    unsigned char* p = DATA;
    DoorOrderEntry* e = &g_doorOrders[p[1]];
    e->rotVel[0] = (signed char)p[2];
    e->rotVel[1] = (signed char)p[3];
    e->rotVel[2] = (signed char)p[4];
    DATA += 6;
    return 1;
}

// 0x1B: order rotation += rotation velocity
static int door_op_order_rot_add_vel(void)
{
    DoorOrderEntry* e = &g_doorOrders[DATA[1]];
    e->rotation[0] = (short)(e->rotation[0] + e->rotVel[0]);
    e->rotation[1] = (short)(e->rotation[1] + e->rotVel[1]);
    e->rotation[2] = (short)(e->rotation[2] + e->rotVel[2]);
    DATA += 2;
    return 1;
}

// 0x1C: order rotation velocity[sub] += (signed char)j
static int door_op_order_rotvel_add(void)
{
    unsigned char* p = DATA;
    DoorOrderEntry* e = &g_doorOrders[p[1]];
    e->rotVel[p[2]] = (short)(e->rotVel[p[2]] + (signed char)p[3]);
    DATA += 4;
    return 1;
}

// 0x1D: order vertex write (3 shorts at entry+0x7C + sub*8)
static int door_op_order_vert_set(void)
{
    unsigned char* p = DATA;
    DoorOrderEntry* e = &g_doorOrders[p[1]];
    short* dst = (short*)((unsigned char*)e + 0x7C + p[2] * 8);
    dst[0] = *(short*)(p + 4);
    dst[1] = *(short*)(p + 6);
    dst[2] = *(short*)(p + 8);
    DATA += 10;
    return 1;
}

// 0x1E: order vertex add (3 shorts at [modelPtr] + sub*8)
static int door_op_order_vert_add(void)
{
    unsigned char* p = DATA;
    DoorOrderEntry* e = &g_doorOrders[p[1]];
    short* dst = (short*)((char*)e->modelPtr + p[2] * 8);
    dst[0] = (short)(dst[0] + *(short*)(p + 4));
    dst[1] = (short)(dst[1] + *(short*)(p + 6));
    dst[2] = (short)(dst[2] + *(short*)(p + 8));
    DATA += 10;
    return 1;
}

// 0x1F: set_message_display
static int door_op_message(void)
{
    unsigned char* p = DATA;
    set_message_display(p[1], *(unsigned short*)(p + 2));
    DATA += 4;
    return 1;
}

// 0x20: play_sfx - waits for the BGM-ready flag (flags2 bit 0x800000) to clear
static int door_op_sfx(void)
{
    if ((g_main_state_flags2 & MSF2_SND_BUSY) != 0) {
        g_doorState &= ~2u;             // suspend dispatch until the flag clears
        return 0;
    }
    unsigned char* p = DATA;
    play_sfx(p[1], p[2], p[3]);
    DATA += 4;
    return 1;
}

// 0x21: order flags set (only while the order is active)
static int door_op_order_flags(void)
{
    unsigned char* p = DATA;
    DoorOrderEntry* e = &g_doorOrders[p[1]];
    if (e->flags != 0) {
        e->flags = *(unsigned short*)(p + 2);
    }
    DATA += 4;
    return 1;
}

// 0x22: order texture page (order = word & 0xff, tpage = word >> 8)
// The original walks the order's TMD object data (pointers at entry+0x0C and
// entry+0x10) and ORs the tpage into each primitive's CLUT word. The port's
// DX11 TMD path binds textures at PSXObject_Store time and has no per-frame
// CLUT words, so the write is a no-op here; entry+0x0C/+0x10 are never set by
// anything else in the door system.
static int door_op_order_tpage(void)
{
    DATA += 4;
    return 1;
}

// 0x23: image buffer advance (no-op in the port - the DX11 path has no
// software framebuffer). The original returns word >> 1 as the continue flag.
static int door_op_buffer_advance(void)
{
    int cont = *(unsigned short*)(DATA + 2) >> 1;
    DATA += 4;
    return cont;
}

// 0x24: disable dispatch (clears state bit 1)
static int door_op_disable_dispatch(void)
{
    g_doorState &= ~2u;
    DATA += 2;
    return 0;
}

// 0x25: clear flags2 bit 0x800000 (BGM-ready wait flag)
static int door_op_clear_flags2(void)
{
    g_main_state_flags2 &= ~MSF2_SND_BUSY;
    DATA += 2;
    return 1;
}

typedef int (*DoorOpFn)(void);
// Original opcode handlers (Ghidra door_phase_00..37, in table order):
//   0x00443950 0x00443960 0x00443990 0x004439d0 0x004439e0 0x00443a60
//   0x00443af0 0x00443b40 0x00443b80 0x00443bf0 0x00443c40 0x00443c70
//   0x00443ca0 0x00443cd0 0x00443d00 0x00443d40 0x00443d80 0x00443e40
//   0x00443eb0 0x00443ef0 0x00443f30 0x00443f60 0x00443fa0 0x00443fe0
//   0x00444020 0x00444060 0x004440a0 0x004440f0 0x00444140 0x00444180
//   0x004441d0 0x00444220 0x00444250 0x004442a0 0x004442d0 0x00444340
//   0x00444380 0x004443a0
static DoorOpFn const g_doorOps[38] = {
    door_op_end, door_op_clear, door_op_wait_free, door_op_yield2,
    door_op_jmp_byte, door_op_jmp_short, door_op_loop_push, door_op_loop,
    door_op_activate, door_op_clear_entry, door_op_bytevar_set,
    door_op_bytevar_add, door_op_shortvar_set, door_op_shortvar_add,
    door_op_fade_in, door_op_fade_out, door_op_order_setup, door_op_cam_matrix,
    door_op_delta_load, door_op_matrix_add_delta, door_op_delta_add,
    door_op_order_pos, door_op_order_vel, door_op_order_pos_add_vel,
    door_op_order_vel_add, door_op_order_rot, door_op_order_rotvel,
    door_op_order_rot_add_vel, door_op_order_rotvel_add, door_op_order_vert_set,
    door_op_order_vert_add, door_op_message, door_op_sfx, door_op_order_flags,
    door_op_order_tpage, door_op_buffer_advance, door_op_disable_dispatch,
    door_op_clear_flags2,
};

// ============================================================================
// DoorComposeChain (0x00483580) - compose the order's ScaMatrixData owner chain
// into the output matrix (same function the item viewer uses). The chain is
// linked through ScaMatrixData.owner (+0x48, parent) with each parent's child
// back-ref at field_4c (+0x4C). Walks to the root via owners, then composes
// local -> world from the root down.
// ============================================================================
static void DoorComposeChain(ScaMatrixData* node, MATRIX* out)
{
    ScaMatrixData* chain[0x14];
    int count = 0;
    ScaMatrixData* p = node;
    while (p != 0 && count < 0x14) {
        chain[count++] = p;
        p = (ScaMatrixData*)(size_t)p->owner;
    }

    for (int i = count - 1; i >= 0; i--) {
        ScaMatrixData* j = chain[i];
        if (j->owner == 0) {
            // Root: world = local (8 dwords; the original's loop copies
            // node[1..8] to node[9..16]).
            for (int k = 0; k < 8; k++) {
                ((int*)&j->worldMatrix)[k] = ((int*)&j->localMatrix)[k];
            }
        } else {
            CompMatrix(&((ScaMatrixData*)(size_t)j->owner)->worldMatrix,
                       &j->localMatrix, &j->worldMatrix);
        }
    }
    CompMatrix(&g_RoomCameraData, &node->worldMatrix, out);
}

// ============================================================================
// DoorAnimLoop (0x00444540) - the animation main loop
// ============================================================================
static void DoorAnimLoop(void)
{
    g_doorPhase = 0;
    g_doorFrameCount = 0;
    int holdCounter = 0;

    while ((g_doorState & 1) != 0) {

        // Black out the screen for the first 3 frames (the door data is
        // loading - the room is black behind the animation).
        if (g_doorFrameCount >= 0 && g_doorFrameCount < 3) {
            g_rect.textureId = 0;
            g_rect.r = 0;
            g_rect.g = 0;
            g_rect.b = 0;
            g_rect.x = -160;
            g_rect.y = -120;
            g_rect.w = 320;
            g_rect.h = 240;
            draw_rect(&g_rect, 0, 0);
        }

        DoorPhaseCheck();

        // Dispatch the command-entry scripts. The gate (state bit 1) is
        // re-opened once g_main_state_flags2 bit 0x800000 clears (BGM ready).
        if ((g_doorState & 2) != 0) {
            g_doorCmdCur = &g_doorCommands[0];
            while (g_doorCmdCur < &g_doorCommands[8]) {
                if (g_doorCmdCur->status != 0 && g_doorCmdCur->data != NULL) {
                    unsigned char op;
                    do {
                        op = *(unsigned char*)g_doorCmdCur->data;
                        if (op >= 38) {         // safety: never in shipped data
                            g_doorCmdCur->status = 0;
                            break;
                        }
                    } while (g_doorOps[op]() != 0);
                }
                g_doorCmdCur++;
            }
        } else if ((g_main_state_flags2 & MSF2_SND_BUSY) == 0) {
            g_doorState |= 2;
        }

        // Render the order entries.
        MatrixToCamera(&g_doorCameraMatrix);

        for (int i = 0; i < 12; i++) {
            DoorOrderEntry* e = &g_doorOrders[i];

            // Flag bits 0x2000/0x4000/0x8000 live in the flags word's high
            // byte (byte +1), exactly as the original's TEST byte [EBP+1] does.
            if ((((unsigned char*)&e->flags)[1] & 0x40) != 0) {     // rotate (0x4000)
                RotMatrix((SVECTOR*)&e->rotation, &e->sca.localMatrix);
                *(int*)&e->sca.field_00 = 0;
            }
            if ((((unsigned char*)&e->flags)[1] & 0x80) != 0) {     // draw (0x8000)
                MATRIX out;
                if (e->hierRoot != NULL) {
                    DoorComposeChain((ScaMatrixData*)e->hierRoot, &out);
                } else {
                    memset(&out, 0, sizeof(out));
                    out.t[2] = g_doorCameraMatrix.t[2];
                }
                if ((((unsigned char*)&e->flags)[1] & 0x20) != 0) { // light (0x2000)
                    // multAndSetLightMatrix (0x0040ad70) - same port pattern
                    // as the item viewer: light * out -> light matrix.
                    MATRIX lightM;
                    MulMatrix0(&g_lightMatrix, &out, &lightM);
                    SetLightMatrix(&lightM);
                }
                SetRotAndTransMatrix(&out);

                unsigned int state = e->drawState & 0xFFFFF1FFu;
                e->drawState = state | ((unsigned int)(e->flags & 0xF0) << 5);

                int type = e->flags & 0xF;
                if (type == 1 || type == 2 || type == 3) {
                    DoorRequestDraw(i);
                }
            }
        }

        // Holding any d-pad/circle button skips the animation (after frame 10).
        if (((g_PlayerPadHeld & 0xC0) != 0) && (g_doorFrameCount > 10)) {
            g_SpriteAsyncFlag = 1;
            g_doorState = 0;
        }

        Task_sleep(1);

        // Phase 2 holds for up to 5 frames (the "door fully open" beat).
        if (g_doorPhase == 2 && holdCounter < 5) {
            holdCounter++;
            continue;
        }
        holdCounter = 0;
        g_doorPhase++;
        g_doorFrameCount++;
    }
}

// ============================================================================
// DoorAsyncTeardown (0x004849e0) - clear the door texture page and destroy the
// 12 door TMD slots.
// ============================================================================
static void DoorAsyncTeardown(void)
{
    VideoDriver_ClearState348(g_doorTexPage, g_pMarniDirect3D);
    for (int i = 0; i < 12; i++) {
        g_doorTmdSlots[i]->CleanupObjects(g_pMarniDirect3D);
    }
}

// ============================================================================
// DoorAnimTeardown (0x00444500)
// NOTE: in the original the ExecAsync goes through a one-line wrapper at
// 0x00484a20 (ExecAsync(&DoorAsyncTeardown)); the port calls the async body
// from here directly.
// ============================================================================
static void DoorAnimTeardown(void)
{
    ExecAsync((void*)DoorAsyncTeardown);
    g_main_state_flags2 &= ~MSF2_SND_BUSY;
    g_fading_state = (short)0xFFFF;     // fade inactive (0x00444500)
    g_bGameActive = 2;
    g_imageBufferPtr = g_imageBufferDataA;
    g_imageBufferPtr2 = g_imageBufferDataB;
    g_main_state_flags ^= MSF_ROOM_TRANSITION;    // clear the "animation running" bit
}

// ============================================================================
// DoorAnimTask (0x00444770) - the door animation task, spawned as task 1 by
// room_transition_load.
// ============================================================================
static void DoorAnimTask(void)
{
    DoorAnimInit();
    DoorAnimLoop();
    DoorAnimTeardown();
    Task_exit();
}

// ============================================================================
// Public entry points (called from room_transition_load)
// ============================================================================

// FUN_00412300 - load the door data file and kick off the texture creation.
// Called BEFORE the task spawn so the animation starts with data ready.
void door_system_load_data(void)
{
    crashlog_mark("door: door_system_load_data");
    DoorLoadData();
}

// Spawn the door animation task (FUN_00444770). The caller then Task_sleep(1)
// and proceeds with the room load; the anim runs concurrently and
// room_transition_load waits for bit 0x4000000 to clear.
void door_system_start_animation(void)
{
    crashlog_mark("door: door_system_start_animation");
    Task_execute(1, (void*)DoorAnimTask);
}

// ============================================================================
// The door-driven room/stage transition, moved here from GameState.cpp.
//
// Helpers defined in other files
// ============================================================================
extern void object_delete_00442170(int category);   // 0x00442170 ObjectManager.cpp
extern void SndCompactAsync(void);                  // MarniSound.cpp
extern void load_room_sfx(unsigned char);           // SoundSystem.cpp
extern void room_set(void);                         // RoomInit.cpp
extern void init_room(void);                        // RoomInit.cpp

// 0x00442180 - empty in the original. Kept as a named call so the shape of
// room_transition_load still matches the disassembly.
static void FUN_00442180(void) { }

// ============================================================================
// BuildEnemySnap (0x0048f1a0) - tear the outgoing room's enemy list down and
// snapshot what should survive into g_savedEnemyStates.
//
// This was a report-only stub, and the missing half of it is the destructive
// one: the ORIGINAL clears status_flags on every entity slot and drains
// g_enemy_count to zero before the destination room loads. Without that, the
// outgoing room's enemies stay flagged active, and update_entities keeps
// dispatching them in the NEW room against data the load has already
// overwritten. Plant 42 makes that fatal rather than merely wrong: its state-0
// handler ALLOCATES (two entity clones plus their animation objects) out of
// g_loadDataDestPointer, so a stale Plant 42 re-initialising after the load
// writes straight through the freshly loaded room - observed leaving room 40C0
// as a missing player model, cameras cycling, and entities reporting positions
// outside every room zone.
//
// FUN_0048f330 (the restore side, called from cmd_enemy_set) was already ported,
// so until now it scanned a table nothing ever filled.
//
// `valid` is a TTL, not a boolean: set to 5 here, aged by one on each room
// change, and treated as "occupied" by the restore while non-zero.
//
// Two bounds are the port's, not the original's. The original's free-slot scan
// walks off the end of the 16-entry table when more than 16 enemies need
// saving, and its clear loop runs one entity past g_EnemiesList[29] into the
// 0x40 gap that precedes the table in the original's .bss. Neither is safe to
// reproduce here (see the .bss adjacency note in the docs), so both are clamped.
// ============================================================================
static void BuildEnemySnap(void)
{
    const unsigned char* record = (const unsigned char*)g_pendingDoorRecord;

    // Age every snapshot when the destination is a different room than the one
    // we last came from. g_AttractMode_RoomCameraId still holds the previous
    // room here; the caller overwrites it right after this returns.
    if (record != nullptr && (char)record[0x0D] != (char)g_AttractMode_RoomCameraId) {
        for (int i = 0; i < 16; i++) {
            if (g_savedEnemyStates[i].valid != 0) {
                g_savedEnemyStates[i].valid--;
            }
        }
    }

    ENTITY = g_EnemiesList;
    g_pSavedEnemyState = g_savedEnemyStates;

    while (g_enemy_count != 0) {
        // Advance to the first free slot. Bounded, unlike the original.
        int slotIdx = (int)(g_pSavedEnemyState - g_savedEnemyStates);
        while (slotIdx < 16 && g_savedEnemyStates[slotIdx].valid != 0) {
            slotIdx++;
        }
        if (slotIdx >= 16) break;
        g_pSavedEnemyState = &g_savedEnemyStates[slotIdx];
        SavedEnemyState* slot = g_pSavedEnemyState;

        bool store;
        if (ENTITY->health < 0) {
            slot->statusFlags = 0;
            // A death with a room event attached is permanent: the event flag
            // is what keeps it dead, so no snapshot is written.
            store = ((char)ENTITY->death_event_id == -1);
        } else {
            slot->statusFlags = (unsigned char)(ENTITY->status_flags & 0x0F);
            store = true;
        }

        // Bit 7 of +0x161 marks an entity cmd_enemy_set spawned unconditionally;
        // those are never snapshotted.
        if (store && (char)ENTITY->pad_160[1] >= 0) {
            slot->behaviorFlags = ENTITY->behavior_flags;
            slot->roomId        = g_roomId;
            slot->enemyType     = ENTITY->pad_160[1];
            *(unsigned int*)&slot->pad_04[0] = (unsigned int)ENTITY->state;
            slot->posX  = (short)ENTITY->scaMatrixData.localMatrix.t[0];
            slot->posY  = (short)ENTITY->scaMatrixData.localMatrix.t[1];
            slot->posZ  = (short)ENTITY->scaMatrixData.localMatrix.t[2];
            slot->angle = (unsigned short)ENTITY->angle;
            slot->valid = 5;
        }

        ENTITY++;
        g_enemy_count--;
    }

    g_enemy_count = 0;
    for (int i = 0; i < 30; i++) {
        g_EnemiesList[i].status_flags = 0;
    }
}

// ============================================================================
// FUN_0048f330 (0x0048f330) - restore_saved_enemy_state
// Scans the 16 slots of g_savedEnemyStates for an occupied entry matching the
// current room and the given enemy type. On a hit, copies the saved flags,
// position and angle into ENTITY, consumes the slot (valid = 0) and returns 1.
// Returns 0 when nothing matches.
//
// SCD opcode 0x1B (cmd_enemy_set) uses the return value to decide whether to skip
// its own spawn initialisation - a hit means "this enemy already has state, keep
// it where it was" rather than respawning at the script's coordinates.
//
// posY is applied only when the saved behaviorFlags have any of 0x70 set; the
// original tests ENTITY->behavior_flags, which it has just written from the slot.
// ============================================================================
int FUN_0048f330(unsigned char param)
{
    g_pSavedEnemyState = g_savedEnemyStates;
    int i = 0;
    while (g_pSavedEnemyState->valid == 0 ||
           g_pSavedEnemyState->roomId != g_roomId ||
           g_pSavedEnemyState->enemyType != param) {
        i++;
        g_pSavedEnemyState++;
        if (i > 0xF) {
            return 0;
        }
    }

    ENTITY->status_flags   = g_pSavedEnemyState->statusFlags;
    ENTITY->behavior_flags = g_pSavedEnemyState->behaviorFlags;
    ENTITY->scaMatrixData.localMatrix.t[0] = (int)g_pSavedEnemyState->posX;
    if ((ENTITY->behavior_flags & 0x70) != 0) {
        ENTITY->scaMatrixData.localMatrix.t[1] = (int)g_pSavedEnemyState->posY;
    }
    ENTITY->scaMatrixData.localMatrix.t[2] = (int)g_pSavedEnemyState->posZ;
    *(unsigned short*)&ENTITY->angle = g_pSavedEnemyState->angle;
    g_pSavedEnemyState->valid = 0;
    return 1;
}

// ============================================================================
// room_transition_load (0x004813c0) — the room/stage transition loader
//
// Previously stubbed in EngineStubs.cpp as "restore room state after menu close".
// That was wrong: it reads g_pendingDoorRecord seven times and is what actually
// carries the player through a door. door_try_enter stores the destination record
// and blacks the screen; this loads the room behind it.
//
// The branch flags Ghidra reports as `unaff_retaddr & 0x80/0x40` are NOT a
// parameter. The disassembly at 0x00481430 is:
//     MOV AL,[EAX] ; AND AL,0xC0 ; MOV byte ptr [ESP+0xb],AL
// with EAX = &record[0x0B]. So they are the top two bits of the record's own byte
// +0x0B: 0x80 = do not reload the room (camera-only transition), 0x40 = suppress
// the door sound. The low six bits are the entry camera.
//
// Destination encoding in record+0x0D: values < 0x20 are a room in the current
// stage; >= 0x20 also changes stage, as (dest >> 5) - 1, with +5 applied once
// g_ScenarioFlags bit 0 is set (the second-visit stage variants).
// ============================================================================
void room_transition_load(void)
{
    unsigned char* record = (unsigned char*)g_pendingDoorRecord;
    if (record == nullptr) {
        dbg_printf("[roomtrans] g_pendingDoorRecord is NULL - nothing to load\n");
        return;
    }

    g_roomTransitionBusy   = 1;
    g_AttractModeIdleTimer = 1;

    object_delete_00442170(0);
    SetScreenOffset(160, 120);

    // 0x004813ef: latch the record's fields.
    g_nextRoomDoorType = record[0x08];
    g_nextRoomSfxId    = record[0x09];
    g_nextRoom_be05b7  = record[0x0A];
    g_nextRoomCameraId = (unsigned char)(record[0x0B] & 0x3f);
    g_nextRoomDest     = record[0x0D];
    unsigned char flags = (unsigned char)(record[0x0B] & 0xc0);

    // 0x00481438: reset the three positional sound channels to centre/default.
    for (int i = 0; i < 3; i++) {
        g_SndPanVol[i].volume = 0x5f;
        g_SndPanVol[i].pan    = 0x5f;
    }

    load_room_sfx(g_nextRoomSfxId);
    door_system_load_data();            // FUN_00412300 - load the .dor + start the texture page

    // The original does:
    //     Task_execute(1, FUN_00444770);   // spawns the door-animation task
    //     Task_sleep(1);                   // yields so it can run
    //
    // 0x00444770 is `FUN_004443c0(); FUN_00444540(); FUN_00444500(); Task_exit();`
    // - the 3D door-opening animation (DoorSystem.cpp): it sets g_main_state_flags
    // bit 0x4000000 on init, animates the door (black rect, camera dolly, door
    // panels through the TMD queue) while this task loads the destination room,
    // and clears the bit on teardown. The wait loop below polls that bit.
    door_system_start_animation();
    Task_sleep(1);

    // 0x0048148c: place the player at the destination's entry point.
    //
    // X and Z are ZERO-extended into the 32-bit matrix translation, Y is SIGN-extended.
    // That asymmetry is explicit in the original and is not a decompiler artifact:
    //
    //   0048148f: XOR EAX,EAX / MOV AX,[rec+0x0E] / MOV [0x00be6318],EAX   <- zero-ext
    //   004814a8: MOVSX EAX, word ptr [rec+0x10]  / MOV [0x00be631c],EAX   <- sign-ext
    //   004814b3: XOR EAX,EAX / MOV AX,[rec+0x12] / MOV [0x00be6320],EAX   <- zero-ext
    //
    // It makes sense: X/Z are room coordinates that legitimately exceed 0x7FFF, while
    // Y is a height that goes negative. The port sign-extended all three, so any
    // entry point with X or Z >= 0x8000 landed ~65536 units away. The position
    // SVECTOR stores are plain 16-bit copies, so only the matrix writes differ.
    unsigned short ux = *(unsigned short*)(record + 0x0E);
    short          sy = *(short*)(record + 0x10);
    unsigned short uz = *(unsigned short*)(record + 0x12);
    g_playerEntity.scaMatrixData.localMatrix.t[0] = (int)(unsigned int)ux;
    g_playerEntity.scaMatrixData.localMatrix.t[1] = (int)sy;
    g_playerEntity.scaMatrixData.localMatrix.t[2] = (int)(unsigned int)uz;
    g_playerEntity.directionAngle = *(short*)(record + 0x14);
    g_playerEntity.posY       = (unsigned short)sy;
    g_playerEntity.animationId = 0;
    g_playerEntity.position.x = (short)ux;
    g_playerEntity.position.y = sy;
    g_playerEntity.position.z = (short)uz;

    // 0x004814f9: load the destination room.
    //
    // Bit 0x80 of record+0x0B means "camera-only transition" - stay in this room and
    // just re-aim the camera, which is why the else branch below is only a
    // check_camera_switch.
    //
    // The order here is load-bearing and was what the old gate lost:
    //   1. BuildEnemySnap saves the outgoing room's enemy state
    //   2. g_AttractMode_RoomCameraId remembers which room we came from
    //   3. the SCA pool rewinds to its base, freeing the outgoing room's hit data
    //   4. g_roomId becomes the DESTINATION before room_set/init_room reads it
    //
    // Destination encoding in record+0x0D: < 0x20 is a room in the current stage;
    // >= 0x20 also changes stage, as (dest >> 5) - 1, with +5 once g_ScenarioFlags
    // bit 0 is set (the scenario stage-variant bit: stages 0/1 remap to their
    // +5 variants; armed by room scripts / save data, never by ported code). A
    // stage change needs the heavier init_room, which re-points the stage data
    // and BGM tables first.
    if ((flags & 0x80) == 0) {
        BuildEnemySnap();
        g_AttractMode_RoomCameraId = g_roomId;
        g_scaPoolPtr = g_scaPoolBase;
        g_roomId = (unsigned char)(g_nextRoomDest & 0x1f);

        if (g_nextRoomDest < 0x20) {
            dbg_printf("[roomtrans] loading same-stage room %u (stage %u)\n",
                       (unsigned int)g_roomId, (unsigned int)g_stageId);
            room_set();
        } else {
            g_stageId = (unsigned char)((g_nextRoomDest >> 5) - 1);
            if ((Flg_ck((int)&g_ScenarioFlags, SCENARIO_FLAG_STAGE_VARIANT) != 0) && (g_stageId < 2)) {
                g_stageId = (unsigned char)(g_stageId + 5);
            }
            dbg_printf("[roomtrans] loading stage %u room %u (stage change)\n",
                       (unsigned int)g_stageId, (unsigned int)g_roomId);
            init_room();
        }
    }

    g_AttractModeIdleTimer = 1;
    // Waits for the door-animation task to clear bit 0x4000000. That task is not
    // spawned yet, so nothing sets the bit and this falls straight through.
    while ((g_main_state_flags & MSF_ROOM_TRANSITION) != 0) {
        Task_sleep(1);
    }

    // 0x0048156c: put the newly loaded room on screen. The camera-only branch has no
    // new data to build, so it re-runs the zone test instead.
    if ((flags & 0x80) == 0) {
        Room_SetupCamera();
        load_room_bg_image();
        Room_ApplySpriteFlags();
    } else {
        check_camera_switch(1);
    }

    update_room_bgm();
    if ((flags & 0x40) == 0) {
        play_sfx(0, 1, 0);
    }
    SndCompactAsync();
    FUN_00442180();

    g_AttractModeIdleTimer = 0;
    g_roomTransitionBusy   = 0;
}
