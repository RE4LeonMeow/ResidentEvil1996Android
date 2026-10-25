#pragma once
// Entry points for the two migrations the GUI drives.

#include "core/Types.h"

#include <string>

namespace re1 {

struct PcMigrationOptions {
    // Either an already-extracted folder or a disc image, per sourceIsImage.
    std::string sourcePath;
    bool sourceIsImage = false;
    // Game folder the USA/ or JPN/ tree is written under.
    std::string targetRoot;
    AssetVersion version = AssetVersion::USA;
    // Convert every .avi in the migrated Movie folder to .mp4 (H.264 + AAC).
    bool convertMovies = false;
    // Keep the source .avi next to the generated .mp4 (the engine prefers the
    // .mp4 and falls back to the .avi).
    bool keepAvi = true;
    // ffmpeg executable; resolved from PATH when it is a bare name.
    std::string ffmpegPath = "ffmpeg";

    // Optional PS1 disc image (the 1996 release or the Director's Cut) whose
    // PS1-only assets are added to the same USA/JPN tree, for the PS1 ending
    // credits in OG mode ([Game] Ps1EndingCredits=1). A raw .bin/.cue keeps
    // the movies' CD-XA audio.
    std::string ps1ImagePath;
    // Copy DATA/STAFF.STF, STAFF2.STF and BIO.TIM into <tree>/Data. The
    // tree's own EN05/EN07/CLIS01/JILL01 are left alone.
    bool ps1Credits = true;
    // Convert the disc's .STR movies to .mp4 in <tree>/Movie: always the
    // credits movies STFC/STFJ, and any other movie the tree has no version
    // of (under its PC name).
    bool ps1Movies = true;
    // Also overwrite the tree's own movies with the PS1 versions, and
    // re-convert STFC/STFJ when they already exist.
    bool ps1ReplaceMovies = false;
    // Decode the disc's DATA/JIMAKU*.RGB into <tree>/Data/jimaku*.png, the
    // prologue FMV subtitles ([Game] Ps1FmvSubtitles=1). Only the JPN releases
    // carry them; a disc without them is skipped, not an error.
    bool ps1FmvSubtitles = true;
    // Replace the tree's sound effects, voices and BGM with the disc's (the
    // files in <tree>/Sound and <tree>/Voice, under the PC's names). Every
    // name the PS1 cannot supply keeps the tree's file; everything replaced is
    // listed in Sound/PS1AUDIO.TXT. A later run with a PC source restores the
    // PC files. Off by default.
    bool ps1Audio = false;
    bool ps1AudioSfx = true;
    bool ps1AudioVoices = true;
    bool ps1AudioBgm = true;
    // WAV, or Ogg Vorbis at ps1AudioOggQuality (needs ffmpeg with libvorbis).
    bool ps1AudioOgg = false;
    int ps1AudioOggQuality = 6;
};

struct DcMigrationOptions {
    // The Director's Cut disc image (.bin/.cue/.iso). A raw 2352-byte image is
    // required for clean CD-XA audio in the movie conversion.
    std::string imagePath;
    // Game folder the DC/ overlay is written under.
    std::string targetRoot;
    // Base tree the overlay is built on top of.
    AssetVersion base = AssetVersion::USA;
    Backgrounds backgrounds = Backgrounds::All;
    // Convert the disc's .STR movies to .mp4 into the overlay's Movie/ folder.
    bool convertMovies = false;
    // Run the coverage/background check after the overlay is written.
    bool verify = true;
    std::string ffmpegPath = "ffmpeg";
    // Decode the disc's DATA/JIMAKU*.RGB into <target>/JPN/Data/jimaku*.png,
    // the prologue FMV subtitles ([Game] Ps1FmvSubtitles=1). They go into the
    // JPN tree, not the overlay: Ps1FmvSubtitles builds its path from the
    // compile-time GAME_DATA_ROOT_JPN and calls LoadFile directly, so the
    // overlay's Data folder is never searched for them. Only the JPN discs
    // carry the files; a disc without them is skipped, not an error.
    bool fmvSubtitles = true;
};

// migratePcAssets runs the base-tree copy when sourcePath is set, then the PS1
// supplement when ps1ImagePath is set; at least one of the two is required.
// Both return true on success; on failure `error` carries a human-readable
// message. Cancellation through `progress.cancelled` returns false with
// "cancelled" as the error.
bool migratePcAssets(const PcMigrationOptions& opts, const Progress& progress,
                     std::string* error);

bool migrateDcAssets(const DcMigrationOptions& opts, const Progress& progress,
                     std::string* error);

}  // namespace re1
