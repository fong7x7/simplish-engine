#pragma once

/// @file editor-logic-library.h
/// @brief A project's game logic, loaded into the editor from its library.
/// @par Threading Main-thread-only.

#include <filesystem>
#include <string_view>
#include <game/logic/game-logic-factory.h>

namespace eng::editor {

/// One load of a project's game logic library: open while this lives,
/// closed — and its copy deleted — when it goes.
///
/// Held by `std::shared_ptr`, because two things need it open: the editor,
/// which offers it to the next playtest, and a running playtest, whose
/// instance of the logic is code inside it. A rebuild while playing loads
/// the new library beside the old one; the old one closes when the last
/// playtest using it stops.
/// @thread_safety Main-thread-only.
class EditorLogicLibrary {
public:
  /// A library already open at @p handle, exporting @p factory, loaded
  /// from @p path. `loadEditorLogicLibrary` is the way to make one.
  EditorLogicLibrary(void* handle, game::GameLogicFactory factory,
                     std::filesystem::path path);
  ~EditorLogicLibrary();

  EditorLogicLibrary(const EditorLogicLibrary&) = delete;
  EditorLogicLibrary& operator=(const EditorLogicLibrary&) = delete;
  EditorLogicLibrary(EditorLogicLibrary&&) = delete;
  EditorLogicLibrary& operator=(EditorLogicLibrary&&) = delete;

  /// What makes and unmakes an instance of the logic.
  [[nodiscard]] game::GameLogicFactory factory() const { return factory_; }

  /// The address of what the library exports as @p name, or null — for
  /// the exports besides the logic's own, such as its tests.
  [[nodiscard]] void* symbol(std::string_view name) const;

  /// The copy that was loaded.
  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
  /// The platform's handle on the open library.
  void* handle_;
  /// Its exports.
  game::GameLogicFactory factory_;
  /// The copy it was opened from, deleted on close.
  std::filesystem::path path_;
};

}  // namespace eng::editor
