# RE1 Text Encoding & the STR() Macro

This document describes how in-game text is represented and rendered in the
decomp, and how the compile-time `STR()` macro (`src/game/PrintText.h`) turns
readable ASCII source strings into the byte streams the game engine consumes.

## 1. Overview

RE1 text is **not** stored as ASCII. Every text glyph is a byte in the range
`0x00..0xFF`, where the low 144 values (`0x00..0x8F`) index a glyph in the
**8×14 game-text font** region of `fontus.tim`, and the rest are control codes
(`0xF8/0xF9/0xFA` extended characters) or the `0x02..0x0B` controller
symbols. See the font layout at the top of `src/game/PrintText.cpp`.

Two consumers interpret these bytes:

| Consumer | Function | Purpose |
|----------|----------|---------|
| Formatted text renderer | `PrintFormattedText` (0x00455190) | Debug / menu text |
| Message display | `UpdateMessageDisplay` (0x004557b0) | Room dialogue, item messages, prompts |

The decomp stores the actual text as readable C++ strings and converts them to
the font encoding **at compile time** with `STR()`:

```cpp
static constexpr auto s_pftHello = STR("HELLO WORLD");
PrintFormattedText(x, y, color, s_pftHello);
```

## 2. Character encoding

`pft_detail::encodeChar()` (`src/game/PrintText.h`) maps one ASCII character to
one font-index byte:

| ASCII | Font byte | | ASCII | Font byte |
|-------|-----------|-|-------|-----------|
| `A`–`Z` | `0x1D` + (c − 'A') | | `a`–`z` | `0x3D` + (c − 'a') |
| `0`–`9` | `0x0C` + (c − '0') | | ` ` (space) | `0x00` |
| `!` | `0x1A` | | `"` (closing) | `0x19` |
| `,` | `0x18` | | `.` | `0x79` |
| `:` | `0x16` | | `;` | `0x17` |
| `?` | `0x1B` | | `'` | `0x3A` |
| `(` | `0x37` | | `)` | `0x39` |
| `-` | `0x3B` | | `/` and `\` | `0x38` |
| `\x02..\x0B` (raw) | unchanged | | `\xF8..\xFF` (raw) | unchanged |

Notes:

- `0x02..0x0B` are the **8×14 controller symbols** (Start, L2/R2/L1/R1, △, ○,
  ×, □, ▼). They pass through `encodeChar` unchanged, so they can be embedded
  in a string literal with `\x02`..`\x0B` or the `STR_*` fragment macros.
- Bytes `≥ 0xF8` also pass through unchanged — they are `PrintFormattedText`
  control codes.
- Any unmapped printable character becomes `?` (`0x1B`).
- The 8×14 grid is 18 columns × 8 rows; a plain glyph byte `b` is drawn at
  `col = b % 18`, `row = b / 18 + 2` (the `+2` skips the control/symbol rows).

## 3. The STR() macro

Defined in `src/game/PrintText.h`:

```cpp
#define STR(str) (::pft_detail::Encoded<sizeof(str)>{str})
```

`Encoded<N>` is a `constexpr` struct holding `unsigned char bytes[N]`. Its
constructor walks the source literal (its `\` escapes first, then
`encodeChar()` per character) and writes the encoded bytes, then always appends
**one terminator byte** `0x01`. The result is exposed through `.bytes`
(implicitly convertible to `const unsigned char*`).

### Escapes

| Escape | Emits | Meaning |
|--------|-------|---------|
| `\n` | `0x02` | Newline (message: line break) |
| `\p` | `0x03` | Page break; the next char is a delay operand |
| `\s` | `0x04` | Set character delay |
| `\i` | `0x05` | Item-name placeholder (**see §6 — currently collides with tag 0x05**) |
| `\c` | `0x08` | Yes/No prompt |
| `\q` | `0x0A` | Square glyph |
| `\o` | `0x78` | Opening double quote (plain `"` is the closing form `0x19`) |
| `\d` | — | Auto-dismiss: the next character is encoded and written **after** the `0x01` terminator (see below) |
| `\\` | `0x38` | Backslash glyph |

