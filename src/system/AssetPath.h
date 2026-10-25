// AssetPath.h - Compile-time asset root
//
// The original hardcodes GAME_DATA_ROOT in front of every asset path. This project keeps
// the same tree under ".\assets\USA\" for development, so the only thing that
// differs is the root.
//
// That substitution is done at COMPILE TIME, with GAME_DATA_ROOT standing in for
// the original's GAME_DATA_ROOT literal. An earlier revision instead rewrote paths at
// runtime (ResolveAssetPath, which scanned for a "\usa\" component and spliced in
// "\assets\USA\"). That was worse in two ways:
//
//   1. It was a behavioural divergence with no counterpart in the original - a
//      string transform sitting between the game and the filesystem.
//   2. It was not idempotent, and nothing stopped it running twice. Once the sound
//      loaders resolved a path and handed the result to CreateSound, which resolved
//      again, ".\assets\USA\voice\V001_00.wav" matched its own "\USA\" component
//      and became ".\assets\assets\USA\voice\V001_00.wav".
//
// A macro cannot double-apply, and it keeps the path literals in the decompiled
// code reading the way the original's do.
#pragma once
#include <stddef.h>

#if !defined(_WIN32)
// Non-Windows: the shipped asset tree lives at ./assets/<REGION>/ and the
// filesystem wants '/' separators. Per-component case is fixed up at open time
// by plat_normalize_path.
#define GAME_DATA_ROOT      "./assets/USA/"
#elif defined(_DEBUG)
#define GAME_DATA_ROOT      ".\\assets\\USA\\"
#else
#define GAME_DATA_ROOT      ".\\usa\\"
#endif

// Japanese (Biohazard) data root. Like GAME_DATA_ROOT it is substituted at
// compile time, so JPN path literals (e.g. the FMV table) initialized statically
// read as the JPN tree instead of the retail ".\usa\" form that ResolveAssetRoot
// would otherwise have to swap. Debug/Release lengths match GAME_DATA_ROOT.
#if !defined(_WIN32)
#define GAME_DATA_ROOT_JPN  "./assets/JPN/"
#elif defined(_DEBUG)
#define GAME_DATA_ROOT_JPN  ".\\assets\\JPN\\"
#else
#define GAME_DATA_ROOT_JPN  ".\\jpn\\"
#endif

// Save directory root. The original builds save paths as "%ssavedat%d.dat"
// with the literal "SAVE\\". This is only the compile-time fallback now - the
// live value comes from GetSaveRoot() (config.ini [Save] Path, else
// <assets base>/SAVE).
#if !defined(_WIN32)
#define GAME_SAVE_ROOT      "./assets/SAVE/"
#elif defined(_DEBUG)
#define GAME_SAVE_ROOT      ".\\assets\\SAVE\\"
#else
#define GAME_SAVE_ROOT      "SAVE\\"
#endif

// Length of the root, excluding the terminator.
#define GAME_DATA_ROOT_LEN  (sizeof(GAME_DATA_ROOT) - 1)

// Byte offset of a character in a path built as GAME_DATA_ROOT followed by the
// original's template body. Needed only by paths that are fixed-layout templates
// patched by character index (g_bgPathTemplate). `originalIndex` is the index the
// original used, which is relative to its own 6-character GAME_DATA_ROOT root.
#define GAME_DATA_PATH_IDX(originalIndex) (GAME_DATA_ROOT_LEN + (originalIndex) - 6)

#ifdef __cplusplus
extern "C" {
#endif

// Runtime asset-root selector. Compile-time GAME_DATA_ROOT stays the USA default
// so static initializers (g_bgPathTemplate, the door table, g_maskPathTemplate)
// are still valid; SetAssetVersion() swaps the root the readers use at runtime,
// letting a single build run USA *or* JPN assets per config.ini [Assets].
//
// Where that root *is* comes from SetAssetBase(): the folder holding the USA/
// and JPN/ trees, which config.ini [Assets] Path sets (relative paths are
// resolved by the caller against the executable's directory). With no base
// configured the compile-time roots above stay in force.
void         SetAssetBase(const char* base);
const char*  GetAssetBase(void);
void         SetAssetVersion(const char* version);
const char*  GetAssetRoot(void);

// Save folder (with a trailing separator), from config.ini [Save] Path or
// <asset base>/SAVE/ when that key is empty. Replaces GAME_SAVE_ROOT, which is
// now only the compile-time fallback. The result is case-resolved against the
// filesystem, so an existing SAVE/save/Save directory is found either way.
void         SetSaveRoot(const char* path);
const char*  GetSaveRoot(void);
// 1 when the JPN (Biohazard) asset tree is active, 0 for the USA default.
int          GetAssetVersion(void);

// Content-mode overlay (config.ini [Game] Mode). The base tree selected above
// is always complete and is never written to; a non-OG mode ships only the
// files it changes or adds, in a sibling folder named after the mode
// ("<base>/DC/", and with no [Assets] Path the same shape as the base root -
// "./assets/DC/" in a dev build, ".\dc\" in retail).
//
// ResolveAssetRoot then searches OVERLAY FIRST, BASE SECOND: a file the overlay
// does not carry falls through to the base tree unchanged. That is what lets
// one install hold several releases at once, keeps OG byte-identical to today
// (with no overlay set the probe does not run at all), and means a mode only
// has to supply its diff rather than a whole duplicated tree.
//
// Pass NULL or "" for OG (no overlay). The name is used verbatim as the folder
// name; case does not matter, since Windows is case-insensitive and
// plat_normalize_path resolves components case-insensitively elsewhere.
void         SetAssetMode(const char* mode);
// The active overlay's folder name, "" when none (OG).
const char*  GetAssetModeName(void);

// Rewrites whichever known asset-root form a path was compiled with (debug
// ".\assets\USA\" or retail ".\usa\") to the current runtime root, writing the
// result into `out` (outSize bytes) and returning `out`. Returns `path`
// unchanged when no rewrite is needed (non-rooted path, or out of room). The
// rewrite is idempotent: the configured root never equals a recognized form, so
// running the resolved path through again is a no-op.
const char*  ResolveAssetRoot(const char* path, char* out, size_t outSize);

#ifdef __cplusplus
}
#endif
