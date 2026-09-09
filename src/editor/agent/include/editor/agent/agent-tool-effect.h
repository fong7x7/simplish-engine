#pragma once

/// @file agent-tool-effect.h
/// @brief What running a tool costs the editor.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// How far a tool reaches into the editor.
///
/// The manifest publishes this, so an agent can tell a question from an
/// edit before it asks, and a caller that only wants to look at a level can
/// hold itself to the reads. It is also what decides whether a tool's
/// result has to wait for the editor to act on it: only `HOST` tools do.
/// @thread_safety Immutable value type.
enum class AgentToolEffect : uint8_t {
  /// Reads state and changes nothing.
  READ,
  /// Changes the document, the selection, or the active tool. Recorded in
  /// the undo history wherever the editor itself would record it.
  EDIT,
  /// Needs the running editor to carry something out that state alone
  /// cannot — a camera move, a project open, a rescan. The tool leaves an
  /// `AgentHostRequest` behind for the editor to run on its next tick.
  HOST,
};

}  // namespace eng::editor
