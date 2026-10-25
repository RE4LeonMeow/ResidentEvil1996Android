// AssetPath.cpp - Runtime asset-root selection
//
// The original hardcodes a data root in front of every asset path. That root
// differs by region: the North American/GOG version ships a "usa" tree and the
// Japanese PC (Biohazard) version a "jpn" tree. The path templates themselves
// are identical, so the only thing that changes is the root folder.
//
// GAME_DATA_ROOT (see AssetPath.h) is a compile-time macro that stands in for the
// original's GAME_DATA_ROOT; it is baked into string literals and the static
// path templates. That remains the USA default. This translation unit adds a
// runtime override selected from config.ini [Assets] Version so the same build
// can point its readers at the JPN tree without recompiling.
//
// Note on idempotence: the previous ResolveAssetPath rewrite scanned for a
// "\usa\" component and spliced in "\assets\USA\", which double-applied because
// the result still contained "\USA\". ResolveAssetRoot instead substitutes the
// recognized root FORM for the configured root, and the configured root is never
// itself a recognized form, so a second pass cannot match.

#include "AssetPath.h"
#include "../platform/types.h"   // sprintf_s
#include "../platform/platform.h" // plat_normalize_path
#include "../DebugPrint.h"        // dbg_printf
#include <stdio.h>
#include <string.h>

// The configured data root (folder + trailing separator). Defaults to the
// compile-time USA root so release behaviour is unchanged unless config.ini
// selects JPN.
#if !defined(_WIN32)
#define ROOT_USA "./assets/USA/"
#define ROOT_JPN "./assets/JPN/"
#elif defined(_DEBUG)
#define ROOT_USA ".\\assets\\USA\\"
#define ROOT_JPN ".\\assets\\JPN\\"
#else
#define ROOT_USA ".\\usa\\"
#define ROOT_JPN ".\\jpn\\"
#endif

#if defined(_WIN32)
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

// Folder holding the USA/ and JPN/ trees (config.ini [Assets] Path). Empty
// means "not configured": the compile-time roots above stay in force.
static char s_base[240] = "";
// Composed root ("<base>/<region>/") used when a base is configured.
static char s_assetRootBuf[260] = "";
const char* s_assetRoot = ROOT_USA;
// Explicit save folder (config.ini [Save] Path). Empty means "<base>/SAVE/".
static char s_saveRoot[260] = "";

// Version is tracked as an int, not by comparing s_assetRoot's pointer against
// a string literal. Debug builds compile without string pooling (/GF is off),
// so two ".\assets\JPN\" literals have different addresses and a pointer
// comparison would report USA even when JPN is selected. Content comparison is
// the safe alternative, but an explicit flag is unambiguous.
static int s_assetIsJpn = 0;

// Content-mode overlay (config.ini [Game] Mode), searched BEFORE the base tree.
// An empty name / NULL root means OG: no overlay, and ResolveAssetRoot then
// does exactly what it did before this existed - no probe, no extra cost.
static char s_modeName[16] = "";
static char s_overlayRootBuf[260] = "";
static const char* s_overlayRoot = NULL;

