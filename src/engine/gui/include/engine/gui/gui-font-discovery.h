#pragma once

/// @file gui-font-discovery.h
/// @brief Scan bundled and system paths for fonts; dedupe by family.
/// See docs/technical-approaches/editor/font-settings.md §2.

#include <engine/gui/gui-font-list-entry.h>
#include <filesystem>
#include <vector>

namespace eng {

/// Standard OS font directories for the current platform.
/// @thread_safety Main thread only.
std::vector<std::filesystem::path> guiSystemFontDirectories();

/// Discover fonts: bundled_dir first, then system paths; family names deduped.
/// @thread_safety Main thread only.
std::vector<GuiFontListEntry>
discoverGuiFonts(const std::filesystem::path& bundled_dir);

}  // namespace eng
