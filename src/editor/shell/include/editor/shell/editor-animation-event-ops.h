#pragma once

/// @file editor-animation-event-ops.h
/// @brief What each clip and sheet plays, and the sound files it names.
/// @par Threading Main-thread-only (loading reads files).

#include <editor/shell/editor-animation-event-table.h>
#include <editor/shell/editor-clip-event-set.h>
#include <engine/animation/rig.h>
#include <engine/audio/audio-clip-bank.h>
#include <engine/audio/audio-clip-id.h>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// The events every clip of @p rig — the model @p asset_ref names — plays,
/// in clip order: the table's row for the clip when @p table has one, in
/// time order; otherwise a footstep wherever a foot comes down
/// (`animation::detectFootContacts`); otherwise none.
[[nodiscard]] std::vector<EditorClipEventSet>
resolveEditorClipEvents(const EditorAnimationEventTable& table,
                        std::string_view asset_ref, const animation::Rig& rig);

/// @p table's row for the sheet at @p sheet, or null.
[[nodiscard]] const EditorSheetEventEntry*
findEditorSheetEvents(const EditorAnimationEventTable& table,
                      std::string_view sheet);

/// Whether @p set has a footstep in it — which is what hands the timing of
/// a walker's steps from its stride to its clip.
[[nodiscard]] bool editorEventsStep(const EditorClipEventSet& set);

/// Whether @p sound names a file under the assets rather than `footstep`
/// or one of the game's sound slots.
[[nodiscard]] bool editorEventNamesFile(std::string_view sound);

/// Load every file @p table's events name, from under @p assets_dir, into
/// @p bank — each under its own path, prefixed `file:` — and say, one line
/// each, which could not be.
[[nodiscard]] std::vector<std::string>
loadEditorEventSounds(audio::AudioClipBank& bank,
                      const EditorAnimationEventTable& table,
                      const std::filesystem::path& assets_dir);

/// The clip @p sound plays in @p bank — a slot by its name, a file by the
/// name `loadEditorEventSounds` gave it — or nothing.
[[nodiscard]] std::optional<audio::AudioClipId>
findEditorEventClip(const audio::AudioClipBank& bank, std::string_view sound);

}  // namespace eng::editor