### Terminator & the byte after it

The byte **following** the `0x01` terminator is read by the message state
machine and is part of the message, not padding:

- `0x00` → state 5: hold the text and wait for a button press.
- `N ≠ 0` → state 6: auto-dismiss after `N` frames.

`Encoded`'s zero-initialised tail gives the wait-for-input form (byte `0x00`)
by default. Messages that must clear themselves supply the frame count with
`\d` — in the original all four opening narrations do
(`0x004bf763/0x004bf795/0x004bf7e4/0x004bf836`).

### 8×14 controller glyph fragments

```cpp
STR_START "\x02"  STR_L2 "\x03"  STR_R2 "\x04"  STR_L1 "\x05"  STR_R1 "\x06"
STR_TRI  "\x07"  STR_CIR "\x08"  STR_CROSS "\x09"  STR_SQR "\x0A"  STR_SEL "\x0B"
```

14×14 symbols are two-byte sequences `\xF8` + index; use `STR_BTN_*`
(`STR_BTN_L1`, `STR_BTN_TRI`, …) for those.

## 4. PrintFormattedText (0x00455190)

Simple linear renderer over an encoded byte array:

| Byte | Meaning |
|------|---------|
| `0x00` | Advance 8 px (space) |
| `0x01` / `0x07` | End of text (return) |
| `0xF8 nn` | 14×14 controller symbol (index `nn`) |
| `0xF9 nn` | 8×14 character, depth 31 |
| `0xFA nn` | 8×14 character, depth 31, row offset +14 |
| `0xFB` | No-op (advance) |
| `0xFF` | Half-width advance (4 px) |
| *default* | Render glyph `nn` (depth 30) |

Used by `LoadSaveGameState` and other menu rendering.

## 5. Message display state machine (UpdateMessageDisplay, 0x004557b0)

Processes the message byte stream character-by-character with reveal timing.
The tag set (verified against the original binary via Ghidra):

| Tag | Meaning | Operand |
|-----|---------|---------|
| `0x01` | End-of-text marker | next byte: `0` = wait for input, `N` = auto-dismiss after N frames |
| `0x02` | Next page / skip | — |
| `0x03` | Newline (line break) | optional page-delay byte |
| `0x04` | Skip embedded tags | — |
| `0x05` | Set text CLUT color | 1 byte, color index |
| `0x06` | **Item-name lookup** | 1 byte: item id (`0` = selected item) |
| `0x07` | Return from item name | — |
| `0x08` | Yes/No prompt | — |
| `0xF8/0xF9/0xFA` | Extended characters | 1 byte each |

Plain glyph bytes (`0x0C..0x8F`, i.e. `0x0B < b < 0xF8`) render as characters.
The message is selected by `set_message_display()` (`src/game/RoomInit.cpp`):
`msg_id & 0x40` chooses between RDT room messages and the `global_messages[]`
table, `msg_id & 0x3F` is the index.

### Item-name substitution

When the state machine hits tag `0x06`, it sets the read pointer to
`message_item_name_lookup(itemId)` (`src/game/Rendering.cpp`, 0x00455140). The
name string bytes are then processed like normal message characters until the
name's **`0x07` terminator**, which doubles as the "return from item name"
tag (case 7) and jumps back to the message right after the `0x06`/operand
bytes. Item-name strings are therefore terminated with `0x07`, not `0x01`.

The original binary wraps the name in a CLUT color change so the name is
highlighted:

```
...the  05 01  06 00  <name bytes> 07  05 00  ...rest
       └─color 1──┘  └─item name─┘  └─color 0─┘
```

## 6. Encoded string tables in the decomp

### Item names — `src/game/MenuData.cpp` (0x07-terminated)

Originally one flat 984-byte block at `0x004BECC8`; each name terminated by
`0x07`. Now 78 `s_item*` constants, one per unique name:

```cpp
static constexpr auto s_itemCombatKnife = STR("COMBAT KNIFE\x07");  // +0x000
```

