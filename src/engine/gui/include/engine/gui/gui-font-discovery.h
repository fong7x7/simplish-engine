#pragma once

/// @file gui-font-discovery.h
/// @brief Scan bundled and system paths for fonts; dedupe by family.
/// See docs/technical-approaches/editor/font-settings.md §2.

#include <engine/gui/gui-font-list-entry.h>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace eng {

/// Standard OS font directories for the current platform.
/// @thread_safety Main thread only.
std::vector<std::filesystem::path> guiSystemFontDirectories();

/// Discover fonts: bundled_dir first, then system paths; family names deduped.
/// @thread_safety Main thread only.
std::vector<GuiFontListEntry>
discoverGuiFonts(const std::filesystem::path& bundled_dir);

/// Family names to prefer for UI text on this platform, best first.
///
/// Matched against `GuiFontListEntry::family`, which is the file stem. The
/// list is a preference, not a requirement: `selectGuiUiFont` falls through
/// to whatever it did find rather than rendering no text at all.
/// @thread_safety Thread-safe (returns a view of static data).
std::vector<std::string_view> guiPreferredUiFontFamilies();

/// Choose the font to draw UI text with.
///
/// A bundled font wins, so a project that ships its own face gets it on
/// every machine. Failing that, the first preferred family present, and
/// failing that the first font discovered at all. Returns nullopt only when
/// the machine has no usable font file anywhere on the search paths.
/// @thread_safety Main thread only.
std::optional<GuiFontListEntry>
selectGuiUiFont(const std::filesystem::path& bundled_dir);

}  // namespace eng
