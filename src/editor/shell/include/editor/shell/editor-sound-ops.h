#pragma once

/// @file editor-sound-ops.h
/// @brief The game's sound slots, and the project's files in them.
/// @par Threading Main-thread-only (loading reads files).

#include <cstdint>
#include <editor/shell/editor-sound-load.h>
#include <editor/shell/editor-sound-table.h>
#include <engine/audio/audio-clip-bank.h>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// Every slot a project can give its own sound, in the order the Sound
/// screen lists them: today, each combat cue's (`combat.shot_fired` …).
[[nodiscard]] std::vector<std::string> editorSoundSlots();

/// What the Sound screen calls @p slot: "Shot fired", "Blast"; the slot
/// itself for one it does not know.
[[nodiscard]] std::string editorSoundSlotLabel(std::string_view slot);

/// The slot @p name names — as the bank does (`combat.blast`) or by the cue
/// alone (`blast`) — or nothing.
[[nodiscard]] std::optional<std::string>
findEditorSoundSlot(std::string_view name);

/// The file @p table plays in @p slot, relative to `assets/`; empty when
/// the slot plays its built-in sound.
[[nodiscard]] std::filesystem::path
editorAssignedSound(const EditorSoundTable& table, std::string_view slot);

/// Play @p file in @p slot from now on, or — for an empty @p file — the
/// built-in sound again. Does not bump the revision; the caller does, once
/// it has decided the change stands.
void assignEditorSound(EditorSoundTable& table, std::string_view slot,
                       const std::filesystem::path& file);

/// The choice @p steps along from @p current in the list the Sound screen
/// steps through — the built-in sound (empty), then each of @p files —
/// wrapping round. A @p current not in the list counts as the built-in.
[[nodiscard]] std::filesystem::path
editorStepSoundFile(const std::filesystem::path& current,
                    std::span<const std::filesystem::path> files, int steps);

/// Load the built-in sounds into @p bank at @p sample_rate, then each file
/// @p table names, from under @p assets_dir, over its slot's — keeping the
/// same clip ids, so sounds already handed out stay good.
[[nodiscard]] EditorSoundLoad
loadEditorSounds(audio::AudioClipBank& bank, const EditorSoundTable& table,
                 const std::filesystem::path& assets_dir, uint32_t sample_rate);

}  // namespace eng::editor
