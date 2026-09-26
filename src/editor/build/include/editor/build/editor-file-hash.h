#pragma once

/// @file editor-file-hash.h
/// @brief A hash of a folder's files: which ones, and what is in them.
/// @par Threading Main-thread-only (reads the disk).

#include <cstdint>
#include <filesystem>
#include <string>

namespace eng::editor {

/// Which files under a folder count, by their path relative to it, with
/// `/` between its parts.
using EditorFileFilter = bool (*)(const std::string& relative);

/// A hash of every file under @p root that @p keep counts: their relative
/// paths and bytes, in path order, so it is the same on every machine that
/// has the same files. 64-bit FNV-1a — to tell builds and content apart,
/// not to resist tampering.
[[nodiscard]] uint64_t hashFileTree(const std::filesystem::path& root,
                                    EditorFileFilter keep);

/// Whether @p relative names a hidden file or lies in a hidden folder — a
/// Finder's `.DS_Store`, an editor's swap file — which differ from machine
/// to machine and are never part of what is hashed.
[[nodiscard]] bool editorPathHidden(const std::string& relative);

}  // namespace eng::editor
