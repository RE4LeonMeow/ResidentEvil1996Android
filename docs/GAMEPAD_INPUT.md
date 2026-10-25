# Gamepad Input — XInput / WinMM Pad Support

Port addition, 2026-09-03. The original 1997 binary spoke WinMM joystick
(`joyGetPosEx`) and Microsoft SideWinder only, and in the port that path was
dead on arrival. This document covers the pad chain end to end: how a physical
controller reaches gameplay, what was broken, the bit contracts every layer
depends on, and the limits that remain.

Related: `docs/MARNI_SYSTEM.md` §3D (the input class itself),
`docs/ARCHITECTURE.md` (registry and startup order),
`docs/MEMORY_LAYOUT.md` (the past-the-end hazard behind §5.7).

---

## 1. Architecture

Two backends feed **one** joystick slot. Everything downstream — `JoyToPSX`,
`g_JoyRemapTbl[1]`, the options-menu binding editors, the save-file remap
tables — is unchanged from the original and cannot tell them apart.

```
XInput pad ──┐
             ├──▶ g_pMasterInputState.joysticks[0].currPress  (Marni pad mask)
WinMM pad ───┘            │
                          ▼
              JoyToPSX(mask, 1) ── ORs g_JoyRemapTbl[1][bit] per set bit
                          │
                          ▼
              ReadPadBoth() ── merges with the keyboard word (player 0)
                          │
                          ▼
              PlayerPad_Update() ── edge detection + g_padRemapTable
                          │
                          ├──▶ g_RawPadHeld / g_PlayerPadPressed   (raw word)
                          └──▶ g_PlayerDpadHeld / g_PlayerDpadPressed (remapped)
```

`joysticks[0]` is the **only** entry the game ever reads: `ReadPadBoth` tests
`+0x200` / `+0x3D4`, and `read_sidewinder_pad` (0x00497e30) returns `+0x200`.
Entries 1+ are polled but never consumed.

### Backend arbitration

`UpdateAllInputStates` polls XInput **before** the WinMM sweep. When an XInput
pad answers it takes slot 0 and the WinMM sweep skips index 0, so a controller
that enumerates on both APIs is not counted twice. On unplug the slot is
released back to WinMM and the stale press state cleared, so nothing stays
latched down. `s_xinputOwnsSlot0` (file-static in `MarniInput.cpp`) tracks this.

A DualShock 4 plugged straight into Windows is **HID/WinMM, not XInput** — it
only reaches the XInput path through a translation layer (Steam Input,
DS4Windows). Both paths are supported; the differences are in §4.

---

## 2. The pad mask contract

Any backend writing `joysticks[n].currPress` must produce this layout. It is
what `UpdateAllInputStates` has always built from `JOYINFOEX`, and
`g_JoyRemapTbl[1]` is indexed by **bit position** in it.

| Bits | Meaning | Source |
|------|---------|--------|
| 0-3 | Axis direction: `1`=UP `2`=DOWN `4`=LEFT `8`=RIGHT | analogue stick (WinMM), stick **and** D-pad (XInput) |
| 4-7 | POV hat: `0x10`=UP `0x20`=DOWN `0x40`=LEFT `0x80`=RIGHT | D-pad (WinMM) |
| 8+ | Buttons, `dwButtons << 8` — bit 8 = button 1 | both |

Two rules that are easy to get wrong:

- **The XInput D-pad is folded into bits 0-3, not the hat bits.** The hat bits
  are only produced by the WinMM path. This is a deliberate asymmetry: the
  default tables map both, so it makes no difference downstream, and it keeps
  the XInput backend from having to fake a POV angle.
- **The POV read is gated on `JOYCAPS_HASPOV`** (`0x0010`) in
  `JoystickEntry.povFlags`. `InitJoysticks` must copy `joyCaps.wCaps` into that
  field or the entire hat is silently ignored (see §5).

`JoyToPSX` walks all 32 bits and ORs `g_JoyRemapTbl[player][bit]` for each set
bit, so the *values* in the table are raw pad-word bits, not button ids:

