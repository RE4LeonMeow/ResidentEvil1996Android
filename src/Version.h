// Version.h - Port version, compiled into the executable
//
// The decomp itself has no version - the original binary carries none, and
// nothing in the game data is keyed to one. This is purely a port-side build
// stamp, so a bug report ("F1 says VER 0.3.1") names an exact commit range.
//
// The version lives in exactly ONE place: the annotated block below. Merging
// a release PR rewrites it through release-please's "generic" updater (see
// extra-files in release-please-config.json) in the same commit that gets
// tagged, so any build made from main reports the version it was tagged
// with. Between releases a local build reports the last RELEASED version,
// not a future one - the number only moves when a release is cut.
//
// Do not hand-edit the string, and do not reformat the comment markers: the
// updater rewrites every full semver it finds between the start/end lines,
// and matches them by those literal comments.
#pragma once

// x-release-please-start-version
#define GAME_VERSION_STRING "1.3.0"
// x-release-please-end
