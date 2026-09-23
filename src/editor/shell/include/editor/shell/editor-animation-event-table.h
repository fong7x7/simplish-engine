#pragma once

/// @file editor-animation-event-table.h
/// @brief The sounds a project's animations make, read from and written to
/// their table.
/// @par Threading Main-thread-only (touches the filesystem).

#include <cstdint>
#include <editor/shell/editor-clip-event-entry.h>
#include <editor/shell/editor-sheet-event-entry.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// The schema every row of the animation events table declares.
inline constexpr std::string_view EDITOR_ANIMATION_EVENT_SCHEMA =
    "simplish/animation_events/1.0";

/// The animation events table: sounds at moments of models' clips and at
/// frames of sprite sheets, and a count of changes. Written by the editor —
/// an agent's `set_animation_events` changes it and bumps `revision`, and
/// the editor saves it — and by hand.
/// @thread_safety Main-thread-only.
struct EditorAnimationEventTable {
  /// Every clip the project gives events, in file order.
  std::vector<EditorClipEventEntry> clips{};
  /// Every sheet the project gives events, in file order.
  std::vector<EditorSheetEventEntry> sheets{};
  /// What was wrong with the file, or with a sound it names, one line each.
  std::vector<std::string> problems{};
  /// Bumped on every change; the editor saves when it moves on from the
  /// revision it last wrote.
  uint64_t revision = 0;
};

/// Where a project keeps it:
/// `<root>/content/data/animation-events.data.json`.
[[nodiscard]] std::filesystem::path
editorAnimationEventTablePath(const std::filesystem::path& root);

/// The events in the data table @p text (project-format §8.5). A row naming
/// neither a model's clip nor a sheet, or naming one an earlier row took,
/// is skipped and said in `problems`, and so is an event with no sound; an
/// event's time is held to zero or later and its gain to 0–4. A file that
/// is not this table gives nothing and one problem.
[[nodiscard]] EditorAnimationEventTable
parseEditorAnimationEventTable(std::string_view text);

/// The table under @p root; empty, with no problems, when the project has
/// none.
[[nodiscard]] EditorAnimationEventTable
loadEditorAnimationEventTable(const std::filesystem::path& root);

/// @p table as the data table `parseEditorAnimationEventTable` reads.
[[nodiscard]] std::string
writeEditorAnimationEventTable(const EditorAnimationEventTable& table);

/// Write @p table under @p root. False when it could not be written.
bool saveEditorAnimationEventTable(const std::filesystem::path& root,
                                   const EditorAnimationEventTable& table);

}  // namespace eng::editor