Consumed through two pointer tables:

- `g_ItemNamePointers[77]` (0x004BF0A0) — indexed by `itemId - 1`;
  `message_item_name_lookup()` returns one of these.
- `g_UnknownItemNamePointers[16]` (0x004BF260) — generic names shown for
  unexamined items, by item-category.

Each entry's comment records the original offset inside the flat block, so the
pointer-to-string relationship is preserved even though the block is gone.

### Global messages — `src/Globals.cpp` (0x01-terminated)

`global_messages[64]` (0x004BFC58) holds pointers to `s_gm00..s_gm62` STR
constants. These are displayed by `set_message_display()` when `msg_id` bit 6
is set; index is `msg_id & 0x3F`. All four opening narrations end with `\d` so
they auto-dismiss.

### Item descriptions — `src/Globals.cpp` (0x01-terminated)

`g_ItemDescriptions[79]` holds pointers to `s_idesc*` STR constants, shown by
the item viewer (`set_item_description_message`, 0x00455730). These live in
their own table — they are neither RDT nor global messages.

### RDT room messages — raw bytes

Room-specific dialogue lives inside `.rdt` files (see `docs/RDT_FILE_FORMAT.md`)
and is **not** STR-encoded; it is already in font-encoding form when loaded.

### ASCII file names & paths — plain string literals, never STR()

Not every byte string in the menu tables is rendered text. Filenames and paths
are **plain ASCII** and must stay as ordinary C string literals — do **not**
wrap them in `STR()` (that would font-encode them and break file loading):

- `g_ItemModelFileNames[75][8]` — 8-byte null-padded model file names
  (`"i05v"`, `"i00v"`, …), `src/game/MenuData.cpp`.
- `g_ItemModelFileNameING` / `g_ItemModelFileNameMINI` — `"ING"` / `"MINI"`.
- `g_ItemModelExtIVM` — `".ivm"`; `g_ItemModelDir` — `"./usa/item_m2/"`;
  `g_ItemMixPixPath` — `".\usa\data\item_mix.pix"`.

These are consumed via `strcat`/`LoadFile` (e.g. `menu_load_item_model`), not
by the font renderer.

## 7. The `\i` escape vs. tag 0x06

The message protocol's item-name tag is `0x06` (§5). The `\i` escape emits the
full original sequence `05 01 06 00 05 00` — CLUT 1 (green tint), tag 06 with
arg 0 (= `g_selectedItemId`), CLUT 0 — matching e.g. the original "You got the
[item]." bytes at `0x004bf3f7`. `Encoded<N>` sizes `bytes` at `N*3+2`, so a
message ending in `\i` fits.

Two things to remember when a message embeds an item name:

- `\i` resolves `g_selectedItemId` at display time. The typewriter texts
  (`global_messages[30/31]`, 0x004BF886/0x004BF8D9) do **not** use it — the
  original hardcodes the literal `INK RIBBON` between raw `\x05\x01` /
  `\x05\x00` CLUT brackets, because `g_selectedItemId` is not the ribbon when
  those messages fire.
- The green colour is not baked into fontus.tim (its CLUT has one grayscale
  row); `AddTintSprite` (0x0046e0a0) maps `printClutTint - 0x1E0` to an RGB
  tint (0 white, 1 green, 2 red, 3 gray, else yellow), which the port
  implements in `src/game/Rendering.cpp`.

All 64 `global_messages` entries have been verified byte-for-byte against
`ResidentEvil.exe` (`tools/verify_msg_fixes.py` + `tools/decode_msg_table.py`).

## 8. Japanese text — `FONT.TIM` and the `STR_JP()` macro

The Japanese PC release (`Biohazard.exe`) keeps the **identical** message
protocol, the identical tag set and the identical escapes. Only two things
change: the glyph sheet, and the fact that a glyph is no longer always one byte.

### 8.1 The font sheet