| Raw value | Function |
|-----------|----------|
| `0x1000` / `0x4000` / `0x8000` / `0x2000` | up / down / left / right |
| `0x0080` | action / confirm |
| `0x0040` | cancel / run |
| `0x0008` | aim |
| `0x0800` | inventory (START) |
| `0x0900` | options |

See `memory/pad-raw-vs-remapped-bit-map.md` for the raw-vs-remapped distinction
that trips up menu code.

---

## 3. Joystick slot indexing (port deviation)

The original's entry array starts at object offset **0x28** and is walked

```c
for (i = 1; i < joystickCount; i++) joyGetPosEx(i - 1, &entry[i]);
```

so WinMM device 0 lands in `entry[1]`, at offset **0x200** — exactly the entry
every reader uses. The port's `MasterInputState` declares the array **at
0x200**, so port `joysticks[0]` *is* that entry.

Indexing here is therefore **0-based**: device `i` → `joysticks[i]`, and
`joystickCount` holds the plain device count rather than the original's
count + 1. Keeping the original's `i - 1` on top of the re-based array wrote
device 0 into `joysticks[1]`, where nothing reads it.

The same re-basing invalidates the original's zero-fill: a literal 0x3B00-byte
sweep from `+0x28` starts inside the keyboard block and stops partway through
the joystick array. `InitJoysticks` clears the equivalent region field-wise
instead.

> `joyGetNumDevs()` reports how many devices the driver **supports** (16 on
> every modern Windows), not how many are plugged in. The per-device
> `joyGetPosEx` validation in `InitJoysticks` is what prunes the list; entries
> that fail get `enabled = 0` and are skipped by the per-frame sweep.

---

## 4. Default bindings

`g_JoyRemapTbl[1]` as shipped in 1997 has **no options (`0x900`) entry at
all**, puts inventory on a stick click, aim on button 8, leaves the POV hat
unmapped, and points five buttons at raw bits the game never acts on. A pad
player cannot reach the options screen with it.

`InstallPadDefaultBindings()` (`src/game/InputSystem.cpp`) installs a usable
layout, choosing the table that matches whichever backend owns slot 0:

| Function | XInput | WinMM / HID (DualShock naming) |
|----------|--------|-------------------------------|
| action / confirm | A | Cross (button 2) |
| cancel / run | B, LT | Circle (3), L2 (7) |
| aim | X, LB, RB, RT | Square (1), L1 (5), R2 (8) |
| inventory | Y, Start | Triangle (4), Options (10) |
| **options** | **Back** | **Share (9)** |

Both tables also map the POV hat (bits 4-7), which the original left blank.

### Overwrite policy

It **only** replaces a table that is empty or byte-identical to
`g_JoyRemapTblLegacyJoyDefault` (the original layout, kept in `Globals.cpp`
next to the live table — keep the two in sync). Anything the player configured,
including what the SideWinder screen writes, is recognised and left alone, so
the call is idempotent and safe to repeat.

### Where it is called

- `InputUpdate()`, every frame a pad is present.
- `RestoreSaveBlock()`, after a save load has rewritten the table.

> **Trap:** do **not** gate this on a false→true transition of
> `g_bPadConnected`. `InitializeMarniSystem` already sets that flag from the
> startup probe, long before the first `InputUpdate`, so such a transition
> never happens. Gating it that way left the title and save screens on the
> original table — whose stick entries (0-3) are populated but whose hat
> entries (4-7) are empty, which presents as "the analogue stick moves the menu
> but the D-pad does nothing" while gameplay after a save load works fine.

---

## 5. Defects found and fixed

Everything below was broken before this work; each is a distinct failure.

### 5.1 No pad input reached the game at all (four gates)

1. `CMarniDirectInput::InitJoysticks` had **zero callers**. A same-named free
   stub in `MarniSystem.cpp`, which only printed an XInput probe, occupied the
   real call site — a stub-overload trap. It now forwards to the class method.
