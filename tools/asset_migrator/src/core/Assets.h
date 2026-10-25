#pragma once
// PC asset migration: locate the asset folders in a folder or a disc image and
// copy/extract them into a USA/JPN tree.

#include "core/DiscImage.h"
#include "core/Types.h"

#include <string>
#include <vector>

namespace re1 {

// Find the directory under `dir` that holds the asset folders (the directory
// itself, an `assets` child, or a single level down). False when none is found.
bool findAssetRootInDir(const std::string& dir, std::string* root,
                        std::string* error);

// Copy every asset folder from `srcRoot` into `destRoot/<canonical name>/`.
bool copyAssetsFromDir(const std::string& srcRoot, const std::string& destRoot,
                       const Progress& progress, std::string* error);

// Extract the named folders (top level, case-insensitive) from the image into
// `destRoot` under their own names.
bool extractFoldersFromImage(const DiscImage& img,
                             const std::vector<std::string>& folders,
                             const std::string& destRoot,
                             const Progress& progress, std::string* error);

// Find the image directory holding the most asset folders; returns its path
// ("" for the root) in `prefix`.
bool findAssetPrefixInImage(const DiscImage& img, std::string* prefix,
                            std::string* error);

// Extract every asset folder from the image into `destRoot/<canonical name>/`.
bool extractAssetsFromImage(const DiscImage& img, const std::string& destRoot,
                            const Progress& progress, std::string* error);

}  // namespace re1