`data\FONT.TIM` is a **768x256** 4bpp TIM (`fontus.tim` is 256x256). It holds
the 8x8 ASCII region at the top left exactly like the USA font, and then two
game-text regions of **14x14** glyphs, 18 columns each:

| Region | VRAM | Rows | Reached by |
|--------|------|------|-----------|
| Left page | u 0..251, v = 28 + r*14 | r = 0..15 | plain byte, and `0xF8 nn` |
| Right page | u 256..507, v = r*14 | r = 0..17 | `0xF9 nn`, `0xFA nn` |

The third 256-wide page (u 512..767) is empty.

```
plain byte b (0x0C..0xF7)   left page,   row b/18,        col b%18
0xF8 nn                     left page,   row nn/18 + 13,  col nn%18
0xF9 nn                     right page,  row nn/18,       col nn%18
0xFA nn                     right page,  row nn/18 + 14,  col nn%18
```

That is the same arithmetic the USA renderers already use — the `+2`/`+15`
row bias, `/18`, `%18`. What differs is the **page**: the renderers write
`TextureDesc.depth` = `0x1E` for the left page and `0x1F` for the right one,
and the original's `AddTintSprite` resolves that to a texture page (the JPN
`AddTintSprite`, 0x00441120, searches page slots 12-14 for the id in
`TextureDesc+0x0C`). `texU` is a byte in the descriptor and cannot reach 256,
so the port adds the page offset in `AddTintSprite` (`src/game/Rendering.cpp`),
gated on the font sheet actually being wider than one page. `fontus.tim` is one
page wide, never registers a `0x1F`, and is unaffected.

Glyph width is 14 px instead of 8, which `PrintText8x14`, `PrintFormattedText`
and `message_render_chars` already select from `GetVersion()`.

### 8.2 Character table

The left page's first five rows are the **same table** `fontus.tim` has, so
`A-Z`, `a-z`, `0-9` and most punctuation encode to the same bytes as `STR()`.
From index 87 (where the USA font has `Ä`) it diverges into kana, then kanji;
the right page is 324 kanji. The full map lives in `tools/jpn_font_table.py`.

Three ASCII characters cannot keep their `fontus.tim` index:

| Char | USA | JPN | Why |
|------|-----|-----|-----|
| `.` | `0x79` | `0xF8 0x1C` | index 121 is a kana here; the latin full stop is on left row 14 |
| `,` | `0x18` | `0x17` | index 24 is `。`; `0x17` is `、`, which the Japanese text uses |
| `;` | `0x17` | `0x17` | no `;` glyph — falls back to `、` |

### 8.3 `STR_JP()`

`STR_JP()` (`src/game/PrintText.h`) is `STR()` for that sheet. Source text is
written as UTF-8 and looked up by codepoint in `src/game/JpnFontTable.h`, a
generated codepoint-sorted table the macro binary-searches in a constant
expression:

```cpp
static constexpr auto s_msg = STR_JP(u8"カギがかかっている");
```

The literal **must** be `u8""` and the file **must** be UTF-8 with a BOM — a
narrow literal is converted to the execution code page first and the codepoints
never arrive. The escapes are exactly `STR()`'s (`\n \p \s \i \c \q \xNN \d`);
`\o` has no counterpart because the Japanese quotes are `“` and `”` (left row
14) and are written as themselves.

### 8.4 The four text tables

`src/game/JpnTextTables.cpp` (generated) holds every text table the Japanese
executable carries in its own image. Each reader picks the JPN table when the
JPN asset tree is selected, because the encoding only means anything against
`FONT.TIM`:

| Table | JPN address | USA address | Read by |
|-------|-------------|-------------|---------|
| `global_messages_jpn[64]` | `0x004CDE58` | `0x004BFC58` | `set_message_display` (JPN 0x00491980) |
| `g_ItemNamePointersJpn[128]` | `0x004CD388` | `0x004BF0A0` | `message_item_name_lookup` (JPN 0x00491440) |
| `g_UnknownItemNamePointersJpn[16]` | `0x004CD548` | `0x004BF260` | same, for unexamined items |
| `g_ItemDescriptionsJpn[79]` | `0x004C9370` | `0x004C6160` | `set_item_description_message` (JPN 0x00491A40) |