// Which top-level folders a non-OG mode is allowed to take from the BASE tree.
//
// The intent is that a mode's own tree is authoritative for its content, so
// that OG stays vanilla and nothing silently mixes the two. These five are the
// exceptions, and they are not a policy choice - the Director's Cut disc simply
// has nothing to put there:
//
//   sound, voice  the PS1 keeps its samples in VAB banks and streams speech as
//                 XA audio. There is no per-effect WAV to copy: 547 of the PC
//                 tree's Sound names and 563 of its Voice names have no DC
//                 counterpart at all. The DC's audio is in any case identical
//                 to the USA release's.
//   movie         PS1 .STR, which the port cannot play, and 8 of the PC's 27
//                 movies have no DC counterpart.
//   effspr        PC-only asset class; the DC disc has no such folder.
//   objspr        PC-only asset class; the DC disc has no such folder.
//
//   item_m2/filem_, item_m2/arror
//                 the readable documents. The DC packs all 46 pages into one
//                 FILEM.PIX at a 51200 stride - and every page of it is
//                 BYTE-IDENTICAL to a PS1 OG page, so the DC adds no document
//                 art whatever. The PC's per-file containers hold the same
//                 pictures in a format this build already reads, so falling
//                 back to them loses nothing. (ARROR.TIM is the PC's name for
//                 the DC/PS1 FILEMARR.PIX.)
//
// Everything else - stage rooms, enemy and player models, item art, data - the
// DC does own, so a miss there means the mode tree is INCOMPLETE and the player
// is about to see OG content in a DC session. That is exactly the failure this
// list exists to make visible, so it is logged rather than passed over. Data,
// Item_m2 and Players are only partly covered; those log too, and the log is
// the record of what a mode tree still owes.
//
// Entries are path PREFIXES, matched case-insensitively with either separator.
// A folder name is written with its trailing separator so that "sound/" cannot
// also match a "soundtrack/" that someone adds later.
static const char* const kBaseFallbackPrefixes[] = {
    "sound/", "voice/", "movie/", "effspr/", "objspr/",
    "item_m2/filem_", "item_m2/arror",
};
static const int kBaseFallbackCount =
    (int)(sizeof(kBaseFallbackPrefixes) / sizeof(kBaseFallbackPrefixes[0]));

// One line the FIRST time each top-level folder resolves, saying which tree it
// came from. The miss log below only fires for a folder the mode is supposed to
// own, and says nothing at all when the overlay is working - so "is my overlay
// live?" and "where did the rooms come from?" had no answer in the log. This
// gives both, at about a dozen lines a session.
static char s_logged[24][20];
static int  s_loggedCount = 0;

static void LogOverlayDecision(const char* tail, int hit)
{
    char key[20];
    int n = 0;
    while (tail[n] != '\0' && tail[n] != '/' && tail[n] != '\\' &&
           n < (int)sizeof(key) - 1) {
        char c = tail[n];
        key[n] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        n++;
    }
    key[n] = '\0';

    for (int i = 0; i < s_loggedCount; i++) {
        if (strcmp(s_logged[i], key) == 0) return;
    }
    if (s_loggedCount < (int)(sizeof(s_logged) / sizeof(s_logged[0]))) {
        strcpy(s_logged[s_loggedCount++], key);
    }
    dbg_printf("[assets] %s/%s -> %s\n", s_modeName, key,
               hit ? "overlay" : "BASE TREE");
}

static int FallbackIsExpected(const char* tail)
{
    for (int i = 0; i < kBaseFallbackCount; i++) {
        const char* a = tail;
        const char* b = kBaseFallbackPrefixes[i];
        while (*b != '\0') {
            char ca = *a;
            char cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
            if (ca == '\\') ca = '/';          // either separator matches
            if (ca != cb) break;
            a++;
            b++;
        }
        if (*b == '\0') {
            return 1;
        }
    }
    return 0;
}

// Rebuild s_assetRoot from the configured base + region. With no base the
// compile-time root is kept, which also keeps every static path template (they
// are compiled against it and patched by character index) valid.
static void ComposeAssetRoot(void)
{
    if (s_base[0] == '\0') {
        s_assetRoot = s_assetIsJpn ? ROOT_JPN : ROOT_USA;
        return;
    }
    sprintf_s(s_assetRootBuf, sizeof(s_assetRootBuf), "%s%c%s%c",
              s_base, PATH_SEP, s_assetIsJpn ? "JPN" : "USA", PATH_SEP);
    s_assetRoot = s_assetRootBuf;
}

