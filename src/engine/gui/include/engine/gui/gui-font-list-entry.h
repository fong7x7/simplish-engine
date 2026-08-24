#pragma once

/// @file gui-font-list-entry.h
/// @brief One discoverable font (family, path, bundled flag).
/// See docs/technical-approaches/editor/font-settings.md §1.

#include <string>

namespace eng {

/// A font file discovered on disk (bundled or system search paths).
/// @thread_safety Main thread only.
struct GuiFontListEntry {
  /// Display family name (typically derived from filename stem).
  std::string family{};
  /// Path to the .ttf or .otf file.
  std::string file_path{};
  /// True when found under the caller-supplied bundled directory.
  bool is_bundled = false;
};

}  // namespace eng