The name table's last 16 entries **are** the unexamined-item table — the two
overlap in both builds (`g_ItemNamePointers[112..127]`), and the port keeps
them as two arrays with identical tails, as it already did for the USA side.

Item names end with `0x07`, not `0x01`: the message state machine reads that as
"return from item name". `STR_JP()` still appends its own `0x01` after it, the
same way `STR()` does for the USA names in `MenuData.cpp`; nothing reads past
the `0x07`.

**`STR()` escapes have two levels** — get this wrong and the message renderer can
hang. `STR()` is a constexpr *parser*, so its own escapes must be **doubled** in
the C source, while a literal quote uses the normal C escape:

| need | write in the C source | why |
|---|---|---|
| line break (`0x02`) | `\\n` | the compiler hands the parser `\`+`n`, which it maps |
| opening quote (`0x78`) | `\\o` | same — the port's `...is \\oMoonlight...` |
| closing quote (`0x19`) | `\"` | a bare or doubled quote terminates the literal |

A single `\n` becomes raw `0x0A`; the parser then builds a string with no `0x01`
terminator and the message renderer loops forever. This hung the item viewer on
item `0x02` on 2026-09-14 (found while porting the DC's description table,
`tools/gen_dc_item_descriptions.py`).

Messages 27-29 (the opening narration) are **English even in the Japanese
build**, byte-for-byte the same text as the USA table.

Two of the readers also need the glyph width and the left margin, not just the
table: `draw_item_name` (`FUN_00454fd0`, JPN 0x004912C0) advances 14px per
glyph, and its callers push `0x22` for the item-name line where the USA ones
push `0x30` — the same shift the message box gets in §8.5. The item box
(`0x2A`) and the file-title x table are identical in both builds.

### 8.5 Message box layout

The message box is not just the same box with wider glyphs — the Japanese
`UpdateMessageDisplay` (0x00491ac0) and `message_render_chars` (0x00492360)
carry their own constants, because 14px glyphs would otherwise run off the
right edge and the Yes/No row would sit under the text:

| | USA (0x004557b0 / 0x00456020) | JPN (0x00491ac0 / 0x00492360) |
|---|---|---|
| text left margin (and after `
`) | `0x30` | `0x22` |
| page-wait ▼ | index 11 → texU `11*8` = 88, 8 wide | texU `11*14` = `0x9A`, 14 wide |
| Yes/No ► cursor | index 2 → texU `2*8` = `0x10`, 8 wide | texU `2*14` = `0x1C`, 14 wide |
| Yes/No cursor X | `0xD0` / `0xF8` | `0xA0` / `0xE6` |
| `"Yes  No"` X | `0xD8` | `0xAE` |

Both cursors are the **same character-table indices** in both builds (11 and 2
on row 0); only the column pitch and sprite width change. The cursor always
sits one glyph left of its label, and the "No" cursor five glyphs right of the
"Yes" one, so the whole row is derived from the glyph width in
`src/game/Rendering.cpp`.

### 8.6 Tooling

| Tool | Does |
|------|------|
| `tools/jpn_font_table.py` | the glyph map; the single source of truth |
| `tools/jpn_msg_decode.py messages\|items\|names\|unknown` | decode any JPN table to readable text |
| `tools/jpn_msg_decode.py verify` | decode + re-encode all four tables and diff against the executable |
| `tools/gen_jpn_text.py` | regenerate `JpnFontTable.h`, `JpnTextTables.cpp` and `test_str_jp.cpp` |
| `tools/test_str_jp.cpp` | `static_assert`s all 233 strings against the original bytes; `cl /nologo /c /EHsc tools\test_str_jp.cpp` |

`jpn_msg_decode.py verify` reports **all entries byte-identical** and
`test_str_jp.cpp` compiles clean, so the glyph table, the encoder and the C++
macro agree with the original executable for every string it ships.