2. With no init, `joystickCount` stayed 0 and the per-frame poll loop never ran.
3. `g_NumControllers` was hard-coded to 1, and `ReadPadBoth` gates the joystick
   merge on `1 < g_NumControllers`. `InputUpdate` now maintains it.
4. The `i - 1` indexing described in §3 wrote device 0 where nothing read it.

### 5.2 `g_isSideWinderConnected` was two flags in one

The original conflates a one-shot event with a persistent capability, and the
port inherited it. Split:

| Flag | Meaning | Read by |
|------|---------|---------|
| `g_isSideWinderConnected` | **One-shot event** — raises raw `0x800` (START) once, then clears itself | `main_loop` (0x00428f4a) |
| `g_bPadConnected` | **Capability** — a pad is present | title screen (`t_start.tim` vs `t_press.tim`), character select, save/load screen, joy-vs-key remap backup selection |

Raising the one-shot on pad detection pops the inventory open on the first
frame. The registry probe in `Installation.cpp` and the startup probe in
`MarniSystem.cpp` now write the capability flag.

### 5.3 Loading any pre-existing save killed the pad

Every save file written before pad support stores an **all-zero** joystick
table. Two facts combine:

- `OFFSET_JOY_BACKUP` (0x8A0) is `OFFSET_JOY_REMAP` (0x820) + 0x80, so the save
  assembly writes `g_joyRemapBackupJoy` **straight over** `g_JoyRemapTbl[1]`'s
  bytes in the file.
- That backup was never populated while the SideWinder flag was hard-wired
  false — only `g_joyRemapBackupKey` (0xA02) was ever refreshed.

Restoring blanked every pad binding, and because it is a global the pad stayed
dead for the rest of the session, title screen included, while the keyboard
kept working (table[0] is intact).

`RestoreSaveBlock` now falls back to the other backup, then to the live table,
when the restored table is empty — then runs `InstallPadDefaultBindings()`.
Saves written after this fix round-trip correctly.

Verified against the shipped saves:

```
assets/SAVE/savedat1.dat
  0x820+0x80  table[1]   : all zeros
  0x8A0       joyBackup  : all zeros
  0xA02       keyBackup  : 0x1000 0x4000 0x8000 0x2000 ... 0x80 0x40 ...
```

### 5.4 The WinMM D-pad was dead

`InitJoysticks` called `joyGetDevCapsA` and discarded the result, leaving
`povFlags` permanently zero — and `UpdateAllInputStates` gates the entire POV
read on `JOYCAPS_HASPOV` in that field. Now `pJoy->povFlags = joyCaps.wCaps`.

### 5.5 The D-pad churned the options remap screens

`read_sidewinder_pad()` returns the whole pad word. The options scan state
machines treat **any** change in it as a button event, then search bits 8-15
for which button it was. On the original SideWinder layout the hat was unmapped
so directions never reached this code; once the port binds the hat, every
cursor movement churned the scan and the edit state would not settle.

The four `s_optJoyButtonScan` sites now mask with `JOY_SCAN_BUTTON_MASK`
(`0xFFFFFF00`). Every reader of that variable looks only at buttons.

> Deliberately **not** masked: `options_display_config_handler`'s `padResult`,
> which is a whole-word equality gate the original also fed unmasked.

### 5.6 Only buttons 1-9 could be displayed

`options_map_key_to_print_index` (0x00453950) turns a bit index into one font
character: 0-3 and 4-7 → arrow glyphs, 8..16 → `'1'`..`'9'`, everything else →
`'_'`. It spells the button number as a single digit and runs out at `'9'`.

This is a **font limit, not a binding limit** — the JOY PAD tab's capture loop
already accepted bit indices 4..30, so buttons 10+ did bind; they just rendered
as `_`, which reads as unbound. The run now continues with `'A'`..`'O'` for
buttons 10-24 (bit indices 17-31), and the capture bound was raised from
`bitIdx < 0x1f` to `< 0x20` so bit 31 (button 24) is reachable.

