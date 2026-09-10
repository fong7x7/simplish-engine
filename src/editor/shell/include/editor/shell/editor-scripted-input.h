#pragma once

/// @file editor-scripted-input.h
/// @brief Input queued for a playtest to run on in place of the keyboard.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <engine/sim/player-input.h>

namespace eng::editor {

/// One input held for a run of ticks — what the agent API's `send_input`
/// queues, so a playthrough can be scripted and replayed exactly rather
/// than steered by whoever is at the keyboard.
/// @thread_safety Main-thread-only.
struct EditorScriptedInput {
  /// What player 1 does on each of those ticks.
  sim::PlayerInput input{};
  /// How many ticks are still to run on it. Removed once this reaches zero.
  uint32_t ticks = 1;
};

}  // namespace eng::editor
