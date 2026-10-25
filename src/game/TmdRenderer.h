// TmdRenderer.h - 3D TMD object render queue (DX11 replacement for the
// original DX5 ordering-table 3D path).
//
// Original data flow (from the binary):
//   options_render_entity / render_entity
//     -> FUN_00483250 -> FUN_00483080 (0x00483080)
//          builds model->view float matrix, computes OT depth
//       -> CMarniDirect3DTMD::Transform (0x00415520)
//          copies the 16-float matrix into each per-object data entry
//          (objData + 8) and calls CMarniDirect3D vtable[10] (0x00448300)
//       -> OT_InsertPrimitive(objData, depth)
//   ...at present time the OT was walked and every queued TMD object was
//   rendered with D3D5 execute buffers.
//
// In this port the OT is replaced by a flat per-frame queue (TmdQueueObject)
// that FlushTmdObjects() drains once per frame from FrameRateGovernor,
// drawing through MarniDX::DrawTriangles.
//
// The original also had a D3D depth buffer resolving the triangles within one
// queued object. MarniDX::DrawTriangles carries no Z, so FlushTmdObjects
// depth-sorts individual triangles instead and does not backface-cull.
//
// See docs/MARNI_SYSTEM.md section 3B ("DX11 render path" and "Pipeline
// invariants") for the full chain and the list of constraints this path
// depends on.
#pragma once

// ============================================================================
// CMarniDirect3DTMD slot regions.
//
// The original keeps two SEPARATE regions, and the separation is load-bearing:
//
//   0x00923b50  the main TMD object buffer. ObjectCleanupCallback (0x00483e00)
//               sweeps exactly 250 slots of it (`MOV EDI,0x923b50` ... `CMP
//               ESI,0x3e8` counting by 4), calling CleanupObjects on each.
//   0x009104c8  the 12 door-animation slots, sized to end exactly where the
//               door texture page begins at 0x009207b8
//               (0x9104c8 + 12*0x1594 == 0x9207b8). No sweep reaches them.
//
// That matters because a stage-changing transition runs the destination room's
// load - and therefore the cleanup - WHILE the door animation task is still
// drawing. An earlier revision of this port folded both regions into
// g_tmdObjectBuffer at index 235, inside the swept range, so the sweep
// destroyed the door's TMD objects mid-animation: the first frame drew, then
// every frame after it saw m_initialized == 0 and drew nothing. Doors within
// one stage were unaffected, which is why only the stairs - always a stage
// change - looked black.
//
// So the port mirrors the split. g_doorTmdSlotBuffer is its own array, which
// means no sweep bound has to stay in step with a door-slot index: the cleanup
// count and the main buffer's capacity are now independent facts rather than
// two numbers that had to agree. TmdQueueObject resolves an objData pointer
// against both regions (plus the item-viewer slot at g_renderStateTMD, which
// the original also cleans separately, in FUN_00484e70).
// ============================================================================
// The item viewer's slots are the same story: the original puts them at
// 0x008f8d88, also outside the swept buffer. The port had them at index 247,
// inside it. Nothing draws them during a room load so it was latent rather than
// broken, but keeping them in their own region removes the coupling instead of
// relying on that staying true.
// With the reserved regions out of g_tmdObjectBuffer entirely, that buffer has
// exactly one meaning again: slots the entity allocator hands out and the
// cleanup destroys. CreateTmdObjectInternal scans 0..249 and
// ObjectCleanupCallback sweeps 0..249 - the same 250 the original uses, and no
// longer a number anything else has to dodge. (g_tmdObjectBuffer is sized for
// 290 slots; the extra 40 are slack, not a reservation.)
#define TMD_SLOT_STRIDE         0x1594
#define TMD_CLEANUP_SLOT_COUNT  250     // main-buffer slots ObjectCleanupCallback destroys
#define TMD_DOOR_SLOT_COUNT     12      // one per door order entry
#define TMD_ITEM_SLOT_COUNT     3       // one per item-viewer model object

extern unsigned char g_doorTmdSlotBuffer[TMD_DOOR_SLOT_COUNT * TMD_SLOT_STRIDE]; // DoorSystem.cpp, orig 0x009104c8
extern unsigned char g_itemTmdSlotBuffer[TMD_ITEM_SLOT_COUNT * TMD_SLOT_STRIDE]; // MainMenu.cpp,   orig 0x008f8d88
extern unsigned char g_itemSharedTmdSlot[TMD_SLOT_STRIDE];                       // MainMenu.cpp,   orig 0x008f8908

// Queue a TMD per-object data entry (0x84-byte block inside a
// CMarniDirect3DTMD slot: m_objectData or m_objectDataCopy) for rendering
// this frame. Called from the CMarniDirect3D vtable[10] implementation.
//
// Records the pointer only. CMarniDirect3DTMD::Transform writes the object's
// model->view matrix AFTER queueing it, so the render state must be read at
// flush time, never snapshotted here.
void  TmdQueueObject(void* objData, int depth);

// Queue one entry from the complex-object pool that ComplexTmdObjectSetup
// builds (geometry in g_objectListPtrArray at stride 0x38, render state in
// g_complexTmdObjectData at stride 0x84). Called only from FUN_00486df0.
void  TmdQueueComplexObject(void* objData, void* elem, int depth);

// (0x00486df0) - draw the complex-object pool. FUN_00483080 dispatches here for
// any animation object ComplexTmdObjectSetup has processed (spriteData[4] == 1).
void  FUN_00486df0(void* spriteData);

// Draw every queued TMD object, then clear the queue. Called once per frame
// from FrameRateGovernor after the background sprites and before the 2D sprite
// command flush. Triangles from all objects are pooled, depth-sorted far to
// near, and submitted with adjacent same-texture runs merged.
void  FlushTmdObjects(void);

// View-space Z -> the normalised [0,1] depth the TMD triangles write into the
// D3D11 depth buffer. The translucent ground-shadow / blood pool quads test
// against that buffer, so they must map their per-corner view Z through this
// exact function (see SpriteRenderer.cpp's type-12 branch).
float TmdViewZToNdc(float vz);

// Drop every queued object without drawing (frame reset).
void  TmdQueue_Reset(void);