### 5.7 The options binding screens scanned unrelated memory

Found while writing this document. It is why the JOY PAD tab showed `-` on
rows that were in fact bound.

Three sites in `src/game/OptionsMenu.cpp` walked `g_JoyRemapTbl[1]`
**backwards starting from `&g_JoyWarnPrinted`**:

- `options_init_keybind_display` — populates the whole binding display
- `options_display_config_handler` — finds the button bound to `0x800`
- `options_key_config_input` (state 0) — populates the JOY PAD tab's five rows

```c
const int* pJoy = &g_JoyWarnPrinted;   // "at end of g_JoyRemapTbl data"
int i = 0x20;
do { int joyVal = *pJoy; /* ... match against 0x80/0x40/8/0x800/0x900 ... */
     pJoy--; i--; } while (i > 0);
```

In the **original** that is exact, not a hack:

```
g_JoyRemapTbl    0x004b1858 .. 0x004b1958   (2 x 32 x 4 = 0x100)
g_JoyWarnPrinted 0x004b1958                 == &g_JoyRemapTbl[1][32]
```

So the first read is a harmless past-the-end read of index 32 and the walk-down
covers indices 31..1 of the live table.

In the **port** that adjacency does not exist. `g_JoyWarnPrinted` is
zero-initialised so it lands in `.bss`, while `g_JoyRemapTbl` has non-zero
initialisers and lands in `.data` — confirmed with `dumpbin /symbols`:

```
g_JoyWarnPrinted  SECT4 (.bss)  +0x9DBC
g_JoyRemapTbl     SECT5 (.data) +0x0058
```

`pJoy--` therefore walks 32 ints backwards through unrelated `.bss` globals, and
whichever of them happens to equal `0x80`, `0x40`, `8`, `0x800` or `0x900`
decides what the binding screens display. Bindings themselves are unaffected —
`JoyToPSX` indexes the table directly — so this is a **display and
populate** defect, not an input one.

This is exactly the past-the-end hazard `docs/MEMORY_LAYOUT.md` warns about:
*"original code that reaches a neighbouring data block via `&someArray[N]`
works only because of the original address layout. Never port it as-is."*

**Fixed** by addressing the table directly: all three sites now index
`g_JoyRemapTbl[1][i]` instead of walking a neighbour pointer. Sites 677 and
1762 scan `i` = 31 down to 1, site 1026 scans 31 down to 0 for `0x800` — the
ranges the original covers. The dropped index-32 iteration only ever read the
warning flag (0 or 1), which matches no function value. Index 0 is not scanned
by the two populate sites in the original either; that part is authentic and
was left alone.

---

## 6. Menu behaviour

Two screens edit `g_JoyRemapTbl[1]`, and they work in opposite directions.

| | JOY PAD tab (`options_key_config_input`) | SideWinder screen (`options_joystick_config_input`) |
|---|---|---|
| Orientation | 5 **functions**, press the button to bind | 8 **buttons**, cycle through a function list |
| Rows | ACTION/SELECT, DASH/CANCEL, GET READY, SUB SCREEN, OPTION | `g_JoyRemapTbl[1][8..15]` = buttons 1-8 |
| Button range | bits 4-31 → buttons 1-24 | bits 8-15 → buttons 1-8 only |
| Layout | 5 fixed rows | 8 fixed positions in `g_abOptJoyLabelX/Y` |

Cancelling out of the SideWinder edit state requires the **keyboard** (Esc,
Ctrl, or the bound cancel key); a pad press always commits. That is original
behaviour, not a port defect.

### Debug menu (port addition, `src/game/DebugMenu.cpp`)

The F1 debug overlay takes the pad too. It folds pad input into the same
`DBGKEY_*` bitmask the keyboard sampling builds, so every context — room
change, inventory editor, flag editor, quick access — gets it at once.