// Rebuild the overlay root from the configured base + mode name. Mirrors the
// shape of the base root, so an install with no [Assets] Path gets ".\dc\"
// beside the executable in retail and "./assets/DC/" in a dev build, exactly as
// the base tree is ".\usa\" / "./assets/USA/".
static void ComposeOverlayRoot(void)
{
    if (s_modeName[0] == '\0') {
        s_overlayRoot = NULL;
        return;
    }
    if (s_base[0] != '\0') {
        sprintf_s(s_overlayRootBuf, sizeof(s_overlayRootBuf), "%s%c%s%c",
                  s_base, PATH_SEP, s_modeName, PATH_SEP);
    } else {
#if !defined(_WIN32)
        sprintf_s(s_overlayRootBuf, sizeof(s_overlayRootBuf),
                  "./assets/%s/", s_modeName);
#elif defined(_DEBUG)
        sprintf_s(s_overlayRootBuf, sizeof(s_overlayRootBuf),
                  ".\\assets\\%s\\", s_modeName);
#else
        sprintf_s(s_overlayRootBuf, sizeof(s_overlayRootBuf),
                  ".\\%s\\", s_modeName);
#endif
    }
    s_overlayRoot = s_overlayRootBuf;
}

void SetAssetMode(const char* mode)
{
    if (mode == NULL || mode[0] == '\0') {
        s_modeName[0] = '\0';
    } else {
        strncpy(s_modeName, mode, sizeof(s_modeName) - 1);
        s_modeName[sizeof(s_modeName) - 1] = '\0';
    }
    ComposeOverlayRoot();
}

const char* GetAssetModeName(void)
{
    return s_modeName;
}

void SetAssetBase(const char* base)
{
    if (base == NULL || base[0] == '\0') {
        s_base[0] = '\0';
        ComposeAssetRoot();
        ComposeOverlayRoot();
        return;
    }

    strncpy(s_base, base, sizeof(s_base) - 1);
    s_base[sizeof(s_base) - 1] = '\0';
    // Drop a trailing separator so composition is uniform.
    size_t n = strlen(s_base);
    while (n > 1 && (s_base[n - 1] == '/' || s_base[n - 1] == '\\')) {
        s_base[--n] = '\0';
    }
    ComposeAssetRoot();
    ComposeOverlayRoot();
}

const char* GetAssetBase(void)
{
    return s_base;
}

void SetSaveRoot(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        s_saveRoot[0] = '\0';
        return;
    }

    strncpy(s_saveRoot, path, sizeof(s_saveRoot) - 1);
    s_saveRoot[sizeof(s_saveRoot) - 1] = '\0';
    size_t n = strlen(s_saveRoot);
    if (n + 1 < sizeof(s_saveRoot) &&
        s_saveRoot[n - 1] != '/' && s_saveRoot[n - 1] != '\\') {
        s_saveRoot[n] = PATH_SEP;
        s_saveRoot[n + 1] = '\0';
    }
}

const char* GetSaveRoot(void)
{
    // The folder is "SAVE" (what the original kept next to the executable), but
    // the path is run through the platform layer so an existing "save" or
    // "Save" directory is found as well - save reads are plain fopen, not
    // LoadFile, so without this a copied Windows SAVE folder was invisible on a
    // case-sensitive filesystem while writes (plat_file_write, which does
    // resolve) went somewhere else. Returned buffer is a shared static, like the
    // default it replaces: callers build a name with it immediately.
    static char s_resolved[260];
    const char* raw;

    if (s_saveRoot[0] != '\0') {
        raw = s_saveRoot;
    } else if (s_base[0] == '\0') {
        raw = GAME_SAVE_ROOT;   // nothing configured
    } else {
        static char s_defaultSave[260];
        sprintf_s(s_defaultSave, sizeof(s_defaultSave), "%s%cSAVE%c",
                  s_base, PATH_SEP, PATH_SEP);
        raw = s_defaultSave;
    }

    plat_normalize_path(raw, s_resolved, sizeof(s_resolved));
    return s_resolved;
}

