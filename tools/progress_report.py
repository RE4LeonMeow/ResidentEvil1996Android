#!/usr/bin/env python3
"""Progress report: how many original-game functions are implemented in src/.

Usage:
  1. Dump the Ghidra function entry list to a text file, one per line:
       "<name> at 00441350"   (e.g. paste from MCP list_functions, or a
       headless script using getFunctions(true))
  2. Run:  python tools/progress_report.py <dumped_list.txt> [src_dir]

It extracts every hex address from the dump (function entry points), then
scans all source files for those same hex tokens. A source hit means the
function is implemented (or at least referenced by its original address).

Functions are classified into work streams:
  - import-thunk      : DLL imports / ICF thunks (provided by loader)
  - crt-out-of-scope  : static MSVC CRT/runtime + SEH helpers (toolchain)
  - marni-dx11        : Marni System DirectX internals pending DX11 port
  - game-logic        : everything else (the real decomp work)
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

ADDR_RE = re.compile(r"\b(?:0x)?([0-9A-Fa-f]{8})\b")
SRC_EXTS = {".cpp", ".h", ".c", ".hpp"}

# --- out-of-scope classification -------------------------------------------
# Static MSVC CRT / runtime library names found in the binary.
CRT_NAME = re.compile(
    r"^(__|___|_std_|FID_conflict|operator_new|operator_delete|"
    r"free$|malloc$|memcpy$|memset|memclr|printf$|srand$|^_rand$|"
    r".*ExceptionHandler$|.*Unwind.*|.*GuardHandler$|__chkstk$|__ftol$|"
    r"__fpmath$|__flsbuf$|__isctype|__close$|__openfile|__getstream|"
    r"__write$|__read$|__lseek$|__ioinit|cvtdate|_gmtime$|__tzset|___tzset|"
    r"__isindst|__ftime$|__XcptFilter$|xcptlookup|__ismbblead|"
    r"_JumpToContinuation|_CallMemberFunction|_CallCatchBlock2|"
    r"_CallSETranslator|TranslatorGuardHandler|CatchGuardHandler|"
    r"__cintrindisp|__ctrandisp|__fload$|__global_unwind2|__local_unwind2|"
    r"__abnormal_termination|__msize_dbg|_CheckBytes|__CrtCheckMemory|"
    r"__CrtSetDbgFlag|__CrtIsValid|__CrtMem|__printMemBlockData|"
    r"__CrtDumpMemoryLeaks|__CrtDbgBreak|__CrtDbgReport|_CrtMessageWindow|"
    r"write_char$|write_multi_char$|write_string$|get_int_arg$|"
    r"get_int64_arg$|get_short_arg$|__freebuf$|_fflush$|__flush|flsall$|"
    r"__filbuf$|__output$|___crtLCMapStringA|_strncnt$|__cfltcvt_init|"
    r"^_isalpha$|^_sprintf$|^_strrchr$|^_vsprintf$|^_fclose$|^_fread$|"
    r"^_fseek$|^_ftell$|^_fopen$|^strchr$|^_asctime$|^store_dt$|"
    r"^_localtime$|^_time$|^_exit$|^doexit$|^_atexit$|^realloc_help|"
    r"eh_vector_destructor_iterator|eh_vector_constructor_iterator|"
    r"^FindHandler$|^FindHandlerForForeignException$|"
    r"^GetRangeOfTrysToCheck$|^TypeMatch$|^CatchIt$|^CallCatchBlock$|"
    r"^ExFilterRethrow$|^BuildCatchObject$|^DestructExceptionObject$|"
    r"^AdjustPointer$|^_inconsistency$)"
)
# DirectShow/Win32 API thunk wrappers statically linked into the exe.
IMPORT_THUNK_NAME = re.compile(
    r"^(DirectDrawCreate|DirectDrawEnumerateA|DirectSoundCreate|acmMetrics|"
    r"ImmAssociateContext|findAndOpenFile)"
)
# Compiler-generated SEH glue / destructor thunks. Their semantics are fully
# absorbed by the real C++ constructors/destructors already present in src\
# (e.g. ~PSXTexture implements the _eh_vector_destructor_iterator_ loop).
SEH_GLUE_NAME = re.compile(
    r"(_DestructorThunk\d*$|Direct3DTMD_SEH_Handler_|"
    r"CMarniExecuteBuffer_SEH_Handler_|TriangleDivide_SEH_|"
    r"PSXTexture_SEH_|^PSXTexture_VectorDestructorIter$|"
    r"^PSXTexture_CallDestructorBody$|"
    r"^(TextureArray_|TextureElement_|ViewportArray_|ViewportElement_|"
    r"PageTableArray_|PageTableElement_|GlobalMarniBits_|GlobalViewport_)"
    r"(Init|ConstructElements|RegisterCleanup|Cleanup|Constructor|"
    r"Destructor|Clear|Destroy)?$|"
    r"^CMarniViewport2_Convert0_(SEH|Cleanup)$|"
    r"^viewport_release_texture$|^sound_seh_frame_handler$|"
    r"^empty_[0-9a-f]+$|^stub_return_|^dead_|^complex_obj_element_|"
    r"^(tmd_element_|tmd_cleanup_|pagetable_|texture_elem_|viewport_elem_|"
    r"texpage_|keymap_|trig_table_|font_ctx_|charselect_seh_)"
    r"(frame_handler(_[abc])?|dtor_tail|clear_tail|ctor_seh|dtor_seh|dtor_body|"
    r"array_cleanup|static_init|construct_all?|register_cleanup|cleanup|"
    r"init_step|element_ctor)$|"
    r"^texpage_\w+$|^pagetable_\w+$|^texture_elem_\w+$|"
    r"^viewport_elem_\w+$|^tmd_element_\w+$|^tmd_cleanup_\w+$|"
    r"^keymap_\w+$|^trig_table_\w+$|^font_ctx_\w+$|"
    r"^d3dobj_(ctor_frame_handler|dtor_tail)$|"
    r"^bits_(seh_frame_handler|dtor_tail)_|^purecall_base_|"
    r"^door_anim_viewport_dtor_tail$|^shadowmask_|^d3d_ctor_frame_handler_b$|"
    r"^d3d_ctor_purecall_tail$|"
    r"^(viewport_pool_|tmd_pool_|complex_obj\d?_|tmd_pair_|tmd_single_|"
    r"door_complex_obj_|door_tmd_array_|menu_complex_obj_|"
    r"menu_tmd_single_|viewport_arr\d_|effect_viewport_arr_|"
    r"object_list_viewport_arr_|viewport_ctor_|viewport_dtor_|"
    r"sound_seh_frame_handler)"
    r"(static_init|construct_all|construct|register_cleanup|cleanup|"
    r"seh|frame_handler|tail_a|tail_b|release_texture)?$)"
)
# Raw DirectX 5 / Direct3D API plumbing (device creation, Z-buffer attach,
# texture-format enumeration, error printing). Superseded by the MarniDX
# DX11 layer (adapter enumeration, device, textures).
D3D5_NAME = re.compile(
    r"^(d3d|md3d_|print_d3d_error|PrintDirectDrawError|ddraw|bits_fill_rect_|"
    r"dispres_|directfont_|ent_setanim_walkto_helper|"
    r"d3dsurface_|GetSoundBufferStatus|gte_table_|bits_triangle_|bits_sprite_|"
    r"bits_gouraud_|bits_gradient_|ot_draw_primitive|d3d_transalpha|wave_)"
)
# The original launched an external software renderer process and talked to it
# through shared memory for FMVs; the port plays them natively via MCI
# (src/video/VideoPlayback.cpp).
SOFTWARE_FMV_NAME = re.compile(
    r"^(LaunchSoftwareVideoPlayer|CopyStringToSharedMemory|IsVideoPlayerReady|"
    r"ClearVideoPlaybackFlag|CopyVolumeToSharedMemory|CacheWaveOutVolumeAndMute|"
    r"^video_mci_|^UpdateVideoWindow$)"
)
# Marni System / DirectX wrapper classes -> DX11 port stream.
MARNI_NAME = re.compile(
    r"^(CMarni|Marni|Direct3DTMD_|Direct3DObject_|TriangleDivide|"
    r"PSXTexture_SEH|PSXTexture_Vector|PSXTexture_Construct|"
    r"PSXTexture_Destroy|PSXTexture_Call|GlobalMarniBits|GlobalViewport|"
    r"ViewportArray_|ViewportElement|PageTableArray_|PageTableElement|"
    r"TextureArray_|TextureElement|Async_CreateTexture|Async_CreateObject|"
    r"Async_DeleteObject|VideoDriver_|GetDirect3DDriver|GetDisplayMode|"
    r"CreateLights)"
)
# CRT-suspect unnamed region: static CRT + thunks live above this address.
CRT_UNNAMED_RANGE = (0x00498000, 0x00500000)


def classify(name, addr):
    """Return (category, note) for a Ghidra function."""
    if addr >= 0x00990000:
        return "import-thunk", "DLL import"
    n = name.strip()
    if addr >= 0x004A0000:
        return "crt-out-of-scope", "LIBC region (whole 0x004A page)"
    if IMPORT_THUNK_NAME.match(n):
        return "import-thunk", ""
    if "eh_vector_" in n or CRT_NAME.match(n):
        return "crt-out-of-scope", ""
    if SEH_GLUE_NAME.search(n):
        return "seh-glue", "compiler unwind glue (absorbed by real C++)"
    if D3D5_NAME.match(n):
        return "d3d5-superseded", "raw D3D5 API path replaced by MarniDX"
    if SOFTWARE_FMV_NAME.match(n):
        return "software-fmv-superseded", "replaced by native MCI FMV playback"
    lo, hi = CRT_UNNAMED_RANGE
    if n.startswith("FUN_") and lo <= addr < hi:
        return "crt-out-of-scope", "unnamed in CRT region (verify!)"
    if MARNI_NAME.match(n):
        return "marni-dx11", ""
    return "game-logic", ""


def load_ghidra_entries(dump_path):
    addrs = {}
    text = Path(dump_path).read_text(encoding="utf-8", errors="replace")
    for line in text.splitlines():
        m = re.search(r"\bat\s+((?:0x)?[0-9A-Fa-f]{6,8})\s*$", line)
        if not m:
            continue
        addr = int(m.group(1).replace("0x", ""), 16)
        name = line.rsplit(" at ", 1)[0].strip()
        addrs[addr] = name
    return addrs


def scan_source(src_dir, ghidra_addrs):
    hits = set()
    files_by_addr = defaultdict(set)
    for p in sorted(Path(src_dir).rglob("*")):
        if p.suffix.lower() not in SRC_EXTS:
            continue
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for m in ADDR_RE.finditer(text):
            val = m.group(1)
            addr = int(val, 16)
            if addr in ghidra_addrs:
                hits.add(addr)
                files_by_addr[addr].add(p.name)
    # second pass: exact function-name matches (catches rewrites annotated
    # by name instead of address, e.g. table-driven dispatchers)
    for p in sorted(Path(src_dir).rglob("*")):
        if p.suffix.lower() not in SRC_EXTS:
            continue
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for addr, name in ghidra_addrs.items():
            if addr not in hits and name and len(name) > 3 and name != "_":
                if re.search(r"\b%s\b" % re.escape(name), text):
                    hits.add(addr)
                    files_by_addr[addr].add(p.name + " (name)")
    return hits, files_by_addr


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    dump_path = sys.argv[1]
    src_dir = sys.argv[2] if len(sys.argv) > 2 else "src"

    ghidra = load_ghidra_entries(dump_path)

    cats = defaultdict(list)   # category -> [(addr, name)]
    for a, n in sorted(ghidra.items()):
        cats[classify(n, a)[0]].append((a, n))

    hits, files = scan_source(src_dir, ghidra)

    W = 52
    print("=" * W)
    print(" Resident Evil PC decomp - progress report")
    print("=" * W)
    print(f" Total functions in Ghidra project : {len(ghidra)}")
    print()
    print(f" {'Stream':<38}{'total':>6}{'left':>6}")
    total_left_game = 0
    for cat in ("game-logic", "marni-dx11", "seh-glue", "d3d5-superseded",
                "software-fmv-superseded", "crt-out-of-scope", "import-thunk"):
        entries = cats.get(cat, [])
        left = [e for e in entries if e[0] not in hits]
        done = len(entries) - len(left)
        pct = 100.0 * done / len(entries) if entries else 100.0
        label = cat
        if cat == "import-thunk":
            label += " (n/a)"
        elif cat in ("crt-out-of-scope", "seh-glue"):
            label += " (out of scope)"
        elif cat == "d3d5-superseded":
            label += " (replaced by MarniDX)"
        elif cat == "software-fmv-superseded":
            label += " (replaced by native MCI)"
        print(f" {label:<38}{len(entries):>6}{len(left):>6}"
              f"   [{done}/{len(entries)} = {pct:.0f}%]")
        if cat in ("game-logic", "marni-dx11"):
            total_left_game += len(left)

    scope_total = sum(len(cats[c]) for c in ("game-logic", "marni-dx11"))
    scope_done = scope_total - total_left_game
    pct_all = 100.0 * scope_done / scope_total if scope_total else 100.0
    print("-" * W)
    print(f" IN-SCOPE TOTAL                    {scope_total:>6}"
          f"{total_left_game:>6}   [{scope_done}/{scope_total}"
          f" = {pct_all:.1f}%]")
    print()

    pages = defaultdict(int)
    for a, _ in cats["game-logic"]:
        if a not in hits:
            pages[a >> 16] += 1
    if pages:
        print(" Remaining game logic by region:")
        for page in sorted(pages):
            print(f"   0x{page << 16:08X}-0x{page << 16 | 0xFFFF:08X}: "
                  f"{pages[page]:4d}")

    with Path("progress_remaining.txt").open("w", encoding="utf-8") as f:
        f.write("# Not yet referenced in source\n")
        for cat in ("game-logic", "marni-dx11"):
            f.write("\n# === %s ===\n" % cat)
            for a, n in cats[cat]:
                if a not in hits:
                    f.write("%08X %s\n" % (a, n))
        f.write("\n# === crt / seh-glue / d3d5-superseded / fmv-superseded / imports ===\n")
        for cat in ("seh-glue", "d3d5-superseded", "software-fmv-superseded",
                    "crt-out-of-scope", "import-thunk"):
            for a, n in cats[cat]:
                if a not in hits:
                    f.write("%08X [%s] %s\n" % (a, cat, n))
    print("\n Full remaining list written to: "
          f"{Path('progress_remaining.txt').resolve()}")


if __name__ == "__main__":
    main()
