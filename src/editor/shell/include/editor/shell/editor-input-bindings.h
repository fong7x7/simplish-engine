#pragma once

/// @file editor-input-bindings.h
/// @brief The user's own control scheme, kept in their application data.
/// @par Threading Main-thread only (reads and writes a file).

#include <engine/input/input-bindings.h>
#include <filesystem>

namespace eng::editor {

/// The control scheme in @p file, over `editorDefaultInputBindings()`.
///
/// Where the user remaps their keys and pad: the file is theirs, not the
/// project's, so a scheme follows the person across every project they
/// open. When it does not exist yet the defaults are written to it, so
/// there is a complete file to edit rather than a format to look up.
/// Entries the file gets wrong are logged and skipped; the rest still
/// apply. An empty @p file keeps nothing and reads nothing.
[[nodiscard]] input::InputBindings
loadEditorInputBindings(const std::filesystem::path& file);

}  // namespace eng::editor