void SetAssetVersion(const char* version)
{
    if (version != NULL &&
        (version[0] == 'J' || version[0] == 'j') &&
        (version[1] == 'P' || version[1] == 'p')) {
        s_assetIsJpn = 1;
    }
    else {
        s_assetIsJpn = 0;
    }
    ComposeAssetRoot();
}

const char* GetAssetRoot(void)
{
    return s_assetRoot;
}

int GetAssetVersion(void)
{
    return s_assetIsJpn;
}

const char* ResolveAssetRoot(const char* path, char* out, size_t outSize)
{
    if (path == NULL) return NULL;
    if (out == NULL || outSize == 0) return path;

    // Recognized root forms. Both compile-time variants are always matched so
    // a path built against either the debug ("assets") or retail root is
    // remapped, e.g. the FMV table which hardcodes the retail ".\usa\" form.
    // On non-Windows the configured roots are themselves in the list; the
    // substitution is idempotent for them (root -> same root).
#if !defined(_WIN32)
    static const char* kForms[] = { "./assets/USA/", "./assets/JPN/",
                                    ".\\usa\\", ".\\jpn\\" };
    const int kFormCount = 4;
#else
    // The JPN forms belong here too: GAME_DATA_ROOT_JPN paths (the JPN-only
    // document art in MainMenu.cpp) are compiled against them, and without a
    // match this function returns early - so those loads skipped the overlay
    // probe entirely on Windows, while the same paths resolved on Linux,
    // whose list always carried all four.
    static const char* kForms[] = { ".\\assets\\USA\\", ".\\usa\\",
                                    ".\\assets\\JPN\\", ".\\jpn\\" };
    const int kFormCount = 4;
#endif

    const char* match = NULL;
    size_t matchLen = 0;
    for (int i = 0; i < kFormCount; i++) {
        size_t len = strlen(kForms[i]);
        if (len > matchLen && strncmp(path, kForms[i], len) == 0) {
            matchLen = len;
            match = kForms[i];
        }
    }
    if (match == NULL) return path; // Not an asset-rooted path.

    const char* tail = path + matchLen;

    // Content-mode overlay first (config.ini [Game] Mode). A non-OG mode ships
    // only the files it changes or adds, so a miss here is the normal case and
    // falls through to the base tree below - which is what keeps the base tree
    // authoritative for everything the mode did not touch, and what lets
    // several releases live in one install.
    //
    // The probe has to go through plat_normalize_path: on a case-sensitive
    // filesystem the component case is resolved there, not here, so probing the
    // raw path would miss an overlay stored as "assets/dc/stage1/...". The
    // UNnormalized overlay path is what gets returned, because every caller
    // normalizes the result itself (see LoadFile) and doing it here too would
    // just be redundant work.
    if (s_overlayRoot != NULL) {
        char candidate[260];
        int c = sprintf_s(candidate, sizeof(candidate), "%s%s", s_overlayRoot, tail);
        if (c >= 0) {
            char probeBuf[260];
            const char* probe = plat_normalize_path(candidate, probeBuf, sizeof(probeBuf));
            FILE* fp = fopen(probe, "rb");
            if (fp != NULL) {
                fclose(fp);
                int m = sprintf_s(out, outSize, "%s%s", s_overlayRoot, tail);
                if (m >= 0) {
                    LogOverlayDecision(tail, 1);
                    return out;
                }
            }
            // Falling through to the base tree. For the folders a mode does not
            // own that is normal and silent; anywhere else it means this mode's
            // tree is missing a file it should have supplied, and the game is
            // about to load OG content in a non-OG session.
            else {
                LogOverlayDecision(tail, 0);
                if (!FallbackIsExpected(tail)) {
                    dbg_printf("[assets] %s has no %s - falling back to the base tree\n",
                               s_modeName, tail);
                }
            }
        }
    }

    int n = sprintf_s(out, outSize, "%s%s", s_assetRoot, tail);
    if (n < 0) return path;         // Would not fit: keep the original path.
    return out;
}
