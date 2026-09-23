#pragma once

/// @file agent-sound.h
/// @brief The sound tools: volumes, and the project's own sounds.
/// @par Threading Main-thread-only.

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

/// `get_sound`: the user's volumes and the file they are kept in, each of
/// the game's sounds and the project file it plays, the project's sound
/// files, and what was wrong with the sounds table.
AgentResult runAgentGetSound(EditorShellState& state,
                             const nlohmann::json& params);

/// `set_volume`: any of the master, bus volumes and the mute, as the Sound
/// screen sets them. Refused, changing nothing, on a value out of range.
AgentResult runAgentSetVolume(EditorShellState& state,
                              const nlohmann::json& params);

/// `set_sound`: play one of the project's sound files in a slot, or the
/// built-in sound again.
AgentResult runAgentSetSound(EditorShellState& state,
                             const nlohmann::json& params);

/// `import_sound`: bring a sound file into the project, and optionally
/// play it in a slot.
AgentResult runAgentImportSound(EditorShellState& state,
                                const nlohmann::json& params);

/// `play_sound`: have the editor play a slot or a project file once.
AgentResult runAgentPlaySound(EditorShellState& state,
                              const nlohmann::json& params);

}  // namespace eng::editor
