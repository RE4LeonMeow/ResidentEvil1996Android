// ConfigFile.h - config.ini as the single settings store (both builds).
//
// Windows used to keep the player's settings in HKCU\Software\CAPCOM\RESIDENT
// EVIL; this module replaces that with the same file the port already reads as
// a dev override, so settings travel between the Windows and Linux builds and
// nothing lands in a tracked file (config.ini is gitignored now).
//
//   - ConfigFile_Load()      reads [Display]/[Player]/[Input] (plus the
//                            [Assets] and [Debug] dev knobs) into the globals.
//   - ConfigFile_Save()      rewrites ONLY the keys the game owns, in place:
//                            comments, unknown keys and section order survive.
//   - ConfigFile_EnsureExists() writes the documented default file when it is
//                            missing, so a fresh install has something to edit.
//
// The Windows registry is still READ (system/Installation.cpp) before this,
// so an existing install keeps its bindings until the first save; it is no
// longer written.
#pragma once
#include "../platform/types.h"

// Locate config.ini: current directory first, then the parent directories the
// Windows build has always searched. Returns the path to use for writing.
const char* ConfigFile_Find(void);

// Write the default config.ini if none exists.
void ConfigFile_EnsureExists(void);

// Read the file into the game globals. FALSE if there is no file.
BOOL ConfigFile_Load(void);

// Write the current settings back, preserving everything else in the file.
void ConfigFile_Save(void);