| Debug menu action | Keyboard | Pad |
|---|---|---|
| open / close | F1 | **L1 + R1 together** |
| move cursor | arrows | d-pad or stick |
| confirm | Enter / Space | the bound **action** button |
| back / close | Esc | the bound **cancel** button |
| flag editor modifier | the bound aim key | the bound **aim** button |

Navigation reads **`g_RawPadHeld`**, the function-level word — it has already
been through `g_JoyRemapTbl`, so a rebind in the options menu carries over and
the overlay never needs to know which physical button it is on, on either
backend. That word stays live while the menu is open: `PlayerPad_Update` blanks
only the *published* gameplay words (`g_PlayerPadPressed` and friends), never
`g_RawPadHeld`. It also carries the keyboard, so the `GetAsyncKeyState` samples
are partly redundant — they set the same bits, which is harmless.

The open/close toggle cannot work that way, because every bound function
already means something in gameplay. It reads the **raw hardware mask**
(`read_sidewinder_pad()`, bit 8 = pad button 1) and wants bits 12|13 — pad
buttons 5 and 6. Those are L1/R1 (LB/RB) in the XInput ordering *and* in the
WinMM/HID ordering, so it is the one combo that means the same thing on both
backends. The default tables bind both shoulders to **aim**, so holding them
together has no conflicting side effect beyond opening the overlay — which is
why this pair was chosen over Back+Start (options + inventory would both fire).

---

## 7. Files

| File | Role |
|------|------|
| `src/marni/MarniXInput.h` / `.cpp` | XInput backend (new). Slot scan, deadzone, mask synthesis, throttled hot-plug |
| `src/marni/MarniInput.h` / `.cpp` | Marni input class: keyboard, WinMM sweep, XInput arbitration, `MarniPadIsConnected` |
| `src/marni/MarniSystem.cpp` | `InitJoysticks()` startup call site, `g_bPadConnected` probe |
| `src/game/InputSystem.cpp` | `JoyToPSX`, `ReadPadBoth`, `InputUpdate`, `PlayerPad_Update`, pad default tables + `InstallPadDefaultBindings` |
| `src/game/OptionsMenu.cpp` | Binding editors, `JOY_SCAN_BUTTON_MASK`, button glyph mapping |
| `src/game/SaveLoadScreen.cpp` | `RestoreSaveBlock` compatibility guard |
| `src/Globals.cpp` / `.h` | `g_JoyRemapTbl`, `g_JoyRemapTblLegacyJoyDefault`, `g_bPadConnected`, `g_NumControllers` |

### XInput backend details

- Scans all four slots and latches the first that answers. Slots known to be
  empty are rescanned **once a second**, not every frame — `XInputGetState` on
  a disconnected slot is measurably slow and there are four of them, which
  would eat into the 33 ms tick.
- Left-stick deflection past `MARNI_XI_DEFAULT_DEADZONE` (10000 of 32767)
  raises a direction bit; the D-pad ORs into the same bits. Opposite directions
  are resolved (UP beats DOWN, LEFT beats RIGHT).
- Triggers are reported as digital buttons past
  `XINPUT_GAMEPAD_TRIGGER_THRESHOLD`; the game has no analogue consumer.
- Buttons occupy bits 8-19: A, B, X, Y, LB, RB, Back, Start, LS, RS, LT, RT.

---

## 8. Not done

- **`config.ini` `[Input]` section.** `MarniXInput::SetEnabled()` and
  `SetDeadzone()` exist and are wired to nothing; a `[Input] EnableXInput` /
  `Deadzone` block read alongside the `[Display]` keys in `main.cpp` would
  finish it.
- **SideWinder screen past 8 buttons.** Its rows are eight hardcoded screen
  positions, so widening it means a scrolling list or a second page, not a
  larger table. Left as is by request.
- **Rumble.** No vibration support; `XInputSetState` is never called.
- **Per-device button-order detection.** The WinMM default table assumes
  DualShock HID ordering. On an XInput-era pad seen through WinMM every core
  action is still reachable, but shifted by one face button. Keying the table
  off `joyGetDevCapsA`'s `wMid` / `wPid` would fix that properly.
